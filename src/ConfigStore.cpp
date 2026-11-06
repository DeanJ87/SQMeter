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
    // The battery events (Config::AlertsPart::Battery, spec 032).
    static const char *NVS_ALERTS_BATTERY_KEY = "alertsBatt";

    // Each document is stored as a blob under these keys. An NVS string must
    // fit in one 4 KB page, and on a device whose pages are already part used
    // (any device upgraded from older firmware) there's often no single page
    // with room for the ~3 KB config, so the save failed; a blob can span
    // pages. The string keys above are only read, to carry older settings over.
    struct StoredDoc
    {
        const char *blobKey;
        const char *legacyKey;
    };
    static const StoredDoc DOC_CONFIG{"configB", NVS_CONFIG_KEY};
    static const StoredDoc DOC_ALERTS{"alertsB", NVS_ALERTS_KEY};
    static const StoredDoc DOC_ALERTS_CLIENT{"alertclientB", NVS_ALERTS_CLIENT_KEY};
    static const StoredDoc DOC_ALERTS_BATTERY{"alertsBattB", NVS_ALERTS_BATTERY_KEY};

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

        // Reads a blob straight into heap memory, like readNvsString.
        bool readNvsBlob(const char *key, std::string &out)
        {
            nvs_handle_t handle;
            if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK)
                return false;
            size_t length = 0;
            bool ok = nvs_get_blob(handle, key, nullptr, &length) == ESP_OK && length > 0;
            if (ok)
            {
                out.assign(length, '\0');
                ok = nvs_get_blob(handle, key, &out[0], &length) == ESP_OK;
                out.resize(length);
            }
            nvs_close(handle);
            return ok;
        }

        // The stored document: the blob, or the string older firmware wrote.
        bool readDoc(const StoredDoc &doc, std::string &out)
        {
            return readNvsBlob(doc.blobKey, out) || readNvsString(doc.legacyKey, out);
        }

        // Writes the document as a blob and drops the string older firmware
        // wrote, freeing its page space. An empty document removes both.
        bool writeDoc(Preferences &prefs, const StoredDoc &doc, const std::string &json)
        {
            bool ok;
            if (json.empty())
            {
                prefs.remove(doc.blobKey);
                ok = true;
            }
            else
                ok = prefs.putBytes(doc.blobKey, json.data(), json.size()) == json.size();
            if (ok && prefs.isKey(doc.legacyKey))
                prefs.remove(doc.legacyKey);
            return ok;
        }

        // A stored alerts part, or "" when there is none.
        std::string readStoredPart(const StoredDoc &doc)
        {
            std::string part;
            if (!readDoc(doc, part))
                part.clear();
            return part;
        }
    } // namespace

    bool Config::load(Config &out)
    {
        Logger::info(TAG, "Loading configuration from NVS");

        std::string json;
        if (!readDoc(DOC_CONFIG, json) || json.empty())
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
        const std::string alertsJson = readStoredPart(DOC_ALERTS);
        const std::string clientJson = readStoredPart(DOC_ALERTS_CLIENT);
        const std::string batteryJson = readStoredPart(DOC_ALERTS_BATTERY);
        const bool haveParts = !alertsJson.empty() || !clientJson.empty() || !batteryJson.empty();
        const std::string spliced = spliceStored(json, alertsJson, clientJson, batteryJson);

        out = createDefault();
        std::string reason;
        bool ok = applyJson(spliced, out, false, &reason);
        if (!ok)
            Logger::error(TAG, "Stored config rejected: %s", reason.c_str());
        if (!ok && haveParts)
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
        const std::string batteryJson = alertsToJson(false, AlertsPart::Battery);
        Logger::info(
            TAG,
            "Config JSON to save (%u bytes, alerts %u bytes)",
            static_cast<unsigned>(json.length()),
            static_cast<unsigned>(alertsJson.length()));

        if (json.length() > MAX_PERSISTED_JSON_BYTES)
        {
            Logger::error(
                TAG,
                "Config JSON too large for NVS (%u bytes, max %u bytes)",
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

        const bool alertsOk = writeDoc(prefs, DOC_ALERTS, alertsJson) && writeDoc(prefs, DOC_ALERTS_CLIENT, clientJson) &&
                              writeDoc(prefs, DOC_ALERTS_BATTERY, batteryJson);
        const bool configOk = writeDoc(prefs, DOC_CONFIG, json);
        prefs.end();

        if (!alertsOk)
        {
            Logger::error(TAG, "Failed to write alerts config to NVS");
            return false;
        }

        if (!configOk)
        {
            Logger::error(TAG, "Failed to write config to NVS");
            return false;
        }

        Logger::info(TAG, "Configuration saved successfully to NVS (%u bytes)", static_cast<unsigned>(json.size()));

        return true;
    }

} // namespace SQM
