#include "WebServer.h"
#include "Logger.h"
#include "SharedSecrets.h"

// Passwords handed to a paired device (specs/037-share-secrets):
// GET /api/secrets. What is refused and what is sent is lib/ConfigModel
// (SharedSecrets), shared with the demo; this adds the login and HTTP.

namespace SQM
{
    namespace
    {
        constexpr const char *SECRETS_TAG = "Secrets";

        // Every answer this route builds: never cached by a browser or proxy.
        void sendNoStore(AsyncWebServerRequest *request, int status, const std::string &json, uint32_t retryAfterSeconds = 0)
        {
            AsyncWebServerResponse *response = request->beginResponse(status, "application/json", json.c_str());
            response->addHeader("Cache-Control", "no-store");
            if (retryAfterSeconds > 0)
                response->addHeader("Retry-After", String(retryAfterSeconds));
            request->send(response);
        }
    } // namespace

    void WebServer::setupSecretsRoutes()
    {
        server.on("/api/secrets", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetSecrets(request); });
    }

    void WebServer::handleGetSecrets(AsyncWebServerRequest *request)
    {
        const Config &cfg = getConfigCallback();
        const SharedSecrets::Refusal refusal = SharedSecrets::refusal(cfg);
        if (refusal != SharedSecrets::Refusal::None)
        {
            sendNoStore(request, SharedSecrets::refusalStatus(refusal), createErrorJson(SharedSecrets::refusalMessage(refusal)));
            return;
        }

        const uint32_t now = millis();
        const uint32_t retryAfter = secretsLoginLock.retryAfterSeconds(now);
        if (retryAfter > 0)
        {
            sendNoStore(request, 429, createErrorJson(SharedSecrets::TOO_MANY_FAILURES), retryAfter);
            return;
        }

        // A device password is set (refusal above), so this asks for a login.
        if (!requireAuth(request))
        {
            if (secretsLoginLock.failed(now))
                Logger::warn(
                    SECRETS_TAG,
                    "Password sharing locked for a minute after failed logins from %s",
                    request->client()->remoteIP().toString().c_str());
            return;
        }
        secretsLoginLock.succeeded();

        // Never log the values.
        Logger::info(SECRETS_TAG, "Shared passwords with %s", request->client()->remoteIP().toString().c_str());
        sendNoStore(request, 200, SharedSecrets::toJson(cfg));
    }
} // namespace SQM
