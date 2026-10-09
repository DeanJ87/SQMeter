#include "WebServer.h"
#include "WebServerShared.h"
#include "Logger.h"
#include "version.h"
#include <WiFi.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <AsyncJson.h>
#include <PubSubClient.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <cstring>
#include <nvs.h>
#include <nvs_flash.h>
#include <ctime>
#include "calculations/CloudDetection.h"
#include "sensors/RG15Sensor.h"
#include "AlpacaDiscovery.h"
#include "Ipv6Network.h"
#include "FirmwareImage.h"
#include "FirmwareMarker.h"
#include "DualStackClient.h"
#include "NetAddress.h"
#include "WiFiManager.h"
#include "HeapTrace.h"
#include "SunPosition.h"
#include "SafetyHistory.h"
#include <Preferences.h>

extern uint32_t bootCount;

// Firmware and web UI updates: uploads (/api/update) and GitHub releases (/api/updates).

namespace SQM
{
    using namespace WebShared;

    namespace
    {
        // POST /api/update/fs: the LittleFS image, written straight to the
        // partition (no magic byte to check).
        struct FsUpload
        {
            const esp_partition_t *partition = nullptr;
            size_t bytesWritten = 0;
            bool error = false;
            bool touched = false; // LittleFS unmounted and the partition erased
            String errorMessage = "";

            void fail(const char *message)
            {
                error = true;
                errorMessage = message;
            }
        };
        FsUpload fsUpload;

        // POST /api/update: set when the updater's own activation failed but
        // a second esp_ota_set_boot_partition() (which re-verifies the
        // image) succeeded.
        bool activatedOnRetry = false;

        // POST /api/update: the image's marker (spec 027 FR-020), and why it
        // was refused (empty when it wasn't).
        FirmwareImage::Scanner uploadScanner;
        std::string uploadRefusal;

        // Multipart framing around the image in an upload's body.
        constexpr size_t FORM_OVERHEAD = 4096;

        void fsUploadBegin(const String &filename, size_t requestBytes)
        {
            Logger::info("OTA", "Filesystem update started: %s", filename.c_str());
            fsUpload = FsUpload{};

            // Find the LittleFS partition (labeled as "spiffs" in partition table)
            fsUpload.partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
            if (!fsUpload.partition)
            {
                Logger::error("OTA", "Filesystem partition not found!");
                fsUpload.fail("Filesystem partition not found");
                return;
            }
            // An image for another layout (e.g. a 512 KB one from v0.2) would
            // be cut off or leave a filesystem that doesn't mount: refuse it
            // before anything is erased.
            if (!FirmwareImage::fsImageFits(requestBytes, fsUpload.partition->size, FORM_OVERHEAD))
            {
                Logger::error(
                    "OTA", "Filesystem image is the wrong size for this device (%u bytes sent)", static_cast<unsigned>(requestBytes));
                fsUpload.fail("Web UI file is for a different device layout. Use this release's littlefs file.");
                return;
            }
            Logger::info(
                "OTA",
                "Found filesystem partition at 0x%x, size %u bytes",
                fsUpload.partition->address,
                static_cast<unsigned>(fsUpload.partition->size));

            // Unmount LittleFS before writing
            fsUpload.touched = true;
            LittleFS.end();

            Logger::info("OTA", "Erasing filesystem partition...");
            esp_err_t err = esp_partition_erase_range(fsUpload.partition, 0, fsUpload.partition->size);
            if (err != ESP_OK)
            {
                Logger::error("OTA", "Partition erase failed: %d", err);
                fsUpload.fail("Failed to erase partition");
                return;
            }
            Logger::info("OTA", "Partition erased successfully");
        }

        // False when this write failed (the upload is then abandoned).
        bool fsUploadWrite(size_t index, const uint8_t *data, size_t len)
        {
            if (fsUpload.error || !fsUpload.partition)
                return true;
            esp_err_t err = esp_partition_write(fsUpload.partition, fsUpload.bytesWritten, data, len);
            if (err != ESP_OK)
            {
                Logger::error("OTA", "Partition write failed at offset %u: %d", static_cast<unsigned>(fsUpload.bytesWritten), err);
                fsUpload.fail("Failed to write to partition");
                return false;
            }
            fsUpload.bytesWritten += len;
            if (index % 10240 == 0) // Log every ~10KB
                Logger::info("OTA", "Written %u bytes", static_cast<unsigned>(fsUpload.bytesWritten));
            return true;
        }

