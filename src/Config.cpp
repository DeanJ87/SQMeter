#include "Config.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <Preferences.h>

namespace SQM
{
    static const char *NVS_NAMESPACE = "sqm";
    static const char *NVS_CONFIG_KEY = "config";

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

    std::string Config::toJson(bool includeSecrets) const
    {
        StaticJsonDocument<2048> doc;

        doc["deviceName"] = deviceName;
        doc["timezone"] = timezone;
        doc["primaryTimeSource"] = static_cast<int>(primaryTimeSource);
        doc["secondaryTimeSource"] = static_cast<int>(secondaryTimeSource);

        JsonObject wifi = doc.createNestedObject("wifi");
        wifi["ssid"] = this->wifi.ssid;
        wifi["password"] = includeSecrets ? this->wifi.password : "";
        wifi["hostname"] = this->wifi.hostname;
        wifi["autoReconnect"] = this->wifi.autoReconnect;
        wifi["reconnectDelayMs"] = this->wifi.reconnectDelayMs;
        wifi["maxReconnectDelayMs"] = this->wifi.maxReconnectDelayMs;

        JsonObject mqtt = doc.createNestedObject("mqtt");
        mqtt["enabled"] = this->mqtt.enabled;
        mqtt["broker"] = this->mqtt.broker;
        mqtt["port"] = this->mqtt.port;
        mqtt["username"] = this->mqtt.username;
        mqtt["password"] = includeSecrets ? this->mqtt.password : "";
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

        if (rain.mode != "polling" && rain.mode != "continuous")
        {
            return setError(error, "Rain sensor mode is invalid");
        }

        if (rain.resolution != "high" && rain.resolution != "low" && rain.resolution != "switch")
        {
            return setError(error, "Rain sensor resolution is invalid");
        }

        if (rain.units != "metric" && rain.units != "imperial" && rain.units != "switch")
        {
            return setError(error, "Rain sensor units are invalid");
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

    std::optional<Config> Config::fromJson(const std::string &json)
    {
        StaticJsonDocument<2048> doc;
        DeserializationError error = deserializeJson(doc, json);

        if (error)
        {
            Logger::error(TAG, "JSON parse error: %s", error.c_str());
            return std::nullopt;
        }

        Config cfg;

        cfg.deviceName = doc["deviceName"] | "SQM-ESP32";
        cfg.timezone = doc["timezone"] | "UTC";
        int primaryTimeSource = doc["primaryTimeSource"] | 0;     // 0 = NTP
        int secondaryTimeSource = doc["secondaryTimeSource"] | 1; // 1 = GPS
        cfg.primaryTimeSource = static_cast<TimeSource>(primaryTimeSource);
        cfg.secondaryTimeSource = static_cast<TimeSource>(secondaryTimeSource);

        JsonObject wifi = doc["wifi"];
        cfg.wifi.ssid = wifi["ssid"] | "";
        cfg.wifi.password = wifi["password"] | "";
        cfg.wifi.hostname = wifi["hostname"] | "sqm-esp32";
        cfg.wifi.autoReconnect = wifi["autoReconnect"] | true;
        int64_t wifiReconnectDelayMs = wifi["reconnectDelayMs"] | 1000;
        int64_t wifiMaxReconnectDelayMs = wifi["maxReconnectDelayMs"] | 300000;

        JsonObject mqtt = doc["mqtt"];
        cfg.mqtt.enabled = mqtt["enabled"] | false;
        cfg.mqtt.broker = mqtt["broker"] | "";
        int64_t mqttPort = mqtt["port"] | 1883;
        cfg.mqtt.username = mqtt["username"] | "";
        cfg.mqtt.password = mqtt["password"] | "";
        cfg.mqtt.topic = mqtt["topic"] | "sqm/data";
        int64_t mqttPublishIntervalMs = mqtt["publishIntervalMs"] | 60000;

        JsonObject ntp = doc["ntp"];
        cfg.ntp.enabled = ntp["enabled"] | true;
        cfg.ntp.server1 = ntp["server1"] | "pool.ntp.org";
        cfg.ntp.server2 = ntp["server2"] | "time.nist.gov";
        cfg.ntp.timezone = ntp["timezone"] | "UTC0";
        cfg.ntp.gmtOffsetSec = ntp["gmtOffsetSec"] | 0;
        cfg.ntp.daylightOffsetSec = ntp["daylightOffsetSec"] | 0;
        int64_t ntpSyncIntervalMs = ntp["syncIntervalMs"] | 3600000;

        JsonObject gps = doc["gps"];
        cfg.gps.enabled = gps["enabled"] | false;
        int gpsRxPin = gps["rxPin"] | 17;
        int gpsTxPin = gps["txPin"] | 16;
        int64_t gpsBaudRate = gps["baudRate"] | 9600;

        JsonObject rain = doc["rain"];
        cfg.rain.enabled = rain["enabled"] | false;
        int rainRxPin = rain["rxPin"] | 18;
        int rainTxPin = rain["txPin"] | 19;
        int64_t rainBaudRate = rain["baudRate"] | 9600;
        cfg.rain.mode = rain["mode"] | "polling";
        cfg.rain.resolution = rain["resolution"] | "high";
        cfg.rain.units = rain["units"] | "metric";

        JsonObject sensor = doc["sensor"];
        int64_t sensorReadIntervalMs = sensor["readIntervalMs"] | 5000;
        int i2cSDA = sensor["i2cSDA"] | 21;
        int i2cSCL = sensor["i2cSCL"] | 22;
        int64_t i2cFrequency = sensor["i2cFrequency"] | 100000;

        if (wifiReconnectDelayMs < 0 || wifiMaxReconnectDelayMs < 0 ||
            mqttPort < 0 || mqttPort > 65535 ||
            mqttPublishIntervalMs < 0 || mqttPublishIntervalMs > 86400000 ||
            ntpSyncIntervalMs < 0 || ntpSyncIntervalMs > 86400000 ||
            gpsBaudRate < 0 || gpsBaudRate > 115200 ||
            rainBaudRate < 0 || rainBaudRate > 115200 ||
            sensorReadIntervalMs < 0 || sensorReadIntervalMs > 3600000 ||
            i2cFrequency < 0 || i2cFrequency > 400000)
        {
            Logger::error(TAG, "Configuration contains numeric values outside allowed ranges");
            return std::nullopt;
        }

        if (!isValidGpio(gpsRxPin) || !isValidGpio(gpsTxPin) ||
            !isValidGpio(rainRxPin) || !isValidGpio(rainTxPin) ||
            !isValidGpio(i2cSDA) || !isValidGpio(i2cSCL))
        {
            Logger::error(TAG, "Configuration contains invalid GPIO pin values");
            return std::nullopt;
        }

        cfg.wifi.reconnectDelayMs = static_cast<uint32_t>(wifiReconnectDelayMs);
        cfg.wifi.maxReconnectDelayMs = static_cast<uint32_t>(wifiMaxReconnectDelayMs);
        cfg.mqtt.port = static_cast<uint16_t>(mqttPort);
        cfg.mqtt.publishIntervalMs = static_cast<uint32_t>(mqttPublishIntervalMs);
        cfg.ntp.syncIntervalMs = static_cast<uint32_t>(ntpSyncIntervalMs);
        cfg.gps.baudRate = static_cast<uint32_t>(gpsBaudRate);
        cfg.gps.rxPin = static_cast<uint8_t>(gpsRxPin);
        cfg.gps.txPin = static_cast<uint8_t>(gpsTxPin);
        cfg.rain.baudRate = static_cast<uint32_t>(rainBaudRate);
        cfg.rain.rxPin = static_cast<uint8_t>(rainRxPin);
        cfg.rain.txPin = static_cast<uint8_t>(rainTxPin);
        cfg.sensor.readIntervalMs = static_cast<uint32_t>(sensorReadIntervalMs);
        cfg.sensor.i2cSDA = static_cast<uint8_t>(i2cSDA);
        cfg.sensor.i2cSCL = static_cast<uint8_t>(i2cSCL);
        cfg.sensor.i2cFrequency = static_cast<uint32_t>(i2cFrequency);

        std::string validationError;
        if (!cfg.validate(&validationError))
        {
            Logger::error(TAG, "Configuration validation failed: %s", validationError.c_str());
            return std::nullopt;
        }

        return cfg;
    }

} // namespace SQM
