#include "Config.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <cstring>

namespace SQM
{
    static const char *NVS_NAMESPACE = "sqm";
    static const char *NVS_CONFIG_KEY = "config";
    static const char *SECRET_MASK = "********";

    namespace
    {
        bool isPlaceholderSecret(const char *value)
        {
            return value == nullptr || value[0] == '\0' ||
                   std::strcmp(value, SECRET_MASK) == 0 ||
                   std::strcmp(value, "***") == 0;
        }

        void assignSecret(JsonObject obj, const char *key, std::string &target, bool preservePlaceholders)
        {
            if (!obj.containsKey(key))
            {
                return;
            }

            JsonVariant value = obj[key];
            if (value.isNull())
            {
                target.clear();
                return;
            }

            const char *secret = value | "";
            if (preservePlaceholders && isPlaceholderSecret(secret))
            {
                return;
            }

            target = secret;
        }
    }

    std::optional<Config> Config::load()
    {
        Logger::info(TAG, "Loading configuration from NVS");

        Preferences prefs;
        if (!prefs.begin(NVS_NAMESPACE, true))
        {
            Logger::error(TAG, "Failed to open NVS namespace");
            return std::nullopt;
        }

        String jsonStr = prefs.getString(NVS_CONFIG_KEY, "");
        prefs.end();

        if (jsonStr.length() == 0)
        {
            Logger::warn(TAG, "No config found in NVS, creating default");
            Config defaultCfg = createDefault();
            if (defaultCfg.save())
            {
                Logger::info(TAG, "Default config saved successfully");
                return defaultCfg;
            }
            Logger::error(TAG, "Failed to save default config");
            return std::nullopt;
        }

        Logger::info(TAG, "Loaded config JSON (%d bytes)", jsonStr.length());

        std::string json = jsonStr.c_str();
        auto config = fromJson(json);
        if (config)
        {
            Logger::info(TAG, "Config parsed successfully - SSID: '%s'", config->wifi.ssid.c_str());
        }
        else
        {
            Logger::error(TAG, "Failed to parse config JSON");
        }

        return config;
    }

    bool Config::save() const
    {
        Logger::info(TAG, "Attempting to save configuration to NVS...");

        std::string json = toJson();
        Logger::info(TAG, "Config JSON to save (%d bytes)", json.length());

        Preferences prefs;
        if (!prefs.begin(NVS_NAMESPACE, false))
        {
            Logger::error(TAG, "Failed to open NVS namespace for writing");
            return false;
        }

        size_t written = prefs.putString(NVS_CONFIG_KEY, json.c_str());
        prefs.end();

        if (written == 0)
        {
            Logger::error(TAG, "Failed to write config to NVS");
            return false;
        }

        Logger::info(TAG, "Configuration saved successfully to NVS (%d bytes)", written);

        // Verify by reading back
        Preferences verifyPrefs;
        if (verifyPrefs.begin(NVS_NAMESPACE, true))
        {
            String verified = verifyPrefs.getString(NVS_CONFIG_KEY, "");
            verifyPrefs.end();
            Logger::info(TAG, "Verification: NVS contains %d bytes", verified.length());
        }

        return true;
    }

    Config Config::createDefault()
    {
        Config cfg;

        cfg.deviceName = "SQM-ESP32";
        cfg.timezone = "UTC";

        cfg.wifi.ssid = "";
        cfg.wifi.password = "";
        cfg.wifi.hostname = "sqm-esp32";
        cfg.wifi.autoReconnect = true;
        cfg.wifi.reconnectDelayMs = 1000;
        cfg.wifi.maxReconnectDelayMs = 300000; // 5 minutes

        cfg.mqtt.enabled = false;
        cfg.mqtt.broker = "";
        cfg.mqtt.port = 1883;
        cfg.mqtt.username = "";
        cfg.mqtt.password = "";
        cfg.mqtt.topic = "sqm/data";
        cfg.mqtt.publishIntervalMs = 60000; // 1 minute

        cfg.ota.enabled = false;
        cfg.ota.password = "";

        cfg.ntp.enabled = true;
        cfg.ntp.server1 = "pool.ntp.org";
        cfg.ntp.server2 = "time.nist.gov";
        cfg.ntp.timezone = "UTC0"; // POSIX format
        cfg.ntp.gmtOffsetSec = 0;
        cfg.ntp.daylightOffsetSec = 0;
        cfg.ntp.syncIntervalMs = 600000; // 10 minutes

        cfg.gps.enabled = false;
        cfg.gps.rxPin = 17;
        cfg.gps.txPin = 16;
        cfg.gps.baudRate = 9600;

        cfg.rain.enabled = false;
        cfg.rain.rxPin = 18;
        cfg.rain.txPin = 19;
        cfg.rain.baudRate = 9600;
        cfg.rain.mode = "polling";
        cfg.rain.resolution = "high";
        cfg.rain.units = "metric";

        cfg.sensor.readIntervalMs = 5000; // 5 seconds
        cfg.sensor.i2cSDA = 21;
        cfg.sensor.i2cSCL = 22;
        cfg.sensor.i2cFrequency = 100000; // 100kHz

        cfg.primaryTimeSource = TimeSource::NTP;
        cfg.secondaryTimeSource = TimeSource::GPS;

        return cfg;
    }

