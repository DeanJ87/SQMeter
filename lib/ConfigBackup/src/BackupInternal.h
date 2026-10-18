#pragma once

// Helpers shared by BackupFile.cpp and BackupPreview.cpp.

#include "ConfigBackup.h"

#include <ArduinoJson.h>

#include <string>

namespace SQM::Backup::Detail
{
    // The secret settings, by index: their paths in the settings JSON...
    constexpr size_t SECRET_COUNT = 9;
    inline constexpr const char *SECRET_PATHS[SECRET_COUNT] = {
        "wifi.password",
        "mqtt.password",
        "ota.password",
        "auth.password",
        "ble.passkey",
        "alerts.pushover.userKey",
        "alerts.pushover.appToken",
        "alerts.ntfy.token",
        "alerts.webhook.authHeader",
    };
    constexpr size_t BLE_PASSKEY = 4;

    // ...and where each lives in Config (const or not, as `cfg` is).
    template <typename C> auto &secretRef(C &cfg, size_t index)
    {
        switch (index)
        {
        case 1:
            return cfg.mqtt.password;
        case 2:
            return cfg.ota.password;
        case 3:
            return cfg.auth.password;
        case 4:
            return cfg.ble.passkey;
        case 5:
            return cfg.alerts.pushoverUserKey;
        case 6:
            return cfg.alerts.pushoverAppToken;
        case 7:
            return cfg.alerts.ntfyToken;
        case 8:
            return cfg.alerts.webhookAuthHeader;
        default:
            return cfg.wifi.password;
        }
    }

    bool isSecretPath(const std::string &path);

    // The variant at a dotted path ("alerts.pushover.userKey"), creating
    // objects on the way when `create` is set; null when it isn't there.
    JsonVariant atPath(JsonObject root, const std::string &path, bool create);

    // Document capacity to parse `text` in place (zero-copy: strings stay in
    // `text`), plus `extra` bytes for values added afterwards.
    size_t inPlaceCapacity(const std::string &text, size_t extra);

    // Parses `text` in place; it must outlive `doc`. False on any error.
    bool parseInPlace(std::string &text, JsonDocument &doc);

    // JSON text of a value.
    std::string jsonText(JsonVariantConst value);
} // namespace SQM::Backup::Detail
