#include "OtaUpdater.h"
#include "GithubRootCA.h"
#include "Logger.h"
#include "TlsLock.h"
#include "version.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <LittleFS.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#ifndef SQM_ENABLE_BLE
#define SQM_ENABLE_BLE 0
#endif

namespace SQM
{
    namespace
    {
        constexpr const char *TAG = "OtaUpdater";
        // Only the newest few releases matter, and per_page keeps the
        // response (and parse time over TLS) small.
        constexpr const char *RELEASES_URL = "https://api.github.com/repos/DeanJ87/SQMeter/releases?per_page=8";
        // Filtered, the current releases (6 releases x 3 assets) take 3.2 KB;
        // 6 KB leaves room for 8 releases with the BLE assets added.
        constexpr size_t JSON_DOC_CAPACITY = 6144;

        constexpr uint32_t HTTP_TIMEOUT_MS = 15000;
        constexpr size_t OTA_TASK_STACK_WORDS = 8192;

        // Streams an HTTPS GET body to `onChunk`, reporting progress scaled
        // into [progressFrom, progressTo]. Shared by the firmware and
        // filesystem downloads so both go through identical retry/EOF logic.
        bool streamDownload(const std::string &url, size_t sizeHint, int progressFrom, int progressTo,
                             const std::function<bool(const uint8_t *, size_t)> &onChunk,
                             const OtaUpdater::ProgressCallback &progressCb,
                             size_t &written, std::string &error)
        {
            WiFiClientSecure client;
            client.setCACert(GITHUB_ROOT_CA_PEM);

            HTTPClient http;
            http.setTimeout(HTTP_TIMEOUT_MS);
            // GitHub release assets are served via a redirect to a CDN URL; follow it.
            http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
            if (!http.begin(client, url.c_str()))
            {
                error = "Failed to initialize download";
                return false;
            }
            http.addHeader("User-Agent", "SQMeter-ESP32");

            int httpCode = http.GET();
            if (httpCode != HTTP_CODE_OK)
            {
                error = "Download failed (HTTP " + std::to_string(httpCode) + ")";
                http.end();
                return false;
            }

            int contentLength = http.getSize();
            size_t expectedSize = contentLength > 0 ? static_cast<size_t>(contentLength) : sizeHint;

            WiFiClient *stream = http.getStreamPtr();
            uint8_t buf[1024];
            written = 0;
            int lastPercent = -1;

            while (http.connected() && (written < expectedSize || expectedSize == 0))
            {
                size_t available = stream->available();
                if (!available)
                {
                    if (!http.connected())
                        break;
                    delay(10);
                    continue;
                }

                size_t toRead = std::min(available, sizeof(buf));
                size_t readBytes = stream->readBytes(buf, toRead);
                if (readBytes == 0)
                    break;

                if (!onChunk(buf, readBytes))
                {
                    error = "Flash write failed";
                    http.end();
                    return false;
                }

                written += readBytes;
                if (expectedSize > 0 && progressCb)
                {
                    int percent = progressFrom + static_cast<int>(
                                                      (written * static_cast<uint64_t>(progressTo - progressFrom)) / expectedSize);
                    if (percent != lastPercent)
                    {
                        lastPercent = percent;
                        progressCb(percent);
                    }
                }
            }
            http.end();

            if (expectedSize > 0 && written != expectedSize)
            {
                error = "Download incomplete (" + std::to_string(written) + " of " + std::to_string(expectedSize) + " bytes)";
                return false;
            }

            return true;
        }
    }

    OtaUpdater::OtaUpdater(ProgressCallback onProgress, ErrorCallback onError, RestartCallback onRestart)
        : progressCb(std::move(onProgress)), errorCb(std::move(onError)), restartCb(std::move(onRestart))
    {
    }