        struct UploadChunk
        {
            size_t index;
            uint8_t *data;
            size_t len;
            bool final;
        };

        void fsUploadChunk(const String &filename, size_t requestBytes, const UploadChunk &chunk)
        {
            if (!chunk.index)
            {
                fsUploadBegin(filename, requestBytes);
                if (fsUpload.error)
                    return;
            }
            if (!fsUploadWrite(chunk.index, chunk.data, chunk.len) || !chunk.final)
                return;
            if (!fsUpload.error)
                Logger::info("OTA", "Filesystem update success: %u bytes written", static_cast<unsigned>(fsUpload.bytesWritten));
            else
                Logger::error("OTA", "Filesystem update failed: %s", fsUpload.errorMessage.c_str());
        }

        const char *updateErrorText(uint8_t error)
        {
            switch (error)
            {
            case UPDATE_ERROR_OK:
                return "No error";
            case UPDATE_ERROR_WRITE:
                return "Flash write failed";
            case UPDATE_ERROR_ERASE:
                return "Flash erase failed";
            case UPDATE_ERROR_READ:
                return "Flash read failed";
            case UPDATE_ERROR_SPACE:
                return "Not enough space";
            case UPDATE_ERROR_SIZE:
                return "Bad size given";
            case UPDATE_ERROR_STREAM:
                return "Stream read timeout";
            case UPDATE_ERROR_MD5:
                return "MD5 check failed";
            case UPDATE_ERROR_MAGIC_BYTE:
                return "Wrong magic byte";
            case UPDATE_ERROR_ACTIVATE:
                return "Could not activate partition";
            case UPDATE_ERROR_NO_PARTITION:
                return "Partition not found";
            case UPDATE_ERROR_BAD_ARGUMENT:
                return "Bad argument";
            case UPDATE_ERROR_ABORT:
                return "Update aborted";
            default:
                return nullptr;
            }
        }

        // The image is fully written and its first block restored; only
        // switching the boot partition failed. Uploads used to fail like this
        // on the first attempt and succeed on a retry, so retry the switch
        // here. It verifies the image again, so a bad image still can't be
        // booted.
        void retryActivation()
        {
            const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
            const esp_err_t first = esp_ota_set_boot_partition(target);
            Logger::warn("OTA", "Activating %s failed; retry: %s", target ? target->label : "?", esp_err_to_name(first));
            activatedOnRetry = first == ESP_OK;
            if (activatedOnRetry)
                Logger::info("OTA", "Firmware update success on retry, rebooting...");
        }

        void firmwareUploadEnd()
        {
            const FirmwareImage::Verdict verdict = FirmwareImage::check(uploadScanner, FirmwareMarker::layout(), FirmwareMarker::build());
            if (verdict != FirmwareImage::Verdict::Ok)
            {
                // Before Update.end(): the boot partition is never switched.
                uploadRefusal = FirmwareImage::verdictMessage(verdict, FirmwareMarker::build());
                Logger::error("OTA", "Firmware refused: %s", uploadRefusal.c_str());
                Update.abort();
                return;
            }
            if (Update.end(true))
            {
                Logger::info("OTA", "Firmware update success, rebooting...");
            }
            else if (Update.getError() == UPDATE_ERROR_ACTIVATE)
            {
                retryActivation();
            }
            else
            {
                Logger::error("OTA", "Update.end failed: %d", Update.getError());
                Update.printError(Serial);
            }
        }

        void firmwareUploadChunk(const String &filename, size_t index, uint8_t *data, size_t len, bool final)
        {
            if (!index)
            {
                Logger::info("OTA", "Firmware update started: %s", filename.c_str());
                activatedOnRetry = false;
                uploadScanner = FirmwareImage::Scanner();
                uploadRefusal.clear();
                if (Update.isRunning())
                {
                    // An earlier upload was cut off; start clean.
                    Logger::warn("OTA", "Aborting an unfinished update");
                    Update.abort();
                }
                if (!Update.begin(UPDATE_SIZE_UNKNOWN))
                {
                    Logger::error("OTA", "Update.begin failed: %d", Update.getError());
                    Update.printError(Serial);
                }
            }
            uploadScanner.feed(data, len);
            if (Update.write(data, len) != len)
            {
                Logger::error("OTA", "Update.write failed: %d", Update.getError());
                Update.printError(Serial);
            }
            if (final)
                firmwareUploadEnd();
        }

