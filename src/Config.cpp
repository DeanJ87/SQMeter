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

    namespace
    {
        bool isValidGpio(int pin)
        {
            switch (pin)
            {
            case 0:
            case 1:
            case 2:
            case 3:
            case 4:
            case 5:
            case 12:
            case 13:
            case 14:
            case 15:
            case 16:
            case 17:
            case 18:
            case 19:
            case 21:
            case 22:
            case 23:
            case 25:
            case 26:
            case 27:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 39:
                return true;
            default:
                return false;
            }
        }

        bool isValidBaudRate(uint32_t baudRate)
        {
            return baudRate == 2400 || baudRate == 4800 || baudRate == 9600 || baudRate == 19200 ||
                   baudRate == 38400 || baudRate == 57600 || baudRate == 115200;
        }

        bool setError(std::string *error, const char *message)
        {
            if (error)
            {
                *error = message;
            }
            return false;
        }

        bool isTimeSourceEnabled(const Config &cfg, TimeSource source)
        {
            return source == TimeSource::NTP ? cfg.ntp.enabled : cfg.gps.enabled;
        }

        void normalizeTimeSources(Config &cfg)
        {
            if (!cfg.ntp.enabled && !cfg.gps.enabled)
            {
                return;
            }

            if (!isTimeSourceEnabled(cfg, cfg.primaryTimeSource))
            {
                cfg.primaryTimeSource = cfg.ntp.enabled ? TimeSource::NTP : TimeSource::GPS;
            }

            if (!isTimeSourceEnabled(cfg, cfg.secondaryTimeSource))
            {
                cfg.secondaryTimeSource = cfg.gps.enabled && cfg.primaryTimeSource != TimeSource::GPS
                                            ? TimeSource::GPS
                                            : TimeSource::NTP;
            }

            if (cfg.ntp.enabled && cfg.gps.enabled && cfg.primaryTimeSource == cfg.secondaryTimeSource)
            {
                cfg.secondaryTimeSource = cfg.primaryTimeSource == TimeSource::NTP ? TimeSource::GPS : TimeSource::NTP;
            }

            if (!cfg.ntp.enabled || !cfg.gps.enabled)
            {
                cfg.secondaryTimeSource = cfg.primaryTimeSource;
            }
        }
    } // namespace

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

        std::string validationError;
        if (!validate(&validationError))
        {
            Logger::error(TAG, "Refusing to save invalid configuration: %s", validationError.c_str());
            return false;
        }

        std::string json = toJson();
        Logger::info(TAG, "Config JSON to save (%u bytes)", static_cast<unsigned>(json.length()));

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

        cfg.auth.enabled = false;
        cfg.auth.username = "admin";
        cfg.auth.password = "";

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
        cfg.rain.debugUart = false;
        cfg.rain.mode = "polling";
        cfg.rain.resolution = "high";
        cfg.rain.units = "metric";
        cfg.rain.pollIntervalMs = 5000;
        cfg.rain.rainClearDelayMs = 15UL * 60UL * 1000UL;
        cfg.rain.dailyResetEnabled = false;
        cfg.rain.dailyResetHour = 0;
        cfg.rain.dailyResetMinute = 0;

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
        StaticJsonDocument<3584> doc;

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

        JsonObject auth = doc.createNestedObject("auth");
        auth["enabled"] = this->auth.enabled;
        auth["username"] = this->auth.username;
        auth["password"] = redactSecrets && !this->auth.password.empty() ? SECRET_MASK : this->auth.password.c_str();

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
        rain["debugUart"] = this->rain.debugUart;
        rain["mode"] = this->rain.mode;
        rain["resolution"] = this->rain.resolution;
        rain["units"] = this->rain.units;
        rain["pollIntervalMs"] = this->rain.pollIntervalMs;
        rain["rainClearDelayMs"] = this->rain.rainClearDelayMs;
        rain["dailyResetEnabled"] = this->rain.dailyResetEnabled;
        rain["dailyResetHour"] = this->rain.dailyResetHour;
        rain["dailyResetMinute"] = this->rain.dailyResetMinute;

        JsonObject sensor = doc.createNestedObject("sensor");
        sensor["readIntervalMs"] = this->sensor.readIntervalMs;
        sensor["i2cSDA"] = this->sensor.i2cSDA;
        sensor["i2cSCL"] = this->sensor.i2cSCL;
        sensor["i2cFrequency"] = this->sensor.i2cFrequency;

        std::string output;
        serializeJson(doc, output);
        return output;
    }

    bool Config::validate(std::string *error) const
    {
        if (deviceName.empty())
        {
            return setError(error, "Device name is required");
        }

        if (primaryTimeSource != TimeSource::NTP && primaryTimeSource != TimeSource::GPS)
        {
            return setError(error, "Primary time source is invalid");
        }

        if (secondaryTimeSource != TimeSource::NTP && secondaryTimeSource != TimeSource::GPS)
        {
            return setError(error, "Secondary time source is invalid");
        }

        if (!ntp.enabled && !gps.enabled)
        {
            return setError(error, "At least one time source must be enabled");
        }

        if (!isTimeSourceEnabled(*this, primaryTimeSource))
        {
            return setError(error, "Primary time source is disabled");
        }

        if (ntp.enabled && gps.enabled && !isTimeSourceEnabled(*this, secondaryTimeSource))
        {
            return setError(error, "Secondary time source is disabled");
        }

        if (ntp.enabled && gps.enabled && primaryTimeSource == secondaryTimeSource)
        {
            return setError(error, "Time sources must be different when both NTP and GPS are enabled");
        }

        if (wifi.reconnectDelayMs == 0 || wifi.maxReconnectDelayMs == 0 ||
            wifi.reconnectDelayMs > 86400000 || wifi.maxReconnectDelayMs > 86400000 ||
            wifi.reconnectDelayMs > wifi.maxReconnectDelayMs)
        {
            return setError(error, "WiFi reconnect delays are invalid");
        }

        if (mqtt.port == 0)
        {
            return setError(error, "MQTT port is invalid");
        }

        if (mqtt.enabled && (mqtt.broker.empty() || mqtt.topic.empty()))
        {
            return setError(error, "MQTT broker and topic are required when MQTT is enabled");
        }

        if (mqtt.publishIntervalMs < 1000 || mqtt.publishIntervalMs > 86400000)
        {
            return setError(error, "MQTT publish interval is invalid");
        }

        if (auth.enabled && auth.password.empty())
        {
            return setError(error, "HTTP auth password is required when auth is enabled");
        }

        if (auth.enabled && auth.username.empty())
        {
            return setError(error, "HTTP auth username is required when auth is enabled");
        }

        if (ntp.enabled && ntp.server1.empty())
        {
            return setError(error, "Primary NTP server is required when NTP is enabled");
        }

        if (ntp.syncIntervalMs < 600000 || ntp.syncIntervalMs > 86400000)
        {
            return setError(error, "NTP sync interval is invalid");
        }

        if (!isValidGpio(gps.rxPin) || !isValidGpio(gps.txPin) || gps.rxPin == gps.txPin)
        {
            return setError(error, "GPS pins are invalid");
        }

        if (!isValidBaudRate(gps.baudRate))
        {
            return setError(error, "GPS baud rate is invalid");
        }

        if (!isValidGpio(rain.rxPin) || !isValidGpio(rain.txPin) || (rain.enabled && rain.rxPin == rain.txPin))
        {
            return setError(error, "Rain sensor pins are invalid");
        }

        if (!isValidBaudRate(rain.baudRate))
        {
            return setError(error, "Rain sensor baud rate is invalid");
        }

        if (rain.mode != "polling")
        {
            return setError(error, "Rain sensor mode must be polling");
        }

        if (rain.resolution != "high" && rain.resolution != "low" && rain.resolution != "switch")
        {
            return setError(error, "Rain sensor resolution is invalid");
        }

        if (rain.units != "metric" && rain.units != "imperial" && rain.units != "switch")
        {
            return setError(error, "Rain sensor units are invalid");
        }

        if (rain.pollIntervalMs < 1000 || rain.pollIntervalMs > 3600000)
        {
            return setError(error, "Rain sensor poll interval is invalid");
        }

        if (rain.rainClearDelayMs < 60000 || rain.rainClearDelayMs > 86400000)
        {
            return setError(error, "Rain clear delay is invalid");
        }

        if (rain.dailyResetHour > 23 || rain.dailyResetMinute > 59)
        {
            return setError(error, "Rain daily reset time is invalid");
        }

        if (sensor.readIntervalMs < 100 || sensor.readIntervalMs > 3600000)
        {
            return setError(error, "Sensor read interval is invalid");
        }

        if (!isValidGpio(sensor.i2cSDA) || !isValidGpio(sensor.i2cSCL) || sensor.i2cSDA == sensor.i2cSCL)
        {
            return setError(error, "I2C pins are invalid");
        }

        if (sensor.i2cFrequency < 10000 || sensor.i2cFrequency > 400000)
        {
            return setError(error, "I2C frequency is invalid");
        }

        return true;
    }

    std::optional<Config> Config::fromJson(const std::string &json, const Config *baseConfig)
    {
        StaticJsonDocument<3584> doc;
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

        JsonObject auth = doc["auth"];
        if (!auth.isNull())
        {
            if (auth.containsKey("enabled"))
                cfg.auth.enabled = auth["enabled"] | false;
            if (auth.containsKey("username"))
                cfg.auth.username = auth["username"] | "admin";
            assignSecret(auth, "password", cfg.auth.password, preserveSecretPlaceholders);
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
            if (rain.containsKey("debugUart"))
                cfg.rain.debugUart = rain["debugUart"] | false;
            if (rain.containsKey("mode"))
                cfg.rain.mode = rain["mode"] | "polling";
            if (cfg.rain.mode != "polling")
                cfg.rain.mode = "polling";
            if (rain.containsKey("resolution"))
                cfg.rain.resolution = rain["resolution"] | "high";
            if (rain.containsKey("units"))
                cfg.rain.units = rain["units"] | "metric";
            if (rain.containsKey("pollIntervalMs"))
                cfg.rain.pollIntervalMs = rain["pollIntervalMs"] | 5000;
            if (rain.containsKey("rainClearDelayMs"))
                cfg.rain.rainClearDelayMs = rain["rainClearDelayMs"] | (15UL * 60UL * 1000UL);
            if (rain.containsKey("dailyResetEnabled"))
                cfg.rain.dailyResetEnabled = rain["dailyResetEnabled"] | false;
            if (rain.containsKey("dailyResetHour"))
                cfg.rain.dailyResetHour = rain["dailyResetHour"] | 0;
            if (rain.containsKey("dailyResetMinute"))
                cfg.rain.dailyResetMinute = rain["dailyResetMinute"] | 0;
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

        normalizeTimeSources(cfg);

        std::string validationError;
        if (!cfg.validate(&validationError))
        {
            Logger::error(TAG, "Configuration validation failed: %s", validationError.c_str());
            return std::nullopt;
        }

        return cfg;
    }

} // namespace SQM