    std::vector<GithubRelease> OtaUpdater::checkForUpdate(const std::string &track, std::string &error)
    {
        currentPhase = Phase::Checking;

        // Allocate the parse buffer before the TLS session takes its ~45 KB:
        // afterwards there may be no contiguous block left for it.
        DynamicJsonDocument doc(JSON_DOC_CAPACITY);
        StaticJsonDocument<384> filter;
        Releases::buildFilter(filter);
        if (doc.capacity() == 0)
        {
            error = "Not enough free memory to check for updates";
            Logger::error(TAG, "%s (free %u, largest block %u)", error.c_str(), ESP.getFreeHeap(), ESP.getMaxAllocHeap());
            currentPhase = Phase::Error;
            return {};
        }

        TlsLock::Guard tls(15000);
        if (!tls.ok())
        {
            error = "Busy sending alerts - try again in a moment";
            currentPhase = Phase::Idle;
            return {};
        }

        WiFiClientSecure client;
        client.setCACert(GITHUB_ROOT_CA_PEM);

        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT_MS);
        if (!http.begin(client, RELEASES_URL))
        {
            error = "Failed to initialize HTTPS client";
            currentPhase = Phase::Error;
            return {};
        }
        http.addHeader("User-Agent", "SQMeter-ESP32");
        http.addHeader("Accept", "application/vnd.github+json");
        // HTTP/1.0 means no chunked transfer encoding, so the JSON can be
        // parsed straight off the TLS stream.
        http.useHTTP10(true);

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK)
        {
            error = "GitHub API request failed (HTTP " + std::to_string(httpCode) + ")";
            Logger::error(TAG, "%s", error.c_str());
            http.end();
            currentPhase = Phase::Error;
            return {};
        }

        // Stream-parse with a filter instead of http.getString(): the full
        // releases body (tens of KB of release notes) used to be held twice,
        // on top of the TLS session, dropping free heap to ~10 KB.
        // ArduinoJson reads through Stream::timedRead(), whose timeout (1 s
        // by default) is separate from the socket timeout above; a slow TLS
        // read on weak WiFi would otherwise end the parse early.
        static_cast<Stream &>(client).setTimeout(HTTP_TIMEOUT_MS);
        DeserializationError err = deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
        http.end();
        if (err)
        {
            error = err == DeserializationError::NoMemory ? std::string("Release list too large to read")
                                                          : std::string("Couldn't read the GitHub releases list: ") + err.c_str();
            Logger::error(TAG, "%s", error.c_str());
            currentPhase = Phase::Error;
            return {};
        }
        if (doc.overflowed())
            Logger::warn(TAG, "Release list truncated - JSON_DOC_CAPACITY too small");

        currentPhase = Phase::Idle;
        return Releases::parse(doc, track, SQM_ENABLE_BLE != 0);
    }

    bool OtaUpdater::applyUpdate(const GithubRelease &release)
    {
        if (currentPhase == Phase::Downloading || currentPhase == Phase::Writing)
        {
            return false;
        }

        currentPhase = Phase::Downloading;

        struct TaskArgs
        {
            OtaUpdater *self;
            GithubRelease release;
        };
        auto *args = new TaskArgs{this, release};

        xTaskCreatePinnedToCore(
            [](void *arg)
            {
                auto *a = static_cast<TaskArgs *>(arg);
                a->self->runApply(a->release);
                delete a;
                vTaskDelete(nullptr);
            },
            "ota_gh_apply",
            OTA_TASK_STACK_WORDS,
            args,
            1,
            nullptr,
            1 // same core AsyncTCP runs on is fine - this task blocks on network I/O, not CPU
        );

        return true;
    }

    bool OtaUpdater::downloadAndFlashFirmware(const std::string &url, size_t expectedSize, int progressFrom, int progressTo)
    {
        if (!Update.begin(expectedSize > 0 ? expectedSize : UPDATE_SIZE_UNKNOWN))
        {
            Logger::error(TAG, "Update.begin failed: %d", Update.getError());
            if (errorCb)
                errorCb("Not enough space for firmware update");
            return false;
        }

        currentPhase = Phase::Writing;

        size_t written = 0;
        std::string error;
        bool ok = streamDownload(
            url, expectedSize, progressFrom, progressTo,
            [](const uint8_t *data, size_t len)
            { return Update.write(const_cast<uint8_t *>(data), len) == len; },
            progressCb, written, error);

        if (!ok)
        {
            Logger::error(TAG, "Firmware download/write failed: %s", error.c_str());
            if (errorCb)
                errorCb(error.c_str());
            Update.abort();
            return false;
        }

        if (!Update.end(true))
        {
            // Same as a manual upload: if only switching the boot partition
            // failed, retry it once (it re-verifies the written image).
            const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
            const bool activated = Update.getError() == UPDATE_ERROR_ACTIVATE && esp_ota_set_boot_partition(target) == ESP_OK;
            Logger::error(TAG, "Update.end failed: %d%s", Update.getError(), activated ? " (activated on retry)" : "");
            if (!activated)
            {
                if (errorCb)
                    errorCb("Firmware update finalization failed");
                return false;
            }
        }

        Logger::info(TAG, "Firmware flashed successfully (%u bytes)", static_cast<unsigned>(written));
        return true;
    }

    bool OtaUpdater::downloadAndFlashFilesystem(const std::string &url, size_t expectedSize, int progressFrom, int progressTo)
    {
        const esp_partition_t *fsPartition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);

        if (!fsPartition)
        {
            Logger::error(TAG, "Filesystem partition not found");
            if (errorCb)
                errorCb("Filesystem partition not found");
            return false;
        }

        if (expectedSize > fsPartition->size)
        {
            Logger::error(TAG, "Filesystem image too large (%u > %u)",
                           static_cast<unsigned>(expectedSize), static_cast<unsigned>(fsPartition->size));
            if (errorCb)
                errorCb("Filesystem image too large for partition");
            return false;
        }

        LittleFS.end();

        Logger::info(TAG, "Erasing filesystem partition...");
        if (esp_partition_erase_range(fsPartition, 0, fsPartition->size) != ESP_OK)
        {
            Logger::error(TAG, "Filesystem partition erase failed");
            if (errorCb)
                errorCb("Failed to erase filesystem partition");
            return false;
        }

        currentPhase = Phase::Writing;

        size_t writeOffset = 0;
        size_t written = 0;
        std::string error;
        bool ok = streamDownload(
            url, expectedSize, progressFrom, progressTo,
            [fsPartition, &writeOffset](const uint8_t *data, size_t len)
            {
                if (esp_partition_write(fsPartition, writeOffset, data, len) != ESP_OK)
                    return false;
                writeOffset += len;
                return true;
            },
            progressCb, written, error);

        if (!ok)
        {
            Logger::error(TAG, "Filesystem download/write failed: %s", error.c_str());
            if (errorCb)
                errorCb(error.c_str());
            return false;
        }

        Logger::info(TAG, "Filesystem flashed successfully (%u bytes)", static_cast<unsigned>(written));
        return true;
    }

    void OtaUpdater::runApply(GithubRelease release)
    {
        // Hold the TLS lock for the whole download so an alert send can't
        // grab the heap the download sessions need.
        TlsLock::Guard tls(60000);
        // Firmware first (0-50% of progress), then filesystem (50-100%).
        // Only reboot once both have succeeded, so the device never boots
        // with a firmware/web-UI version mismatch.
        // Filesystem first, firmware last. Update.end() flips the boot
        // partition immediately on success (esp_ota_set_boot_partition), so
        // that must be the very last thing that can fail - if it already
        // succeeded and the filesystem write failed afterwards, the device
        // would keep running old firmware but boot into new firmware on its
        // next (possibly unrelated, e.g. power-loss) reset, serving it
        // against a mismatched/old web UI with no way to retry until it's
        // back online. Flashing filesystem first means any failure before
        // the firmware write leaves the boot partition untouched.
        if (!downloadAndFlashFilesystem(release.fsAssetUrl, release.fsAssetSize, 0, 50))
        {
            currentPhase = Phase::Error;
            return;
        }

        if (!downloadAndFlashFirmware(release.firmwareAssetUrl, release.firmwareAssetSize, 50, 99))
        {
            currentPhase = Phase::Error;
            return;
        }

        Logger::info(TAG, "GitHub OTA update to %s successful, rebooting...", release.tag.c_str());
        currentPhase = Phase::Done;
        if (progressCb)
            progressCb(100);
        if (restartCb)
            restartCb();
    }

} // namespace SQM
