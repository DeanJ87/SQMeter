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

        void defaultTime(Config &cfg, const BoardProfile &board)
        {
            cfg.ntp.enabled = true;
            cfg.ntp.server1 = "pool.ntp.org";
            cfg.ntp.server2 = "time.nist.gov";
            cfg.ntp.timezone = "UTC0";       // POSIX format
            cfg.ntp.syncIntervalMs = 600000; // 10 minutes

            cfg.gps.enabled = false;
            cfg.gps.rxPin = board.gpsRx;
            cfg.gps.txPin = board.gpsTx;
            cfg.gps.baudRate = 9600;

            cfg.primaryTimeSource = TimeSource::NTP;
            cfg.secondaryTimeSource = TimeSource::GPS;

            cfg.location.set = false;
            cfg.location.latitude = 0.0;
            cfg.location.longitude = 0.0;
            cfg.location.showSunMoon = true;
        }

        void defaultRain(Config &cfg, const BoardProfile &board)
        {
            cfg.rain.enabled = false;
            cfg.rain.rxPin = board.rainRx;
            cfg.rain.txPin = board.rainTx;
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

        void defaultSensors(Config &cfg, const BoardProfile &board)
        {
            cfg.sensor.readIntervalMs = 5000; // 5 seconds
            cfg.sensor.i2cSDA = board.i2cSDA;
            cfg.sensor.i2cSCL = board.i2cSCL;
            cfg.sensor.i2cFrequency = 100000;   // 100kHz
            cfg.sensor.i2c2SDA = board.i2c2SDA; // NONE == PIN_UNSET
            cfg.sensor.i2c2SCL = board.i2c2SCL;

            cfg.sky = SkyConfig{};
            for (SkyRole role : {SkyRole::Sqm, SkyRole::Sqml})
            {
                cfg.averaging(role).windowSeconds = 90;
                SkyCalibrationConfig &cal = cfg.calibration(role);
                cal.enabled = false;
                cal.sqmOffset = 0.0F;
                cal.darkVisibleOffset = 0.0F;
                cal.darkFullOffset = 0.0F;
                cal.darkIrOffset = 0.0F;
                cal.darkSampleCount = 0;
                cal.darkCalibratedAt = 0;
            }

            cfg.cloudDetection.clearSkyThreshold = -13.0f;
            cfg.cloudDetection.cloudyThreshold = -3.0f;
            cfg.cloudDetection.humidityCorrection = 0.75f;

            cfg.wind.enabled = false;
            cfg.wind.speedPin = board.windSpeed;
            cfg.wind.directionEnabled = false;
            cfg.wind.directionPin = board.windDirection;
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
            alpaca.batteryEnabled = false;
            alpaca.batteryMinPercent = BatteryConfig{}.criticalPercent; // follows it (ConfigJsonIn)
            alpaca.batteryAlert = true;
            alpaca.batteryPhoneAlarm = true;
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
            a.batteryLow = {2, "", "", ""};
            a.batteryCritical = {4, "", "", ""};
            a.batteryShutdown = {4, "", "", ""};
            a.powerRestored = {0, "", "", ""};
            a.chargeComplete = {0, "", "", ""};
            a.chargeFault = {2, "", "", ""};
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

    static_assert(BoardProfile::NONE == SensorConfig::PIN_UNSET, "an unwired I2C-2 bus must read as unset");

    Config Config::createDefault()
    {
        return createDefault(BUILD_BOARD);
    }

    Config Config::createDefault(Board boardId)
    {
        const BoardProfile &board = boardProfile(boardId);
        Config cfg;
        cfg.deviceName = "SQM-ESP32";
        std::strncpy(cfg.language, Language::ENGLISH, sizeof(cfg.language) - 1);
        defaultNetwork(cfg);
        defaultTime(cfg, board);
        defaultRain(cfg, board);
        defaultSensors(cfg, board);
        defaultAlpaca(cfg.alpaca);
        defaultAlerts(cfg.alerts);
        cfg.battery = BatteryConfig{};
        return cfg;
    }

    std::string Config::spliceStored(
        const std::string &main, const std::string &alerts, const std::string &client, const std::string &battery)
    {
        std::string spliced = main;
        const std::pair<const char *, const std::string *> parts[] = {
            {"alerts", &alerts},
            {"alertsClient", &client},
            {"alertsBattery", &battery},
        };
        for (const auto &part : parts)
        {
            const size_t close = spliced.rfind('}');
            if (part.second->empty() || close == std::string::npos)
                continue;
            spliced.insert(close, std::string(",\"") + part.first + "\":" + *part.second);
        }
        return spliced;
    }

    std::optional<Config> Config::fromJson(const std::string &json, const Config *baseConfig, std::string *error)
    {
        std::optional<Config> cfg(baseConfig != nullptr ? *baseConfig : createDefault());
        if (!applyJson(json, *cfg, baseConfig != nullptr, error))
            return std::nullopt;
        return cfg;
    }
} // namespace SQM
