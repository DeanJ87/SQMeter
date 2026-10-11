#include "LanguagePack.h"

#include "LanguageLogic.h"
#include "Logger.h"
#include "OtaUpdater.h"
#include "TlsLock.h"
#include "version.h"
#include "WebServer.h"

#include <WiFi.h>

#include <ESPAsyncWebServer.h>
#include <cstdio>
#include <cstring>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <mbedtls/sha256.h>

namespace SQM
{
    namespace
    {
        constexpr const char *TAG = "Language";
        // Served as /lang.json by the static file handler (it sends the .gz
        // with Content-Encoding: gzip).
        constexpr const char *FILE_PATH = "/lang.json.gz";
        constexpr const char *TEMP_PATH = "/lang.tmp";
        constexpr const char *META_PATH = "/lang.meta"; // "<lang> <version> <size>"
        // Present while a download runs; left behind if it never finished.
        constexpr const char *ATTEMPT_PATH = "/lang.try";
        // Bytes (ESP-IDF FreeRTOS counts stack in bytes). The TLS handshake,
        // the 1 KB read buffer and LittleFS writes overflowed 7168 on a real
        // device; the OTA and alert tasks run the same download code on 8192.
        constexpr size_t TASK_STACK_BYTES = 10240;
        constexpr size_t CHECKSUM_MAX = 128;
        constexpr uint32_t TLS_WAIT_MS = 15000;

        const char *stateName(LanguagePack::State state)
        {
            switch (state)
            {
            case LanguagePack::State::Downloading:
                return "downloading";
            case LanguagePack::State::Installed:
                return "installed";
            case LanguagePack::State::Failed:
                return "failed";
            case LanguagePack::State::Restoring:
                return "restoring";
            case LanguagePack::State::Idle:
            default:
                return "idle";
            }
        }

        size_t freeBytes()
        {
            const size_t total = LittleFS.totalBytes();
            const size_t used = LittleFS.usedBytes();
            return total > used ? total - used : 0;
        }

        std::string hex(const uint8_t *digest)
        {
            static const char *digits = "0123456789abcdef";
            std::string out;
            for (int i = 0; i < 32; ++i)
            {
                out += digits[digest[i] >> 4];
                out += digits[digest[i] & 0xf];
            }
            return out;
        }

        struct Meta
        {
            char lang[12] = "";
            char version[32] = "";
            unsigned size = 0;
        };

        bool readMeta(Meta &meta)
        {
            if (!LittleFS.exists(META_PATH))
                return false;
            File file = LittleFS.open(META_PATH, "r");
            if (!file)
                return false;
            char line[64] = "";
            line[file.readBytes(line, sizeof(line) - 1)] = '\0';
            file.close();
            return std::sscanf(line, "%11s %31s %u", meta.lang, meta.version, &meta.size) == 3 && LittleFS.exists(FILE_PATH);
        }

        void writeMeta(const std::string &code, const std::string &version, size_t size)
        {
            File file = LittleFS.open(META_PATH, "w");
            if (file)
            {
                file.printf("%s %s %u", code.c_str(), version.empty() ? "-" : version.c_str(), static_cast<unsigned>(size));
                file.close();
            }
        }

        void sendError(AsyncWebServerRequest *request, int code, const std::string &message)
        {
            StaticJsonDocument<512> doc;
            doc["error"] = message;
            std::string body;
            serializeJson(doc, body);
            request->send(code, "application/json", body.c_str());
        }

        // Hand upload state (one upload at a time).
        struct Upload
        {
            File file;
            size_t size = 0;
            std::string error;
            uint8_t head[2] = {0, 0};
        };
        Upload upload;
    } // namespace

    LanguagePack::LanguagePack(WebServer &web)
        : web(web)
    {
    }

    std::string LanguagePack::language() const
    {
        return web.getConfigCallback().language;
    }

    bool LanguagePack::installedMatches(const std::string &code) const
    {
        Meta meta;
        return readMeta(meta) && code == meta.lang && std::strcmp(FIRMWARE_VERSION, meta.version) == 0;
    }

    void LanguagePack::fail(const std::string &message)
    {
        lastError = message;
        state = State::Failed;
        Logger::warn(TAG, "%s", message.c_str());
    }

    void LanguagePack::remove()
    {
        LittleFS.remove(FILE_PATH);
        LittleFS.remove(META_PATH);
        LittleFS.remove(TEMP_PATH);
        LittleFS.remove(ATTEMPT_PATH);
    }

    bool LanguagePack::downloadRunning() const
    {
        return state == State::Downloading || state == State::Restoring;
    }