    std::string Config::toJson(bool redactSecrets) const
    {
        StaticJsonDocument<3072> doc;

        doc["deviceName"] = deviceName;
        doc["timezone"] = timezone;
        doc["primaryTimeSource"] = static_cast<int>(primaryTimeSource);
        doc["secondaryTimeSource"] = static_cast<int>(secondaryTimeSource);

        JsonObject wifi = doc.createNestedObject("wifi");
        wifi["ssid"] = this->wifi.ssid;
        wifi["password"] = redactSecrets && !this->wifi.password.empty() ? SECRET_MASK : this->wifi.password.c_str();
        wifi["hostname"] = this->wifi.hostname;
        wifi["autoReconnect"] = this->wifi.autoReconnect;
        wifi["reconnectDelayMs"] = this->wifi.reconnectDelayMs;
        wifi["maxReconnectDelayMs"] = this->wifi.maxReconnectDelayMs;

        JsonObject mqtt = doc.createNestedObject("mqtt");
        mqtt["enabled"] = this->mqtt.enabled;
        mqtt["broker"] = this->mqtt.broker;
        mqtt["port"] = this->mqtt.port;
        mqtt["username"] = this->mqtt.username;
        mqtt["password"] = redactSecrets && !this->mqtt.password.empty() ? SECRET_MASK : this->mqtt.password.c_str();
        mqtt["topic"] = this->mqtt.topic;
        mqtt["publishIntervalMs"] = this->mqtt.publishIntervalMs;

        JsonObject ota = doc.createNestedObject("ota");
        ota["enabled"] = this->ota.enabled;
        ota["password"] = redactSecrets && !this->ota.password.empty() ? SECRET_MASK : this->ota.password.c_str();

        JsonObject ntp = doc.createNestedObject("ntp");
        ntp["enabled"] = this->ntp.enabled;
        ntp["server1"] = this->ntp.server1;
        ntp["server2"] = this->ntp.server2;
        ntp["timezone"] = this->ntp.timezone;
        ntp["gmtOffsetSec"] = this->ntp.gmtOffsetSec;
        ntp["daylightOffsetSec"] = this->ntp.daylightOffsetSec;
        ntp["syncIntervalMs"] = this->ntp.syncIntervalMs;

        JsonObject gps = doc.createNestedObject("gps");
        gps["enabled"] = this->gps.enabled;
        gps["rxPin"] = this->gps.rxPin;
        gps["txPin"] = this->gps.txPin;
        gps["baudRate"] = this->gps.baudRate;

        JsonObject rain = doc.createNestedObject("rain");
        rain["enabled"] = this->rain.enabled;
        rain["rxPin"] = this->rain.rxPin;
        rain["txPin"] = this->rain.txPin;
        rain["baudRate"] = this->rain.baudRate;
        rain["mode"] = this->rain.mode;
        rain["resolution"] = this->rain.resolution;
        rain["units"] = this->rain.units;

        JsonObject sensor = doc.createNestedObject("sensor");
        sensor["readIntervalMs"] = this->sensor.readIntervalMs;
        sensor["i2cSDA"] = this->sensor.i2cSDA;
        sensor["i2cSCL"] = this->sensor.i2cSCL;
        sensor["i2cFrequency"] = this->sensor.i2cFrequency;

        std::string output;
        serializeJson(doc, output);
        return output;
    }

