#pragma once

#include <ArduinoJson.h>
#include <cstddef>
#include <cstdint>
#include "LanguageLogic.h"
#include <string>

class AsyncWebServer;
class AsyncWebServerRequest;

namespace SQM
{
    class WebServer;

    // The one non-English language file the device keeps (specs/023-i18n):
    // downloaded from the GitHub release matching the firmware, or uploaded by
    // hand, checked, and stored in LittleFS for the web UI to load.
    class LanguagePack
    {
    public:
        enum class State
        {
            Idle,
            Downloading,
            Installed,
            Failed,
            Restoring,
        };

        // The web server supplies auth, the configured language and whether an
        // OTA update is running (direct calls: cheaper in flash than callbacks).
        explicit LanguagePack(WebServer &web);

        void registerRoutes(AsyncWebServer &server);

        // Call when the language setting changes: English deletes the file,
        // another language downloads it.
        void onLanguageChanged(const std::string &code);

        // Call from the loop: after a firmware or filesystem update, fetch the
        // file again once the device is online (FR-014).
        void loop();

        void writeStatus(JsonObject target) const;

    private:
        bool startDownload(const std::string &code, State whileRunning, std::string &error);
        void runDownload(const std::string &code);
        bool fetchChecksum(const std::string &asset, Language::Checksum &entry);
        bool fetchFile(const std::string &asset, const Language::Checksum &entry);
        void install(const std::string &code, const std::string &version, size_t size);
        void handleInstall(AsyncWebServerRequest *request);
        void handleUploadChunk(AsyncWebServerRequest *request, size_t index, const uint8_t *data, size_t len);
        void handleUploadDone(AsyncWebServerRequest *request);
        void sendStatus(AsyncWebServerRequest *request) const;
        bool installedMatches(const std::string &code) const;
        void fail(const std::string &message);
        void remove();
        std::string language() const;

        WebServer &web;
        volatile State state = State::Idle;
        std::string lastError;
        bool restoreChecked = false;
    };
} // namespace SQM
