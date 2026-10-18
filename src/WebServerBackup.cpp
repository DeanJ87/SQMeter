#include "WebServer.h"
#include "ConfigBackup.h"
#include "Logger.h"
#include "version.h"

#include <ArduinoJson.h>
#include <esp_random.h>

#include <cstdlib>
#include <cstring>
#include <ctime>

// Settings backup and restore (specs/033-config-backup): POST /api/backup,
// /api/backup/preview and /api/backup/apply. The file format, the crypto and
// the preview are lib/ConfigBackup; this collects request bodies, runs the
// slow steps (key derivation) in a task and keeps the previewed config until
// it is applied.

namespace SQM
{
    namespace
    {
        constexpr uint32_t BACKUP_JOB_STACK_BYTES = 8192;
        constexpr BaseType_t BACKUP_JOB_CORE = 1; // beside the loop task: core 0's idle task stays fed (research R3)
        constexpr uint32_t PREVIEW_TTL_MS = 10UL * 60UL * 1000UL;
        constexpr uint32_t RESTORE_RESTART_DELAY_MS = 2000;

        int statusFor(Backup::Error error)
        {
            if (error == Backup::Error::TooLarge)
                return 413;
            return error == Backup::Error::Busy || error == Backup::Error::Stale ? 409 : 400;
        }

        // Request bodies are collected whole (up to MAX_BODY_BYTES) and parsed once, by the library.
        void collectBody(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total)
        {
            if (total > Backup::MAX_BODY_BYTES)
                return;
            if (index == 0 && request->_tempObject == nullptr)
                request->_tempObject = calloc(total + 1, 1);
            if (request->_tempObject != nullptr && index + len <= total)
                std::memcpy(static_cast<uint8_t *>(request->_tempObject) + index, data, len);
        }

        bool takeBody(AsyncWebServerRequest *request, std::string &body)
        {
            if (request->contentLength() > Backup::MAX_BODY_BYTES)
            {
                request->send(413, "application/json", Backup::errorJson(Backup::Error::TooLarge).c_str());
                return false;
            }
            if (request->_tempObject != nullptr)
            {
                body.assign(static_cast<const char *>(request->_tempObject), request->contentLength());
                free(request->_tempObject);
                request->_tempObject = nullptr;
            }
            return true;
        }

        std::string createdAtNow()
        {
            const time_t now = time(nullptr);
            return now >= Core::CLOCK_VALID_EPOCH ? Core::isoUtc(static_cast<int64_t>(now)) : std::string();
        }
    } // namespace

    // One backup or preview, run by runBackupJob on its own task.
    struct WebServer::BackupJob
    {
        AsyncWebServerRequestPtr request;
        WebServer *owner;
        bool preview;
        std::string body;
        Config config; // the settings when the request came in
        uint32_t revision;
    };

    void WebServer::setupBackupRoutes()
    {
        // The longer paths first: handlers match by prefix.
        server.on(
            "/api/backup/preview",
            HTTP_POST,
            [this](AsyncWebServerRequest *request) { handleBackupStart(request, true); },
            nullptr,
            collectBody);
        server.on(
            "/api/backup/apply", HTTP_POST, [this](AsyncWebServerRequest *request) { handleBackupApply(request); }, nullptr, collectBody);
        server.on(
            "/api/backup", HTTP_POST, [this](AsyncWebServerRequest *request) { handleBackupStart(request, false); }, nullptr, collectBody);
    }

    Backup::DeviceInfo WebServer::backupDevice()
    {
        Backup::DeviceInfo device;
        device.firmware = FIRMWARE_VERSION;
        device.board = CONFIG_IDF_TARGET;
        device.bluetoothBuild = BleService::available();
        device.pins = Backup::classicEsp32Pins(); // until board profiles exist (spec 033 assumptions)
        return device;
    }

    void WebServer::handleBackupStart(AsyncWebServerRequest *request, bool preview)
    {
        if (!requireAuth(request))
            return;
        std::string body;
        if (!takeBody(request, body))
            return;
        bool idle = false;
        if (!backupJobRunning.compare_exchange_strong(idle, true))
        {
            request->send(409, "application/json", Backup::errorJson(Backup::Error::Busy).c_str());
            return;
        }
        auto *job = new BackupJob{request->pause(), this, preview, std::move(body), getConfigCallback(), configRevision.load()};
        if (xTaskCreatePinnedToCore(runBackupJob, "backup_job", BACKUP_JOB_STACK_BYTES, job, 1, nullptr, BACKUP_JOB_CORE) != pdPASS)
        {
            const AsyncWebServerRequestPtr paused = job->request;
            delete job;
            backupJobRunning = false;
            if (std::shared_ptr<AsyncWebServerRequest> again = paused.lock())
                again->send(503, "application/json", createErrorJson("Not enough free memory for a backup").c_str());
        }
    }

