#pragma once

#include <cstdint>
#include <string>

#include "Config.h"

// Passwords handed to a paired device (specs/037-share-secrets): GET /api/secrets.
// Only the MQTT password, the ntfy token and the Pushover keys, only while the
// owner has turned sharing on and a device password is set, and only to a
// correct login. The firmware adds the login check; this decides the rest.

namespace SQM::SharedSecrets
{
    enum class Refusal : uint8_t
    {
        None,             // go on to the login check
        SharingOff,       // sharing.secrets is off
        NoDevicePassword, // password protection off, or no password
    };

    // Password protection is on with a password: requireAuth asks for a login.
    bool devicePasswordSet(const Config &cfg);

    // Why the request is refused before the login is checked, if it is.
    Refusal refusal(const Config &cfg);

    // The {"error": ...} text for a refusal; nullptr for None.
    const char *refusalMessage(Refusal refusal);

    // HTTP status for a refusal (403); 200 for None.
    int refusalStatus(Refusal refusal);

    // {"mqtt":{"password"},"alerts":{"ntfy":{"token"},"pushover":{"userKey","appToken"}}},
    // each value as stored ("" when unset). Never anything else.
    std::string toJson(const Config &cfg);

    static constexpr const char *TOO_MANY_FAILURES = "Too many failed logins; try again in a minute";

    // Failed logins on this request: five in a row lock it for a minute. One
    // lock for the device, in RAM. Times are millis(); unsigned differences
    // keep it right across the 49-day wrap.
    class LoginLock
    {
    public:
        static constexpr uint8_t MAX_FAILURES = 5;
        static constexpr uint32_t LOCK_MS = 60000;

        // Seconds until the lock lifts (at least 1), or 0 when not locked.
        // Call it on every request: a lifted lock is forgotten here.
        uint32_t retryAfterSeconds(uint32_t nowMs);
        // A missing or wrong login. True when this one starts the lock.
        bool failed(uint32_t nowMs);
        void succeeded() { failures = 0; }

    private:
        uint8_t failures = 0;
        bool locked = false;
        uint32_t lockedAtMs = 0;
    };
} // namespace SQM::SharedSecrets
