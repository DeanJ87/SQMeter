#include "Config.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <Preferences.h>

namespace SQM
{
    static const char *NVS_NAMESPACE = "sqm";
    static const char *NVS_CONFIG_KEY = "config";

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

        Logger::info(TAG, "Loaded config JSON (%d bytes): %s", jsonStr.length(), jsonStr.c_str());

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
        Logger::info(TAG, "Config JSON to save (%d bytes): %s", json.length(), json.c_str());

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
            Logger::debug(TAG, "Verification content: %s", verified.c_str());
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

        cfg.sensor.readIntervalMs = 5000; // 5 seconds
        cfg.sensor.i2cSDA = 21;
        cfg.sensor.i2cSCL = 22;
        cfg.sensor.i2cFrequency = 100000; // 100kHz

        cfg.primaryTimeSource = TimeSource::NTP;
        cfg.secondaryTimeSource = TimeSource::GPS;

        return cfg;
    }

    std::string Config::toJson() const
    {
        StaticJsonDocument<1536> doc; // Increased from 1024 for GPS config

        doc["deviceName"] = deviceName;
        doc["timezone"] = timezone;
        doc["primaryTimeSource"] = static_cast<int>(primaryTimeSource);
        doc["secondaryTimeSource"] = static_cast<int>(secondaryTimeSource);

        JsonObject wifi = doc.createNestedObject("wifi");
        wifi["ssid"] = this->wifi.ssid;
        wifi["password"] = this->wifi.password;
        wifi["hostname"] = this->wifi.hostname;
        wifi["autoReconnect"] = this->wifi.autoReconnect;
        wifi["reconnectDelayMs"] = this->wifi.reconnectDelayMs;
        wifi["maxReconnectDelayMs"] = this->wifi.maxReconnectDelayMs;

        JsonObject mqtt = doc.createNestedObject("mqtt");
        mqtt["enabled"] = this->mqtt.enabled;
        mqtt["broker"] = this->mqtt.broker;
        mqtt["port"] = this->mqtt.port;
        mqtt["username"] = this->mqtt.username;
        mqtt["password"] = this->mqtt.password;
        mqtt["topic"] = this->mqtt.topic;
        mqtt["publishIntervalMs"] = this->mqtt.publishIntervalMs;

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

        JsonObject sensor = doc.createNestedObject("sensor");
        sensor["readIntervalMs"] = this->sensor.readIntervalMs;
        sensor["i2cSDA"] = this->sensor.i2cSDA;
        sensor["i2cSCL"] = this->sensor.i2cSCL;
        sensor["i2cFrequency"] = this->sensor.i2cFrequency;

        std::string output;
        serializeJsonPretty(doc, output);
        return output;
    }

    std::optional<Config> Config::fromJson(const std::string &json)
    {
        StaticJsonDocument<1536> doc; // Increased from 1024 for GPS config
        DeserializationError error = deserializeJson(doc, json);

        if (error)
        {
            Logger::error(TAG, "JSON parse error: %s", error.c_str());
            return std::nullopt;
        }

        Config cfg;

        cfg.deviceName = doc["deviceName"] | "SQM-ESP32";
        cfg.timezone = doc["timezone"] | "UTC";
        cfg.primaryTimeSource = static_cast<TimeSource>(doc["primaryTimeSource"] | 0);     // 0 = NTP
        cfg.secondaryTimeSource = static_cast<TimeSource>(doc["secondaryTimeSource"] | 1); // 1 = GPS

        JsonObject wifi = doc["wifi"];
        cfg.wifi.ssid = wifi["ssid"] | "";
        cfg.wifi.password = wifi["password"] | "";
        cfg.wifi.hostname = wifi["hostname"] | "sqm-esp32";
        cfg.wifi.autoReconnect = wifi["autoReconnect"] | true;
        cfg.wifi.reconnectDelayMs = wifi["reconnectDelayMs"] | 1000;
        cfg.wifi.maxReconnectDelayMs = wifi["maxReconnectDelayMs"] | 300000;

        JsonObject mqtt = doc["mqtt"];
        cfg.mqtt.enabled = mqtt["enabled"] | false;
        cfg.mqtt.broker = mqtt["broker"] | "";
        cfg.mqtt.port = mqtt["port"] | 1883;
        cfg.mqtt.username = mqtt["username"] | "";
        cfg.mqtt.password = mqtt["password"] | "";
        cfg.mqtt.topic = mqtt["topic"] | "sqm/data";
        cfg.mqtt.publishIntervalMs = mqtt["publishIntervalMs"] | 60000;

        JsonObject ntp = doc["ntp"];
        cfg.ntp.enabled = ntp["enabled"] | true;
        cfg.ntp.server1 = ntp["server1"] | "pool.ntp.org";
        cfg.ntp.server2 = ntp["server2"] | "time.nist.gov";
        cfg.ntp.timezone = ntp["timezone"] | "UTC0";
        cfg.ntp.gmtOffsetSec = ntp["gmtOffsetSec"] | 0;
        cfg.ntp.daylightOffsetSec = ntp["daylightOffsetSec"] | 0;
        cfg.ntp.syncIntervalMs = ntp["syncIntervalMs"] | 3600000;

        JsonObject gps = doc["gps"];
        cfg.gps.enabled = gps["enabled"] | false;
        cfg.gps.rxPin = gps["rxPin"] | 17;
        cfg.gps.txPin = gps["txPin"] | 16;
        cfg.gps.baudRate = gps["baudRate"] | 9600;

        JsonObject sensor = doc["sensor"];
        cfg.sensor.readIntervalMs = sensor["readIntervalMs"] | 5000;
        cfg.sensor.i2cSDA = sensor["i2cSDA"] | 21;
        cfg.sensor.i2cSCL = sensor["i2cSCL"] | 22;
        cfg.sensor.i2cFrequency = sensor["i2cFrequency"] | 100000;

        return cfg;
    }

} // namespace SQM