        void sendClosing(AsyncWebServerRequest *request, int status, const String &json)
        {
            AsyncWebServerResponse *response = request->beginResponse(status, "application/json", json);
            response->addHeader("Connection", "close");
            request->send(response);
        }
    } // namespace

    void WebServer::setupOTA()
    {
        // Filesystem OTA update (LittleFS partition).
        server.on(
            "/api/update/fs",
            HTTP_POST,
            [this](AsyncWebServerRequest *request) { handleFsUploadDone(request); },
            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
            { fsUploadChunk(filename, request->contentLength(), UploadChunk{index, data, len, final}); });

        // Firmware OTA update (app partition). Registered after /api/update/fs:
        // this server also matches "/api/update" as a prefix of
        // "/api/update/fs", so registered first it took filesystem uploads too.
        server.on(
            "/api/update",
            HTTP_POST,
            [this](AsyncWebServerRequest *request) { handleFirmwareUploadDone(request); },
            [](AsyncWebServerRequest *, String filename, size_t index, uint8_t *data, size_t len, bool final)
            { firmwareUploadChunk(filename, index, data, len, final); });
    }

    void WebServer::handleFsUploadDone(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;
        const bool fsFailed = fsUpload.error;
        // A refused file (wrong size) never touched the filesystem: keep running.
        const bool restart = !fsFailed || fsUpload.touched;
        const String responseJson =
            fsFailed ? String(createErrorJson(fsUpload.errorMessage.c_str()).c_str()) : String("{\"success\":true}");
        fsUpload = FsUpload{}; // reset for the next upload
        sendClosing(request, fsFailed ? (restart ? 500 : 400) : 200, responseJson);
        if (restart)
            WebServer::scheduleRestart(1000);
    }

    void WebServer::handleFirmwareUploadDone(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;
        if (!uploadRefusal.empty())
        {
            sendClosing(request, 400, createErrorJson(uploadRefusal.c_str()).c_str());
            uploadRefusal.clear();
            return;
        }
        const bool success = !Update.hasError() || activatedOnRetry;
        String responseJson;
        if (success)
        {
            // `retried`: the boot switch only took on the second try (see
            // retryActivation), so the success rate of the first attempt can
            // be measured (spec 012 SC-001).
            responseJson = activatedOnRetry ? "{\"success\":true,\"retried\":true}" : "{\"success\":true}";
        }
        else
        {
            const uint8_t error = Update.getError();
            const char *text = updateErrorText(error);
            const String message = text != nullptr ? String(text) : "Error code: " + String(error);
            responseJson = createErrorJson(message.c_str()).c_str();
        }
        sendClosing(request, success ? 200 : 500, responseJson);
        if (success)
            WebServer::scheduleRestart(1000);
    }

    void WebServer::setupGithubUpdates()
    {
        // Check for available releases on the given track (?track=stable|beta,
        // defaults to stable). Returns the filtered release list; staleness
        // relative to the running firmware is computed client-side.
        server.on("/api/updates/check", HTTP_GET, [this](AsyncWebServerRequest *request) { handleUpdatesCheck(request); });

        // Starts a self-download+flash of the given release's firmware AND
        // filesystem assets as one atomic update (never just one, to avoid
        // frontend/backend drift). Body: {"firmwareAssetUrl", "firmwareAssetSize",
        // "fsAssetUrl", "fsAssetSize"}. Progress/errors are pushed over
        // /ws/status ("ota_progress" messages), same channel the manual
        // upload OTA flow already uses.
        AsyncCallbackJsonWebHandler *applyHandler = new AsyncCallbackJsonWebHandler(
            "/api/updates/apply", [this](AsyncWebServerRequest *request, JsonVariant &json) { handleUpdatesApply(request, json); });
        applyHandler->setMethod(HTTP_POST);
        server.addHandler(applyHandler);
    }