    std::optional<Config> Config::fromJson(const std::string &json, const Config *baseConfig)
    {
        StaticJsonDocument<3072> doc;
        DeserializationError error = deserializeJson(doc, json);

        if (error)
        {
            Logger::error(TAG, "JSON parse error: %s", error.c_str());
            return std::nullopt;
        }

        Config cfg = baseConfig != nullptr ? *baseConfig : createDefault();
        const bool preserveSecretPlaceholders = baseConfig != nullptr;

        if (doc.containsKey("deviceName"))
            cfg.deviceName = doc["deviceName"] | "SQM-ESP32";
        if (doc.containsKey("timezone"))
            cfg.timezone = doc["timezone"] | "UTC";
        if (doc.containsKey("primaryTimeSource"))
            cfg.primaryTimeSource = static_cast<TimeSource>(doc["primaryTimeSource"] | 0); // 0 = NTP
        if (doc.containsKey("secondaryTimeSource"))
            cfg.secondaryTimeSource = static_cast<TimeSource>(doc["secondaryTimeSource"] | 1); // 1 = GPS

        JsonObject wifi = doc["wifi"];
        if (!wifi.isNull())
        {
            if (wifi.containsKey("ssid"))
                cfg.wifi.ssid = wifi["ssid"] | "";
            assignSecret(wifi, "password", cfg.wifi.password, preserveSecretPlaceholders);
            if (wifi.containsKey("hostname"))
                cfg.wifi.hostname = wifi["hostname"] | "sqm-esp32";
            if (wifi.containsKey("autoReconnect"))
                cfg.wifi.autoReconnect = wifi["autoReconnect"] | true;
            if (wifi.containsKey("reconnectDelayMs"))
                cfg.wifi.reconnectDelayMs = wifi["reconnectDelayMs"] | 1000;
            if (wifi.containsKey("maxReconnectDelayMs"))
                cfg.wifi.maxReconnectDelayMs = wifi["maxReconnectDelayMs"] | 300000;
        }

        JsonObject mqtt = doc["mqtt"];
        if (!mqtt.isNull())
        {
            if (mqtt.containsKey("enabled"))
                cfg.mqtt.enabled = mqtt["enabled"] | false;
            if (mqtt.containsKey("broker"))
                cfg.mqtt.broker = mqtt["broker"] | "";
            if (mqtt.containsKey("port"))
                cfg.mqtt.port = mqtt["port"] | 1883;
            if (mqtt.containsKey("username"))
                cfg.mqtt.username = mqtt["username"] | "";
            assignSecret(mqtt, "password", cfg.mqtt.password, preserveSecretPlaceholders);
            if (mqtt.containsKey("topic"))
                cfg.mqtt.topic = mqtt["topic"] | "sqm/data";
            if (mqtt.containsKey("publishIntervalMs"))
                cfg.mqtt.publishIntervalMs = mqtt["publishIntervalMs"] | 60000;
        }

        JsonObject ota = doc["ota"];
        if (!ota.isNull())
        {
            if (ota.containsKey("enabled"))
                cfg.ota.enabled = ota["enabled"] | false;
            assignSecret(ota, "password", cfg.ota.password, preserveSecretPlaceholders);
        }

        JsonObject ntp = doc["ntp"];
        if (!ntp.isNull())
        {
            if (ntp.containsKey("enabled"))
                cfg.ntp.enabled = ntp["enabled"] | true;
            if (ntp.containsKey("server1"))
                cfg.ntp.server1 = ntp["server1"] | "pool.ntp.org";
            if (ntp.containsKey("server2"))
                cfg.ntp.server2 = ntp["server2"] | "time.nist.gov";
            if (ntp.containsKey("timezone"))
                cfg.ntp.timezone = ntp["timezone"] | "UTC0";
            if (ntp.containsKey("gmtOffsetSec"))
                cfg.ntp.gmtOffsetSec = ntp["gmtOffsetSec"] | 0;
            if (ntp.containsKey("daylightOffsetSec"))
                cfg.ntp.daylightOffsetSec = ntp["daylightOffsetSec"] | 0;
            if (ntp.containsKey("syncIntervalMs"))
                cfg.ntp.syncIntervalMs = ntp["syncIntervalMs"] | 3600000;
        }

        JsonObject gps = doc["gps"];
        if (!gps.isNull())
        {
            if (gps.containsKey("enabled"))
                cfg.gps.enabled = gps["enabled"] | false;
            if (gps.containsKey("rxPin"))
                cfg.gps.rxPin = gps["rxPin"] | 17;
            if (gps.containsKey("txPin"))
                cfg.gps.txPin = gps["txPin"] | 16;
            if (gps.containsKey("baudRate"))
                cfg.gps.baudRate = gps["baudRate"] | 9600;
        }

        JsonObject rain = doc["rain"];
        if (!rain.isNull())
        {
            if (rain.containsKey("enabled"))
                cfg.rain.enabled = rain["enabled"] | false;
            if (rain.containsKey("rxPin"))
                cfg.rain.rxPin = rain["rxPin"] | 18;
            if (rain.containsKey("txPin"))
                cfg.rain.txPin = rain["txPin"] | 19;
            if (rain.containsKey("baudRate"))
                cfg.rain.baudRate = rain["baudRate"] | 9600;
            if (rain.containsKey("mode"))
                cfg.rain.mode = rain["mode"] | "polling";
            if (rain.containsKey("resolution"))
                cfg.rain.resolution = rain["resolution"] | "high";
            if (rain.containsKey("units"))
                cfg.rain.units = rain["units"] | "metric";
        }

        JsonObject sensor = doc["sensor"];
        if (!sensor.isNull())
        {
            if (sensor.containsKey("readIntervalMs"))
                cfg.sensor.readIntervalMs = sensor["readIntervalMs"] | 5000;
            if (sensor.containsKey("i2cSDA"))
                cfg.sensor.i2cSDA = sensor["i2cSDA"] | 21;
            if (sensor.containsKey("i2cSCL"))
                cfg.sensor.i2cSCL = sensor["i2cSCL"] | 22;
            if (sensor.containsKey("i2cFrequency"))
                cfg.sensor.i2cFrequency = sensor["i2cFrequency"] | 100000;
        }

        return cfg;
    }

} // namespace SQM