    void LanguagePack::onLanguageChanged(const std::string &code)
    {
        std::string error;
        switch (Language::changeAction(code, installedMatches(code), downloadRunning()))
        {
        case Language::ChangeAction::AfterRunning:
            changePending = true;
            return;
        case Language::ChangeAction::RemoveFile:
            remove();
            lastError.clear();
            state = State::Idle;
            return;
        case Language::ChangeAction::UseInstalled:
            state = State::Installed;
            return;
        case Language::ChangeAction::Download:
            if (!startDownload(code, State::Downloading, error))
                fail(error);
            return;
        }
    }

    void LanguagePack::loop()
    {
        if (changePending && !downloadRunning())
        {
            changePending = false;
            onLanguageChanged(language());
        }
        if (restoreChecked || WiFi.status() != WL_CONNECTED || (WiFi.getMode() & WIFI_AP) != 0)
            return;
        restoreChecked = true;
        const std::string code = language();
        const bool unfinished = LittleFS.exists(ATTEMPT_PATH);
        LittleFS.remove(ATTEMPT_PATH);
        switch (Language::bootAction(code, installedMatches(code), unfinished))
        {
        case Language::BootAction::Nothing:
            return;
        case Language::BootAction::UseInstalled:
            state = State::Installed;
            return;
        case Language::BootAction::WaitAfterCrash:
            return fail("The last language download didn't finish - choose the language again to retry");
        case Language::BootAction::Restore:
        {
            std::string error;
            if (!startDownload(code, State::Restoring, error))
                fail(error);
            return;
        }
        }
    }

    bool LanguagePack::startDownload(const std::string &code, State whileRunning, std::string &error)
    {
        if (downloadRunning())
        {
            error = "A language download is already running";
            return false;
        }
        const OtaUpdater::Phase ota = web.otaUpdater->phase();
        if (ota == OtaUpdater::Phase::Downloading || ota == OtaUpdater::Phase::Writing)
        {
            error = "A firmware update is running - try again after it";
            return false;
        }
        state = whileRunning;
        lastError.clear();
        struct Args
        {
            LanguagePack *self;
            std::string code;
        };
        auto *args = new Args{this, code};
        const BaseType_t created = xTaskCreatePinnedToCore(
            [](void *arg)
            {
                auto *a = static_cast<Args *>(arg);
                // Marks the attempt: if it never returns (a crash, power loss),
                // the next boot won't start it again by itself.
                File marker = LittleFS.open(ATTEMPT_PATH, "w");
                marker.close();
                a->self->runDownload(a->code);
                LittleFS.remove(ATTEMPT_PATH);
                delete a;
                vTaskDelete(nullptr);
            },
            "lang_dl",
            TASK_STACK_BYTES,
            args,
            1,
            nullptr,
            1);
        if (created != pdPASS)
        {
            delete args;
            state = State::Idle;
            error = "Not enough free memory - try again in a moment";
            return false;
        }
        return true;
    }

    bool LanguagePack::fetchChecksum(const std::string &asset, Language::Checksum &entry)
    {
        std::string body;
        size_t written = 0;
        std::string error;
        const bool ok = httpsDownload(
            Language::assetUrl(FIRMWARE_VERSION, Language::checksumName(asset)),
            0,
            [&body](const uint8_t *data, size_t len)
            {
                if (body.size() + len > CHECKSUM_MAX)
                    return false;
                body.append(reinterpret_cast<const char *>(data), len);
                return true;
            },
            written,
            error);
        if (!ok)
            fail("Couldn't download the language file for this firmware version");
        else if (!Language::parseChecksum(body, entry, error))
            fail(error);
        return ok && state != State::Failed;
    }

    // Streams the file to TEMP_PATH, hashing as it goes; true when the size
    // and SHA-256 match the checksum file.
    bool LanguagePack::fetchFile(const std::string &asset, const Language::Checksum &entry)
    {
        File file = LittleFS.open(TEMP_PATH, "w");
        if (!file)
            return false;
        mbedtls_sha256_context sha;
        mbedtls_sha256_init(&sha);
        mbedtls_sha256_starts(&sha, 0);
        size_t written = 0;
        std::string error;
        const bool ok = httpsDownload(
            Language::assetUrl(FIRMWARE_VERSION, asset),
            entry.size,
            [&](const uint8_t *data, size_t len)
            {
                mbedtls_sha256_update(&sha, data, len);
                return file.write(data, len) == len;
            },
            written,
            error);
        file.close();
        uint8_t digest[32];
        mbedtls_sha256_finish(&sha, digest);
        mbedtls_sha256_free(&sha);
        return ok && written == entry.size && hex(digest) == entry.sha256;
    }

    void LanguagePack::runDownload(const std::string &code)
    {
        TlsLock::Guard tls(TLS_WAIT_MS);
        if (!tls.ok())
            return fail("Busy sending alerts - try again in a moment");
        const std::string asset = Language::assetName(code);
        Language::Checksum entry;
        if (!fetchChecksum(asset, entry))
            return;
        if (entry.size + Language::FREE_SPACE_MARGIN > freeBytes())
            return fail(Language::fileCheckMessage(Language::FileCheck::NoSpace));
        if (!fetchFile(asset, entry))
        {
            LittleFS.remove(TEMP_PATH);
            return fail("The download was incomplete - try again");
        }
        install(code, FIRMWARE_VERSION, entry.size);
    }