    void WebServer::runBackupJob(void *arg)
    {
        std::unique_ptr<BackupJob> job(static_cast<BackupJob *>(arg));
        int status = 200;
        const std::string body = job->preview ? job->owner->previewRestore(*job, status) : makeBackupFile(*job, status);
        if (std::shared_ptr<AsyncWebServerRequest> request = job->request.lock())
            request->send(status, "application/json", body.c_str());
        job.reset();
        backupJobRunning = false;
        vTaskDelete(nullptr);
    }

    std::string WebServer::makeBackupFile(BackupJob &job, int &status)
    {
        Backup::SealParams seal;
        const Backup::Error error = Backup::readBackupRequest(job.body, seal.passphrase);
        job.body.clear();
        if (error != Backup::Error::None)
        {
            status = statusFor(error);
            return Backup::errorJson(error);
        }
        esp_fill_random(seal.salt, sizeof(seal.salt));
        esp_fill_random(seal.nonce, sizeof(seal.nonce));
        const bool encrypted = !seal.passphrase.empty();
        const std::string createdAt = createdAtNow();
        const uint32_t startedAt = millis();
        std::string file = Backup::makeBackup(job.config, backupDevice(), createdAt, encrypted ? &seal : nullptr);
        seal.passphrase.assign(seal.passphrase.size(), '\0');
        if (encrypted)
            Logger::info(TAG, "Backup key derived in %u ms", static_cast<unsigned>(millis() - startedAt));
        if (file.empty())
        {
            status = 500;
            return createErrorJson("The backup couldn't be made");
        }
        return file;
    }

    std::string WebServer::previewRestore(BackupJob &job, int &status)
    {
        Backup::Parsed parsed;
        std::string passphrase;
        Backup::Error error = Backup::readPreviewRequest(job.body, parsed, passphrase);
        job.body.clear();
        Backup::Secrets secrets;
        const bool restoring = error == Backup::Error::None && parsed.hasSecrets && !passphrase.empty();
        if (restoring)
        {
            const uint32_t startedAt = millis();
            error = Backup::openSecrets(parsed, passphrase, secrets);
            Logger::info(TAG, "Backup key derived in %u ms", static_cast<unsigned>(millis() - startedAt));
        }
        passphrase.assign(passphrase.size(), '\0');
        // Built in place in the staged restore: the candidate isn't copied again.
        std::unique_ptr<StagedRestore> staged(new StagedRestore(job.config));
        std::string reason;
        if (error == Backup::Error::None)
        {
            const Backup::PreviewInput input{&job.config, &parsed, restoring ? &secrets : nullptr, backupDevice()};
            error = Backup::buildPreview(input, staged->preview, &reason);
        }
        if (error != Backup::Error::None)
        {
            status = statusFor(error);
            return Backup::errorJson(error, reason);
        }
        uint8_t random[Backup::TOKEN_BYTES];
        esp_fill_random(random, sizeof(random));
        staged->token = Backup::toHex(random, sizeof(random));
        staged->revision = job.revision;
        staged->stagedAtMs = millis();
        std::string document = Backup::previewJson(staged->preview, parsed, staged->token);
        std::lock_guard<std::mutex> lock(stagedRestoreLock);
        stagedRestore = std::move(staged); // replaces any earlier preview
        return document;
    }

    void WebServer::handleBackupApply(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;
        std::string body;
        if (!takeBody(request, body))
            return;
        const std::string token = Backup::readApplyRequest(body);
        std::unique_ptr<StagedRestore> staged;
        {
            std::lock_guard<std::mutex> lock(stagedRestoreLock);
            if (stagedRestore && millis() - stagedRestore->stagedAtMs > PREVIEW_TTL_MS)
                stagedRestore.reset();
            if (stagedRestore && !token.empty() && stagedRestore->token == token)
                staged = std::move(stagedRestore);
        }
        // What is applied is exactly what was previewed, against the settings it was previewed on (FR-013).
        if (!staged || staged->revision != configRevision.load())
        {
            request->send(409, "application/json", Backup::errorJson(Backup::Error::Stale).c_str());
            return;
        }
        if (!saveAndApply(staged->preview.candidate))
        {
            request->send(500, "application/json", createErrorJson("Failed to save configuration").c_str());
            return;
        }
        const bool restarting = staged->preview.restart != 0;
        Logger::info(TAG, "Settings restored from a backup%s", restarting ? "; restarting" : "");
        request->send(200, "application/json", restarting ? "{\"success\":true,\"restarting\":true}" : "{\"success\":true,\"restarting\":false}");
        if (restarting)
            scheduleRestart(RESTORE_RESTART_DELAY_MS);
    }
} // namespace SQM
