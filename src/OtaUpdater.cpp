#include "OtaUpdater.h"
#include "GithubRootCA.h"
#include "Logger.h"
#include "version.h"
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <Update.h>
#include <LittleFS.h>
#include <esp_partition.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace SQM
{
    namespace
    {
        constexpr const char *TAG = "OtaUpdater";
        constexpr const char *RELEASES_URL = "https://api.github.com/repos/DeanJ87/SQMeter/releases";
        constexpr size_t JSON_DOC_CAPACITY = 24576;
        constexpr uint32_t HTTP_TIMEOUT_MS = 15000;
        constexpr size_t OTA_TASK_STACK_WORDS = 8192;

        bool findAsset(JsonArrayConst assets, const char *prefix, std::string &url, size_t &size)
        {
            const size_t prefixLen = strlen(prefix);
            for (JsonObjectConst asset : assets)
            {
                const char *name = asset["name"] | "";
                if (strncmp(name, prefix, prefixLen) == 0 && strstr(name, ".bin") != nullptr)
                {
                    url = asset["browser_download_url"] | "";
                    size = asset["size"] | 0;
                    return !url.empty();
                }
            }
            return false;
        }

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

    std::vector<GithubRelease> parseGithubReleases(const std::string &json, const std::string &track)
    {
        std::vector<GithubRelease> results;

        DynamicJsonDocument doc(JSON_DOC_CAPACITY);
        DeserializationError err = deserializeJson(doc, json);
        if (err)
        {
            Logger::error(TAG, "Failed to parse releases JSON: %s", err.c_str());
            return results;
        }

        const bool wantPrerelease = (track == "beta");

        for (JsonObjectConst release : doc.as<JsonArrayConst>())
        {
            const bool prerelease = release["prerelease"] | false;
            if (prerelease != wantPrerelease)
                continue;

            GithubRelease entry;
            entry.tag = std::string(release["tag_name"] | "");
            entry.name = std::string(release["name"] | entry.tag.c_str());
            entry.prerelease = prerelease;
            entry.publishedAt = std::string(release["published_at"] | "");

            if (entry.tag.empty())
                continue;

            JsonArrayConst assets = release["assets"].as<JsonArrayConst>();
            const bool hasFirmware = findAsset(assets, "sqmeter-firmware-", entry.firmwareAssetUrl, entry.firmwareAssetSize);
            const bool hasFs = findAsset(assets, "sqmeter-littlefs-", entry.fsAssetUrl, entry.fsAssetSize);

            // Firmware and web UI must always ship as a matched pair - a
            // release missing either asset (e.g. still building) is not
            // offered as an update target, to prevent frontend/backend drift.
            if (!hasFirmware || !hasFs)
                continue;

            results.push_back(std::move(entry));
        }

        return results;
    }

    OtaUpdater::OtaUpdater(ProgressCallback onProgress, ErrorCallback onError, RestartCallback onRestart)
        : progressCb(std::move(onProgress)), errorCb(std::move(onError)), restartCb(std::move(onRestart))
    {
    }

    std::vector<GithubRelease> OtaUpdater::checkForUpdate(const std::string &track, std::string &error)
    {
        currentPhase = Phase::Checking;

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

        int httpCode = http.GET();
        if (httpCode != HTTP_CODE_OK)
        {
            error = "GitHub API request failed (HTTP " + std::to_string(httpCode) + ")";
            Logger::error(TAG, "%s", error.c_str());
            http.end();
            currentPhase = Phase::Error;
            return {};
        }

        std::string body = http.getString().c_str();
        http.end();

        currentPhase = Phase::Idle;
        return parseGithubReleases(body, track);
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
            Logger::error(TAG, "Update.end failed: %d", Update.getError());
            if (errorCb)
                errorCb("Firmware update finalization failed");
            return false;
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
        // Firmware first (0-50% of progress), then filesystem (50-100%).
        // Only reboot once both have succeeded, so the device never boots
        // with a firmware/web-UI version mismatch.
        if (!downloadAndFlashFirmware(release.firmwareAssetUrl, release.firmwareAssetSize, 0, 50))
        {
            currentPhase = Phase::Error;
            return;
        }

        if (!downloadAndFlashFilesystem(release.fsAssetUrl, release.fsAssetSize, 50, 99))
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
