#include "SharedSecrets.h"

#include <ArduinoJson.h>

namespace SQM::SharedSecrets
{
    bool devicePasswordSet(const Config &cfg)
    {
        return cfg.auth.enabled && !cfg.auth.password.empty();
    }

    Refusal refusal(const Config &cfg)
    {
        if (!cfg.sharing.secrets)
            return Refusal::SharingOff;
        if (!devicePasswordSet(cfg)) // dep: D-53
            return Refusal::NoDevicePassword;
        return Refusal::None;
    }

    const char *refusalMessage(Refusal refusal)
    {
        switch (refusal)
        {
        case Refusal::SharingOff:
            return "Password sharing is off in SQMeter";
        case Refusal::NoDevicePassword:
            return "Set a web password in SQMeter before sharing passwords";
        case Refusal::None:
            break;
        }
        return nullptr;
    }

    int refusalStatus(Refusal refusal)
    {
        return refusal == Refusal::None ? 200 : 403;
    }

    std::string toJson(const Config &cfg)
    {
        // Five objects, four values (copied in, with room to grow while copying).
        DynamicJsonDocument doc(
            512 + 2 * (cfg.mqtt.password.size() + cfg.alerts.ntfyToken.size() + cfg.alerts.pushoverUserKey.size() +
                       cfg.alerts.pushoverAppToken.size()));
        doc["mqtt"]["password"] = cfg.mqtt.password;
        JsonObject alerts = doc.createNestedObject("alerts");
        alerts["ntfy"]["token"] = cfg.alerts.ntfyToken;
        JsonObject pushover = alerts.createNestedObject("pushover");
        pushover["userKey"] = cfg.alerts.pushoverUserKey;
        pushover["appToken"] = cfg.alerts.pushoverAppToken;
        std::string json;
        serializeJson(doc, json);
        return json;
    }

    uint32_t LoginLock::retryAfterSeconds(uint32_t nowMs)
    {
        if (!locked)
            return 0;
        const uint32_t elapsed = nowMs - lockedAtMs;
        if (elapsed >= LOCK_MS)
        {
            locked = false; // lifted; forgotten, so a wrap of millis() can't bring it back
            return 0;
        }
        return (LOCK_MS - elapsed + 999) / 1000;
    }

    bool LoginLock::failed(uint32_t nowMs)
    {
        if (++failures < MAX_FAILURES)
            return false;
        failures = 0;
        locked = true;
        lockedAtMs = nowMs;
        return true;
    }
} // namespace SQM::SharedSecrets
