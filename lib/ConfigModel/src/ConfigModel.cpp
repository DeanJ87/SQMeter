#include "Config.h"
#include "ConfigInternal.h"
#include "LanguageLogic.h"
#include <cstring>

// The defaults. JSON is in ConfigJsonOut.cpp and ConfigJsonIn.cpp, validation in
// ConfigValidate.cpp.

namespace SQM
{
    namespace
    {
        void defaultNetwork(Config &cfg)
        {
            cfg.wifi.ssid = "";
            cfg.wifi.password = "";
            cfg.wifi.hostname = "sqmeter";
            cfg.wifi.mdns = true;
            cfg.wifi.ipv6 = true;
            cfg.wifi.autoReconnect = true;
            cfg.wifi.reconnectDelayMs = 1000;
            cfg.wifi.maxReconnectDelayMs = 300000; // 5 minutes

            cfg.mqtt.enabled = false;
            cfg.mqtt.broker = "";
            cfg.mqtt.port = 1883;
            cfg.mqtt.username = "";
            cfg.mqtt.password = "";
            cfg.mqtt.topic = "sqmeter";
            cfg.mqtt.publish = MQTTConfig::Publish{};
            cfg.mqtt.homeAssistant = false;
            cfg.mqtt.discoveryPrefix = "homeassistant";
            cfg.mqtt.publishIntervalMs = 60000; // 1 minute

            cfg.ota.enabled = false;
            cfg.ota.password = "";

            cfg.auth.enabled = false;
            cfg.auth.username = "admin";
            cfg.auth.password = "";
        }

        void defaultTime(Config &cfg)
        {
            cfg.ntp.enabled = true;
            cfg.ntp.server1 = "pool.ntp.org";
            cfg.ntp.server2 = "time.nist.gov";
            cfg.ntp.timezone = "UTC0";       // POSIX format
            cfg.ntp.syncIntervalMs = 600000; // 10 minutes

            cfg.gps.enabled = false;
            cfg.gps.rxPin = 17;
            cfg.gps.txPin = 16;
            cfg.gps.baudRate = 9600;

            cfg.primaryTimeSource = TimeSource::NTP;
            cfg.secondaryTimeSource = TimeSource::GPS;

            cfg.location.set = false;
            cfg.location.latitude = 0.0;
            cfg.location.longitude = 0.0;
            cfg.location.showSunMoon = true;
        }

        void defaultRain(Config &cfg)
        {
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
        }

        void defaultSensors(Config &cfg)
        {
            cfg.sensor.readIntervalMs = 5000; // 5 seconds
            cfg.sensor.i2cSDA = 21;
            cfg.sensor.i2cSCL = 22;
            cfg.sensor.i2cFrequency = 100000; // 100kHz

            cfg.skyAveraging.windowSeconds = 90;

            cfg.skyCalibration.enabled = false;
            cfg.skyCalibration.sqmOffset = 0.0F;
            cfg.skyCalibration.darkVisibleOffset = 0.0F;
            cfg.skyCalibration.darkFullOffset = 0.0F;
            cfg.skyCalibration.darkIrOffset = 0.0F;
            cfg.skyCalibration.darkSampleCount = 0;
            cfg.skyCalibration.darkCalibratedAt = 0;

            cfg.cloudDetection.clearSkyThreshold = -13.0f;
            cfg.cloudDetection.cloudyThreshold = -3.0f;
            cfg.cloudDetection.humidityCorrection = 0.75f;

            cfg.wind.enabled = false;
            cfg.wind.speedPin = 27;
            cfg.wind.directionEnabled = false;
            cfg.wind.directionPin = 35;
            cfg.wind.kmhPerHz = 2.4f;
            cfg.wind.directionOffsetDeg = 0.0f;
            cfg.wind.vanePullupOhms = 10000.0f;

            cfg.ble.enabled = false;
        }

        void defaultAlpaca(AlpacaConfig &alpaca)
        {
            alpaca.enabled = false;
            alpaca.manualOverrideUnsafe = false;
            alpaca.staleAfterSeconds = 30;
            alpaca.cloudCoverEnabled = true;
            alpaca.cloudCoverUnsafePercent = 90.0f;
            alpaca.sqmMinEnabled = false;
            alpaca.sqmMinSafe = 0.0f;
            alpaca.humidityMaxEnabled = false;
            alpaca.humidityMaxSafe = 100.0f;
            alpaca.dewpointMarginEnabled = false;
            alpaca.dewpointMarginMinC = 0.0f;
            alpaca.rainUnsafeEnabled = true;
            alpaca.rainSensorRequired = true;
            alpaca.safeDelaySeconds = 0;
            alpaca.windSpeedUnsafeEnabled = false;
            alpaca.windSpeedUnsafeMs = 10.0f;
            alpaca.windGustUnsafeEnabled = false;
            alpaca.windGustUnsafeMs = 15.0f;
        }

        void defaultAlerts(AlertsConfig &a)
        {
            a.enabled = false;
            a.unsafe = {3, "", "", ""};
            a.safe = {2, "", "", ""};
            a.rainStarted = {4, "", "", ""};
            a.rainStopped = {2, "", "", ""};
            a.sensorFault = {4, "", "", ""};
            a.sensorRecovered = {1, "", "", ""};
            a.dewRisk = {0, "", "", ""};
            a.clearSky = {0, "", "", ""};
            a.cloudedOver = {0, "", "", ""};
            a.clientLost = {3, "", "", ""};
            a.clientBack = {1, "", "", ""};
            a.clientDisconnected = {0, "", "", ""};
            a.sendMode = AlertsConfig::SendMode::Any;
            a.clientSilentSafetySeconds = 120;
            a.clientSilentWeatherSeconds = 600;
            a.dewRiskMarginC = 2.0f;
            a.clearSkyCloudPercent = 20.0f;
            a.cloudedOverCloudPercent = 70.0f;
            a.skyNightOnly = true;
            a.safetyNightOnly = true;
            a.armWithAlpaca = false;
            a.nightSunAltitudeDeg = -12.0f;
            a.cooldownSeconds = 300;
            a.pushoverEnabled = false;
            a.ntfyEnabled = false;
            a.ntfyServer = "https://ntfy.sh";
            a.webhookEnabled = false;
            a.webhookInsecureTls = false;
            a.mqttEnabled = false;
        }
    } // namespace

    Config Config::createDefault()
    {
        Config cfg;
        cfg.deviceName = "SQM-ESP32";
        std::strncpy(cfg.language, Language::ENGLISH, sizeof(cfg.language) - 1);
        defaultNetwork(cfg);
        defaultTime(cfg);
        defaultRain(cfg);
        defaultSensors(cfg);
        defaultAlpaca(cfg.alpaca);
        defaultAlerts(cfg.alerts);
        return cfg;
    }

    std::optional<Config> Config::fromJson(const std::string &json, const Config *baseConfig, std::string *error)
    {
        std::optional<Config> cfg(baseConfig != nullptr ? *baseConfig : createDefault());
        if (!applyJson(json, *cfg, baseConfig != nullptr, error))
            return std::nullopt;
        return cfg;
    }
} // namespace SQM