    void WebServer::handleUpdatesCheck(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;

        std::string track = "stable";
        if (request->hasParam("track") && request->getParam("track")->value() == "beta")
            track = "beta";

        // The check takes seconds over TLS (longer on weak WiFi), so it runs in
        // its own task and answers the paused request when done: blocking the
        // web server's task that long would trip its watchdog.
        bool idle = false;
        if (!updatesCheckRunning.compare_exchange_strong(idle, true))
        {
            request->send(409, "application/json", createErrorJson("Already checking for updates").c_str());
            return;
        }
        auto *job = new UpdatesCheckJob{request->pause(), otaUpdater.get(), track};
        if (xTaskCreate(runUpdatesCheck, "update_check", UPDATES_CHECK_STACK_BYTES, job, 1, nullptr) != pdPASS)
        {
            const AsyncWebServerRequestPtr paused = job->request;
            delete job;
            updatesCheckRunning = false;
            if (std::shared_ptr<AsyncWebServerRequest> again = paused.lock())
                again->send(503, "application/json", createErrorJson("Not enough free memory to check for updates").c_str());
        }
    }

    void WebServer::runUpdatesCheck(void *arg)
    {
        std::unique_ptr<UpdatesCheckJob> job(static_cast<UpdatesCheckJob *>(arg));
        std::string error;
        const std::vector<GithubRelease> releases = job->ota->checkForUpdate(job->track, error);
        const std::string body = error.empty() ? releasesJson(releases) : createErrorJson(error.c_str());
        if (std::shared_ptr<AsyncWebServerRequest> request = job->request.lock())
            request->send(error.empty() ? 200 : 502, "application/json", body.c_str());
        job.reset();
        updatesCheckRunning = false;
        vTaskDelete(nullptr);
    }

    std::string WebServer::releasesJson(const std::vector<GithubRelease> &releases)
    {
        DynamicJsonDocument doc(8192);
        JsonArray arr = doc.to<JsonArray>();
        for (const GithubRelease &r : releases)
        {
            JsonObject o = arr.createNestedObject();
            o["tag"] = r.tag;
            o["name"] = r.name;
            o["prerelease"] = r.prerelease;
            o["publishedAt"] = r.publishedAt;
            o["firmwareAssetUrl"] = r.firmwareAssetUrl;
            o["firmwareAssetSize"] = r.firmwareAssetSize;
            o["fsAssetUrl"] = r.fsAssetUrl;
            o["fsAssetSize"] = r.fsAssetSize;
        }
        std::string json;
        serializeJson(doc, json);
        return json;
    }

    void WebServer::handleUpdatesApply(AsyncWebServerRequest *request, JsonVariant &json)
    {
        if (!requireAuth(request))
            return;

        JsonObject body = json.as<JsonObject>();
        GithubRelease release;
        release.firmwareAssetUrl = std::string(body["firmwareAssetUrl"] | "");
        release.firmwareAssetSize = body["firmwareAssetSize"] | 0;
        release.fsAssetUrl = std::string(body["fsAssetUrl"] | "");
        release.fsAssetSize = body["fsAssetSize"] | 0;

        if (release.firmwareAssetUrl.empty() || release.fsAssetUrl.empty())
        {
            request->send(400, "application/json", createErrorJson("firmwareAssetUrl and fsAssetUrl are required").c_str());
            return;
        }
        if (!otaUpdater->applyUpdate(release))
        {
            request->send(409, "application/json", createErrorJson("Update already in progress").c_str());
            return;
        }
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Update started\"}");
    }

    void WebServer::setOTAProgress(int progress)
    {
        StaticJsonDocument<128> doc;
        doc["type"] = "ota_progress";
        doc["progress"] = progress;

        std::string json;
        serializeJson(doc, json);
        // Send OTA progress to status WebSocket (System page handles OTA)
        wsStatus.textAll(json.c_str());
    }

    void WebServer::setOTAError(const char *error)
    {
        // Send OTA errors to status WebSocket (System page handles OTA)
        wsStatus.textAll(createErrorJson(error).c_str());
    }

} // namespace SQM
