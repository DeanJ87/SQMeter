#include "Config.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <nvs.h>
#include <cstring>

// Persisting the config in NVS. The model (defaults, JSON, validation) is in
// lib/ConfigModel so the native tests and the browser demo share it.

namespace SQM
{
    static const char *NVS_NAMESPACE = "sqm";
    static const char *NVS_CONFIG_KEY = "config";
    static const char *NVS_ALERTS_KEY = "alerts";
    // The imaging-app alert settings (Config::AlertsPart::Client): their own
    // key keeps both alerts strings under the NVS limit.
    static const char *NVS_ALERTS_CLIENT_KEY = "alertclient";

    namespace
    {
        // Reads an NVS string straight into heap memory. Preferences::getString()
        // copies through a variable-length array on the stack - ~2 KB for the
        // config JSON, which nearly overflowed the 8 KB loop task during setup().
        bool readNvsString(const char *key, std::string &out)
        {
            nvs_handle_t handle;
            if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
                return false;
            size_t length = 0;
            bool ok = nvs_get_str(handle, key, nullptr, &length) == ESP_OK && length > 0;
            if (ok)
            {
                out.assign(length, '\0');
                ok = nvs_get_str(handle, key, &out[0], &length) == ESP_OK;
                out.resize(length > 0 ? length - 1 : 0); // drop the terminator
            }
            nvs_close(handle);
            return ok;
        }

        size_t nvsStringLength(const char *key)
        {
            nvs_handle_t handle;
            if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
                return 0;
            size_t length = 0;
            if (nvs_get_str(handle, key, nullptr, &length) != ESP_OK)
                length = 0;
            nvs_close(handle);
            return length > 0 ? length - 1 : 0;
        }
    }

    bool Config::load(Config &out)
    {
        Logger::info(TAG, "Loading configuration from NVS");

        std::string json;
        if (!readNvsString(NVS_CONFIG_KEY, json) || json.empty())
        {
            Logger::warn(TAG, "No config found in NVS, creating default");
            out = createDefault();
            if (out.save())
            {
                Logger::info(TAG, "Default config saved successfully");
                return true;
            }
            Logger::error(TAG, "Failed to save default config");
            return false;
        }

        Logger::info(TAG, "Loaded config JSON (%u bytes)", static_cast<unsigned>(json.length()));

        // Alerts live under their own NVS keys; splice them into the main
        // document and parse once, straight into the caller's Config.
        const size_t close = json.rfind('}');
        std::string alertsJson;
        std::string clientJson;
        const bool haveAlerts = readNvsString(NVS_ALERTS_KEY, alertsJson) && !alertsJson.empty() && close != std::string::npos;
        const bool haveClient = readNvsString(NVS_ALERTS_CLIENT_KEY, clientJson) && !clientJson.empty() && close != std::string::npos;
        std::string spliced = json;
        if (haveAlerts)
            spliced.insert(close, std::string(",\"alerts\":") + alertsJson);
        if (haveClient)
            spliced.insert(spliced.rfind('}'), std::string(",\"alertsClient\":") + clientJson);

        out = createDefault();
        std::string reason;
        bool ok = applyJson(spliced, out, false, &reason);
        if (!ok)
            Logger::error(TAG, "Stored config rejected: %s", reason.c_str());
        if (!ok && (haveAlerts || haveClient))
        {
            // A corrupt alerts entry mustn't take the whole config down.
            Logger::error(TAG, "Failed to parse config JSON with alerts - retrying without them");
            out = createDefault();
            ok = applyJson(json, out, false);
        }

        if (ok)
            Logger::info(TAG, "Config parsed successfully - SSID: '%s'", out.wifi.ssid.c_str());
        else
            Logger::error(TAG, "Failed to parse config JSON");

        return ok;
    }

    bool Config::save() const
    {
        Logger::info(TAG, "Attempting to save configuration to NVS...");

        std::string validationError;
        if (!validate(&validationError))
        {
            Logger::error(TAG, "Refusing to save invalid configuration: %s", validationError.c_str());
            return false;
        }

        std::string json = toJson(false, false);
        const std::string alertsJson = alertsToJson(false, AlertsPart::Main);
        const std::string clientJson = alertsToJson(false, AlertsPart::Client);
        Logger::info(TAG, "Config JSON to save (%u bytes, alerts %u bytes)", static_cast<unsigned>(json.length()),
                     static_cast<unsigned>(alertsJson.length()));

        if (json.length() > MAX_PERSISTED_JSON_BYTES)
        {
            Logger::error(TAG, "Config JSON too large for NVS (%u bytes, max %u bytes)",
                          static_cast<unsigned>(json.length()),
                          static_cast<unsigned>(MAX_PERSISTED_JSON_BYTES));
            return false;
        }

        Preferences prefs;
        if (!prefs.begin(NVS_NAMESPACE, false))
        {
            Logger::error(TAG, "Failed to open NVS namespace for writing");
            return false;
        }

        size_t written = prefs.putString(NVS_CONFIG_KEY, json.c_str());
        const size_t alertsWritten = prefs.putString(NVS_ALERTS_KEY, alertsJson.c_str());
        const size_t clientWritten = prefs.putString(NVS_ALERTS_CLIENT_KEY, clientJson.c_str());
        prefs.end();

        if (alertsWritten == 0 || clientWritten == 0)
        {
            Logger::error(TAG, "Failed to write alerts config to NVS");
            return false;
        }

        if (written == 0)
        {
            Logger::error(TAG, "Failed to write config to NVS");
            return false;
        }

        Logger::info(TAG, "Configuration saved successfully to NVS (%u bytes)", static_cast<unsigned>(written));

        Logger::info(TAG, "Verification: NVS contains %u bytes", static_cast<unsigned>(nvsStringLength(NVS_CONFIG_KEY)));

        return true;
    }

} // namespace SQM