    // All-or-nothing: the previous file stays until the new one is complete.
    void LanguagePack::install(const std::string &code, const std::string &version, size_t size)
    {
        LittleFS.remove(FILE_PATH);
        if (!LittleFS.rename(TEMP_PATH, FILE_PATH))
            return fail("Couldn't write the language file");
        writeMeta(code, version, size);
        lastError.clear();
        state = State::Installed;
        Logger::info(TAG, "Installed %s (%u bytes)", code.c_str(), static_cast<unsigned>(size));
    }

    void LanguagePack::writeStatus(JsonObject target) const
    {
        const std::string chosen = language();
        Meta meta;
        const bool stored = readMeta(meta);
        // A change still to apply, or a file just installed for the previous
        // choice, is not "installed": the web UI reloads its text on that.
        const bool stale = changePending || (state == State::Installed && (!stored || chosen != meta.lang));
        target["language"] = chosen;
        target["state"] = stale ? stateName(State::Downloading) : stateName(state);
        target["firmwareVersion"] = FIRMWARE_VERSION;
        if (stored)
        {
            JsonObject pack = target.createNestedObject("pack");
            pack["lang"] = meta.lang;
            pack["version"] = meta.version;
            pack["size"] = meta.size;
        }
        else
        {
            target["pack"] = nullptr;
        }
        if (!lastError.empty())
            target["error"] = lastError;
    }

    void LanguagePack::handleInstall(AsyncWebServerRequest *request)
    {
        if (!web.requireAuth(request))
            return;
        const std::string code = language();
        if (code == Language::ENGLISH)
            return sendError(request, 409, "English is built in");
        std::string error;
        if (!startDownload(code, State::Downloading, error))
            return sendError(request, 409, error);
        request->send(202, "application/json", "{\"started\":true}");
    }

    void LanguagePack::handleUploadChunk(AsyncWebServerRequest *request, size_t index, const uint8_t *data, size_t len)
    {
        if (index == 0)
        {
            upload = Upload{};
            upload.file = LittleFS.open(TEMP_PATH, "w");
            if (!web.requireAuth(request) || !upload.file)
                upload.error = "Couldn't write the language file";
        }
        if (!upload.error.empty())
            return;
        for (size_t i = 0; i < len && index + i < 2; ++i)
            upload.head[index + i] = data[i];
        upload.size += len;
        const size_t head = upload.size >= 2 ? 2 : upload.size;
        const Language::FileCheck check = Language::checkFile(upload.head, head, upload.size, freeBytes() + upload.size);
        if (head == 2 && check != Language::FileCheck::Ok)
            upload.error = Language::fileCheckMessage(check);
        else if (upload.file.write(data, len) != len)
            upload.error = "Couldn't write the language file";
    }

    void LanguagePack::handleUploadDone(AsyncWebServerRequest *request)
    {
        if (!web.requireAuth(request))
            return;
        if (upload.file)
            upload.file.close();
        // The browser checked the contents and says which language it is.
        const String code = request->hasParam("lang") ? request->getParam("lang")->value() : String();
        const String version = request->hasParam("version") ? request->getParam("version")->value() : String();
        if (upload.error.empty() && (!Language::isSupported(code.c_str()) || code == Language::ENGLISH))
            upload.error = "That isn't a SQMeter language file (.json.gz)";
        if (!upload.error.empty())
        {
            LittleFS.remove(TEMP_PATH);
            return sendError(request, 400, upload.error);
        }
        install(code.c_str(), version.c_str(), upload.size);
        sendStatus(request);
    }

    void LanguagePack::sendStatus(AsyncWebServerRequest *request) const
    {
        StaticJsonDocument<768> doc;
        writeStatus(doc.to<JsonObject>());
        std::string body;
        serializeJson(doc, body);
        request->send(200, "application/json", body.c_str());
    }

    void LanguagePack::registerRoutes(AsyncWebServer &server)
    {
        server.on("/api/i18n/install", HTTP_POST, [this](AsyncWebServerRequest *request) { handleInstall(request); });
        server.on(
            "/api/i18n/upload",
            HTTP_POST,
            [this](AsyncWebServerRequest *request) { handleUploadDone(request); },
            [this](AsyncWebServerRequest *request, String, size_t index, uint8_t *data, size_t len, bool)
            { handleUploadChunk(request, index, data, len); });
        // Registered last: this server matches routes by prefix.
        server.on("/api/i18n", HTTP_GET, [this](AsyncWebServerRequest *request) { sendStatus(request); });
    }
} // namespace SQM
