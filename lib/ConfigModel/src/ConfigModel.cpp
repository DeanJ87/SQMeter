#include "Config.h"
#include "BleAlarm.h"
#include "NetAddress.h"
#include <ArduinoJson.h>
#include <array>
#include <cctype>
#include <cmath>
#include <cstring>

namespace SQM
{
    static const char *SECRET_MASK = "********";

    namespace
    {
        void trimInPlace(std::string &value)
        {
            const size_t start = value.find_first_not_of(" \t\r\n");
            if (start == std::string::npos)
            {
                value.clear();
                return;
            }
            value = value.substr(start, value.find_last_not_of(" \t\r\n") - start + 1);
        }

        bool isPlaceholderSecret(const char *value)
        {
            return value == nullptr || value[0] == '\0' || std::strcmp(value, SECRET_MASK) == 0 || std::strcmp(value, "***") == 0;
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
    } // namespace

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
            return baudRate == 2400 || baudRate == 4800 || baudRate == 9600 || baudRate == 19200 || baudRate == 38400 ||
                   baudRate == 57600 || baudRate == 115200;
        }

        bool setError(std::string *error, const std::string &message)
        {
            if (error)
            {
                *error = message;
            }
            return false;
        }

        // MQTT broker and webhook URL forms, IPv6 literals included (spec 015);
        // web/src/validation/configSchema.ts applies the same rules.
        bool validateAddresses(const Config &cfg, std::string *error)
        {
            if (!cfg.mqtt.broker.empty())
            {
                Net::Host host;
                const Net::HostError hostError = Net::parseHost(cfg.mqtt.broker, host);
                if (hostError != Net::HostError::None)
                    return setError(error, std::string("MQTT broker: ") + Net::hostErrorText(hostError));
            }
            const std::pair<bool, std::pair<const char *, const std::string *>> urls[] = {
                {cfg.alerts.webhookEnabled, {"Alerts: webhook URL: ", &cfg.alerts.webhookUrl}},
                {cfg.alerts.ntfyEnabled, {"Alerts: ntfy server: ", &cfg.alerts.ntfyServer}},
            };
            for (const auto &entry : urls)
            {
                if (!entry.first)
                    continue;
                Net::HttpUrl url;
                Net::HostError hostError = Net::HostError::None;
                const Net::UrlError urlError = Net::parseHttpUrl(*entry.second.second, url, &hostError);
                if (urlError != Net::UrlError::None)
                    return setError(error, entry.second.first + Net::urlErrorText(urlError, hostError));
            }
            for (const std::string *server : {&cfg.ntp.server1, &cfg.ntp.server2})
            {
                if (Net::isIpv6Literal(*server))
                    return setError(error, std::string("NTP server: ") + Net::NTP_IPV6_TEXT);
            }
            return true;
        }

        bool inRange(float value, float min, float max)
        {
            return std::isfinite(value) && value >= min && value <= max;
        }

        // Settings stored as unsigned integers. ArduinoJson turns -1 or 1.5
        // into the default instead of failing, so reject them up front, as
        // the web UI does (test/fixtures/config-ranges.json).
        bool checkWholeNumbers(const JsonDocument &doc, std::string *error)
        {
            static const char *const FIELDS[][2] = {
                {"wifi", "reconnectDelayMs"},
                {"wifi", "maxReconnectDelayMs"},
                {"mqtt", "port"},
                {"mqtt", "publishIntervalMs"},
                {"ntp", "syncIntervalMs"},
                {"sensor", "readIntervalMs"},
                {"sensor", "i2cFrequency"},
                {"skyAveraging", "windowSeconds"},
                {"alpaca", "staleAfterSeconds"},
                {"alpaca", "safeDelaySeconds"},
                {"rain", "pollIntervalMs"},
                {"rain", "rainClearDelayMs"},
                {"rain", "dailyResetHour"},
                {"rain", "dailyResetMinute"},
                {"alerts", "cooldownSeconds"},
                {"alerts", "clientSilentSafetySeconds"},
                {"alerts", "clientSilentWeatherSeconds"},
            };
            for (const auto &field : FIELDS)
            {
                JsonVariantConst value = doc[field[0]][field[1]];
                if (value.isNull())
                    continue;
                const bool port = std::strcmp(field[1], "port") == 0;
                if (port ? value.is<uint16_t>() : value.is<uint32_t>())
                    continue;
                return setError(error, std::string(field[0]) + "." + field[1] + " must be a whole number in range");
            }
            return true;
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
                cfg.secondaryTimeSource = cfg.gps.enabled && cfg.primaryTimeSource != TimeSource::GPS ? TimeSource::GPS : TimeSource::NTP;
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

    Config Config::createDefault()
    {
        Config cfg;

        cfg.deviceName = "SQM-ESP32";

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

        cfg.ntp.enabled = true;
        cfg.ntp.server1 = "pool.ntp.org";
        cfg.ntp.server2 = "time.nist.gov";
        cfg.ntp.timezone = "UTC0";       // POSIX format
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

        cfg.skyAveraging.windowSeconds = 90;

        cfg.skyCalibration.enabled = false;
        cfg.skyCalibration.sqmOffset = 0.0F;
        cfg.skyCalibration.darkVisibleOffset = 0.0F;
        cfg.skyCalibration.darkFullOffset = 0.0F;
        cfg.skyCalibration.darkIrOffset = 0.0F;
        cfg.skyCalibration.darkSampleCount = 0;
        cfg.skyCalibration.darkCalibratedAt = 0;

        cfg.primaryTimeSource = TimeSource::NTP;
        cfg.secondaryTimeSource = TimeSource::GPS;

        cfg.cloudDetection.clearSkyThreshold = -13.0f;
        cfg.cloudDetection.cloudyThreshold = -3.0f;
        cfg.cloudDetection.humidityCorrection = 0.75f;

        cfg.alpaca.enabled = false;
        cfg.alpaca.manualOverrideUnsafe = false;
        cfg.alpaca.staleAfterSeconds = 30;
        cfg.alpaca.cloudCoverEnabled = true;
        cfg.alpaca.cloudCoverUnsafePercent = 90.0f;
        cfg.alpaca.sqmMinEnabled = false;
        cfg.alpaca.sqmMinSafe = 0.0f;
        cfg.alpaca.humidityMaxEnabled = false;
        cfg.alpaca.humidityMaxSafe = 100.0f;
        cfg.alpaca.dewpointMarginEnabled = false;
        cfg.alpaca.dewpointMarginMinC = 0.0f;
        cfg.alpaca.rainUnsafeEnabled = true;
        cfg.alpaca.rainSensorRequired = true;
        cfg.alpaca.safeDelaySeconds = 0;

        cfg.alerts.enabled = false;
        cfg.alerts.unsafe = {3, "", "", ""};
        cfg.alerts.safe = {2, "", "", ""};
        cfg.alerts.rainStarted = {4, "", "", ""};
        cfg.alerts.rainStopped = {2, "", "", ""};
        cfg.alerts.sensorFault = {4, "", "", ""};
        cfg.alerts.sensorRecovered = {1, "", "", ""};
        cfg.alerts.dewRisk = {0, "", "", ""};
        cfg.alerts.clearSky = {0, "", "", ""};
        cfg.alerts.cloudedOver = {0, "", "", ""};
        cfg.alerts.clientLost = {3, "", "", ""};
        cfg.alerts.clientBack = {1, "", "", ""};
        cfg.alerts.clientDisconnected = {0, "", "", ""};
        cfg.alerts.sendMode = AlertsConfig::SendMode::Any;
        cfg.alerts.clientSilentSafetySeconds = 120;
        cfg.alerts.clientSilentWeatherSeconds = 600;
        cfg.alerts.dewRiskMarginC = 2.0f;
        cfg.alerts.clearSkyCloudPercent = 20.0f;
        cfg.alerts.cloudedOverCloudPercent = 70.0f;
        cfg.alerts.skyNightOnly = true;
        cfg.alerts.safetyNightOnly = true;
        cfg.alerts.armWithAlpaca = false;
        cfg.alerts.nightSunAltitudeDeg = -12.0f;
        cfg.alerts.cooldownSeconds = 300;
        cfg.alerts.pushoverEnabled = false;
        cfg.alerts.ntfyEnabled = false;
        cfg.alerts.ntfyServer = "https://ntfy.sh";
        cfg.alerts.webhookEnabled = false;
        cfg.alerts.webhookInsecureTls = false;
        cfg.alerts.mqttEnabled = false;

        cfg.ble.enabled = false;

        cfg.location.set = false;
        cfg.location.latitude = 0.0;
        cfg.location.longitude = 0.0;
        cfg.location.showSunMoon = true;

        cfg.wind.enabled = false;
        cfg.wind.speedPin = 27;
        cfg.wind.directionEnabled = false;
        cfg.wind.directionPin = 35;
        cfg.wind.kmhPerHz = 2.4f;
        cfg.wind.directionOffsetDeg = 0.0f;
        cfg.wind.vanePullupOhms = 10000.0f;
        cfg.alpaca.windSpeedUnsafeEnabled = false;
        cfg.alpaca.windSpeedUnsafeMs = 10.0f;
        cfg.alpaca.windGustUnsafeEnabled = false;
        cfg.alpaca.windGustUnsafeMs = 15.0f;

        return cfg;
    }

    namespace
    {
        constexpr size_t EVENT_COUNT = 12;
        // Events from here on are the imaging-app events, persisted separately.
        constexpr size_t FIRST_CLIENT_EVENT = 9;

        // JSON key for each configurable event, matching the alert event names.
        template <typename Alerts, typename Setting> std::array<std::pair<const char *, Setting *>, EVENT_COUNT> eventSettingsOf(Alerts &a)
        {
            return {
                {{"unsafe", &a.unsafe},
                 {"safe", &a.safe},
                 {"rain_started", &a.rainStarted},
                 {"rain_stopped", &a.rainStopped},
                 {"sensor_fault", &a.sensorFault},
                 {"sensor_recovered", &a.sensorRecovered},
                 {"dew_risk", &a.dewRisk},
                 {"clear_sky", &a.clearSky},
                 {"clouded_over", &a.cloudedOver},
                 {"client_lost", &a.clientLost},
                 {"client_back", &a.clientBack},
                 {"client_disconnected", &a.clientDisconnected}}};
        }
        std::array<std::pair<const char *, const AlertsConfig::EventSetting *>, EVENT_COUNT> eventSettings(const AlertsConfig &a)
        {
            return eventSettingsOf<const AlertsConfig, const AlertsConfig::EventSetting>(a);
        }
        std::array<std::pair<const char *, AlertsConfig::EventSetting *>, EVENT_COUNT> eventSettings(AlertsConfig &a)
        {
            return eventSettingsOf<AlertsConfig, AlertsConfig::EventSetting>(a);
        }

        const char *sendModeName(AlertsConfig::SendMode mode)
        {
            return mode == AlertsConfig::SendMode::WhileConnected ? "whileConnected" : "any";
        }

        void appendEvents(JsonObject alerts, const AlertsConfig &a, size_t first, size_t last)
        {
            JsonObject events = alerts.createNestedObject("events");
            const auto settings = eventSettings(a);
            for (size_t i = first; i < last; ++i)
            {
                JsonObject event = events.createNestedObject(settings[i].first);
                event["level"] = settings[i].second->level;
                event["sound"] = settings[i].second->sound.c_str();
                event["title"] = settings[i].second->title.c_str();
                event["message"] = settings[i].second->message.c_str();
            }
        }

        // Applies an "events" object ({"rain_started": {"level": 4, ...}, ...}).
        void applyEvents(JsonObject events, AlertsConfig &a)
        {
            for (const auto &entry : eventSettings(a))
            {
                JsonObject event = events[entry.first];
                if (event.isNull())
                    continue;
                AlertsConfig::EventSetting &target = *entry.second;
                if (event.containsKey("level"))
                    target.level = event["level"] | target.level;
                if (event.containsKey("sound"))
                    target.sound = event["sound"] | "";
                if (event.containsKey("title"))
                    target.title = event["title"] | "";
                if (event.containsKey("message"))
                    target.message = event["message"] | "";
            }
        }

        // "sendMode", or the older "armWithAlpaca" when there's no sendMode.
        bool applySendMode(JsonObject obj, AlertsConfig &a, std::string *error)
        {
            if (obj.containsKey("sendMode"))
            {
                const std::string mode = obj["sendMode"] | "";
                if (mode == "any")
                    a.sendMode = AlertsConfig::SendMode::Any;
                else if (mode == "whileConnected")
                    a.sendMode = AlertsConfig::SendMode::WhileConnected;
                else
                    return setError(error, "Alerts: when to send must be any or whileConnected");
            }
            else if (obj.containsKey("armWithAlpaca"))
            {
                // Settings and scripts from before sendMode.
                a.sendMode = (obj["armWithAlpaca"] | false) ? AlertsConfig::SendMode::WhileConnected : AlertsConfig::SendMode::Any;
            }
            return true;
        }

        void applyClientSilence(JsonObject obj, AlertsConfig &a)
        {
            if (obj.containsKey("clientSilentSafetySeconds"))
                a.clientSilentSafetySeconds = obj["clientSilentSafetySeconds"] | 120U;
            if (obj.containsKey("clientSilentWeatherSeconds"))
                a.clientSilentWeatherSeconds = obj["clientSilentWeatherSeconds"] | 600U;
        }

        void appendAlerts(JsonObject alerts, const AlertsConfig &a, bool redactSecrets, Config::AlertsPart part)
        {
            auto secret = [redactSecrets](const std::string &value) -> const char *
            { return redactSecrets && !value.empty() ? SECRET_MASK : value.c_str(); };

            if (part == Config::AlertsPart::Client)
            {
                // sendMode is stored here too, so it adds nothing to the main
                // part (which older firmware reads; it has armWithAlpaca).
                alerts["sendMode"] = sendModeName(a.sendMode);
                appendEvents(alerts, a, FIRST_CLIENT_EVENT, EVENT_COUNT);
                alerts["clientSilentSafetySeconds"] = a.clientSilentSafetySeconds;
                alerts["clientSilentWeatherSeconds"] = a.clientSilentWeatherSeconds;
                return;
            }

            alerts["enabled"] = a.enabled;
            appendEvents(alerts, a, 0, part == Config::AlertsPart::All ? EVENT_COUNT : FIRST_CLIENT_EVENT);
            if (part == Config::AlertsPart::All)
            {
                alerts["sendMode"] = sendModeName(a.sendMode);
                alerts["clientSilentSafetySeconds"] = a.clientSilentSafetySeconds;
                alerts["clientSilentWeatherSeconds"] = a.clientSilentWeatherSeconds;
            }
            alerts["dewRiskMarginC"] = a.dewRiskMarginC;
            alerts["clearSkyCloudPercent"] = a.clearSkyCloudPercent;
            alerts["cloudedOverCloudPercent"] = a.cloudedOverCloudPercent;
            alerts["skyNightOnly"] = a.skyNightOnly;
            alerts["safetyNightOnly"] = a.safetyNightOnly;
            // Older firmware reads this instead of sendMode.
            alerts["armWithAlpaca"] = a.sendMode == AlertsConfig::SendMode::WhileConnected;
            alerts["nightSunAltitudeDeg"] = a.nightSunAltitudeDeg;
            alerts["cooldownSeconds"] = a.cooldownSeconds;

            JsonObject pushover = alerts.createNestedObject("pushover");
            pushover["enabled"] = a.pushoverEnabled;
            pushover["userKey"] = secret(a.pushoverUserKey);
            pushover["appToken"] = secret(a.pushoverAppToken);
            pushover["sound"] = a.pushoverSound.c_str();

            JsonObject ntfy = alerts.createNestedObject("ntfy");
            ntfy["enabled"] = a.ntfyEnabled;
            ntfy["server"] = a.ntfyServer.c_str();
            ntfy["topic"] = a.ntfyTopic.c_str();
            ntfy["token"] = secret(a.ntfyToken);

            JsonObject webhook = alerts.createNestedObject("webhook");
            webhook["enabled"] = a.webhookEnabled;
            webhook["url"] = a.webhookUrl.c_str();
            webhook["authHeader"] = secret(a.webhookAuthHeader);
            webhook["insecureTls"] = a.webhookInsecureTls;

            JsonObject mqtt = alerts.createNestedObject("mqtt");
            mqtt["enabled"] = a.mqttEnabled;
        }
    } // namespace

    std::string Config::alertsToJson(bool redactSecrets, AlertsPart part) const
    {
        DynamicJsonDocument doc(3072); // strings are referenced, not copied
        appendAlerts(doc.to<JsonObject>(), alerts, redactSecrets, part);
        std::string json;
        serializeJson(doc, json);
        return json;
    }

    std::string Config::toJson(bool redactSecrets, bool includeAlerts) const
    {
        DynamicJsonDocument doc(8192);

        doc["deviceName"] = deviceName;
        doc["primaryTimeSource"] = static_cast<int>(primaryTimeSource);
        doc["secondaryTimeSource"] = static_cast<int>(secondaryTimeSource);

        JsonObject wifi = doc.createNestedObject("wifi");
        wifi["ssid"] = this->wifi.ssid;
        wifi["password"] = redactSecrets && !this->wifi.password.empty() ? SECRET_MASK : this->wifi.password.c_str();
        wifi["hostname"] = this->wifi.hostname;
        wifi["mdns"] = this->wifi.mdns;
        wifi["ipv6"] = this->wifi.ipv6;
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
        JsonObject publish = mqtt.createNestedObject("publish");
        publish["sky"] = this->mqtt.publish.sky;
        publish["environment"] = this->mqtt.publish.environment;
        publish["clouds"] = this->mqtt.publish.clouds;
        publish["gps"] = this->mqtt.publish.gps;
        publish["rain"] = this->mqtt.publish.rain;
        publish["wind"] = this->mqtt.publish.wind;
        publish["safety"] = this->mqtt.publish.safety;
        publish["diagnostics"] = this->mqtt.publish.diagnostics;
        JsonObject homeAssistant = mqtt.createNestedObject("homeAssistant");
        homeAssistant["enabled"] = this->mqtt.homeAssistant;
        homeAssistant["discoveryPrefix"] = this->mqtt.discoveryPrefix;

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

        JsonObject skyAveraging = doc.createNestedObject("skyAveraging");
        skyAveraging["windowSeconds"] = this->skyAveraging.windowSeconds;

        JsonObject skyCalibration = doc.createNestedObject("skyCalibration");
        skyCalibration["enabled"] = this->skyCalibration.enabled;
        skyCalibration["sqmOffset"] = this->skyCalibration.sqmOffset;
        skyCalibration["darkVisibleOffset"] = this->skyCalibration.darkVisibleOffset;
        skyCalibration["darkFullOffset"] = this->skyCalibration.darkFullOffset;
        skyCalibration["darkIrOffset"] = this->skyCalibration.darkIrOffset;
        skyCalibration["darkSampleCount"] = this->skyCalibration.darkSampleCount;
        skyCalibration["darkCalibratedAt"] = this->skyCalibration.darkCalibratedAt;

        JsonObject cloudDetection = doc.createNestedObject("cloudDetection");
        cloudDetection["clearSkyThreshold"] = this->cloudDetection.clearSkyThreshold;
        cloudDetection["cloudyThreshold"] = this->cloudDetection.cloudyThreshold;
        cloudDetection["humidityCorrection"] = this->cloudDetection.humidityCorrection;

        JsonObject alpaca = doc.createNestedObject("alpaca");
        alpaca["enabled"] = this->alpaca.enabled;
        alpaca["manualOverrideUnsafe"] = this->alpaca.manualOverrideUnsafe;
        alpaca["staleAfterSeconds"] = this->alpaca.staleAfterSeconds;
        alpaca["cloudCoverEnabled"] = this->alpaca.cloudCoverEnabled;
        alpaca["cloudCoverUnsafePercent"] = this->alpaca.cloudCoverUnsafePercent;
        alpaca["sqmMinEnabled"] = this->alpaca.sqmMinEnabled;
        alpaca["sqmMinSafe"] = this->alpaca.sqmMinSafe;
        alpaca["humidityMaxEnabled"] = this->alpaca.humidityMaxEnabled;
        alpaca["humidityMaxSafe"] = this->alpaca.humidityMaxSafe;
        alpaca["dewpointMarginEnabled"] = this->alpaca.dewpointMarginEnabled;
        alpaca["dewpointMarginMinC"] = this->alpaca.dewpointMarginMinC;
        alpaca["rainUnsafeEnabled"] = this->alpaca.rainUnsafeEnabled;
        alpaca["rainSensorRequired"] = this->alpaca.rainSensorRequired;
        alpaca["safeDelaySeconds"] = this->alpaca.safeDelaySeconds;
        alpaca["windSpeedUnsafeEnabled"] = this->alpaca.windSpeedUnsafeEnabled;
        alpaca["windSpeedUnsafeMs"] = this->alpaca.windSpeedUnsafeMs;
        alpaca["windGustUnsafeEnabled"] = this->alpaca.windGustUnsafeEnabled;
        alpaca["windGustUnsafeMs"] = this->alpaca.windGustUnsafeMs;

        JsonObject ble = doc.createNestedObject("ble");
        ble["enabled"] = this->ble.enabled;
        ble["passkey"] = redactSecrets && !this->ble.passkey.empty() ? SECRET_MASK : this->ble.passkey.c_str();

        JsonObject location = doc.createNestedObject("location");
        location["set"] = this->location.set;
        location["latitude"] = this->location.latitude;
        location["longitude"] = this->location.longitude;
        location["showSunMoon"] = this->location.showSunMoon;

        JsonObject wind = doc.createNestedObject("wind");
        wind["enabled"] = this->wind.enabled;
        wind["speedPin"] = this->wind.speedPin;
        wind["directionEnabled"] = this->wind.directionEnabled;
        wind["directionPin"] = this->wind.directionPin;
        wind["kmhPerHz"] = this->wind.kmhPerHz;
        wind["directionOffsetDeg"] = this->wind.directionOffsetDeg;
        wind["vanePullupOhms"] = this->wind.vanePullupOhms;

        if (includeAlerts)
            appendAlerts(doc.createNestedObject("alerts"), this->alerts, redactSecrets, AlertsPart::All);

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

        if (wifi.reconnectDelayMs == 0 || wifi.maxReconnectDelayMs == 0 || wifi.reconnectDelayMs > 86400000 ||
            wifi.maxReconnectDelayMs > 86400000 || wifi.reconnectDelayMs > wifi.maxReconnectDelayMs)
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

        // Letters, digits, _ and -, with / between levels (same rule as the web UI).
        auto validTopic = [](const std::string &topic)
        {
            if (topic.empty() || topic.front() == '/' || topic.back() == '/' || topic.find("//") != std::string::npos)
                return false;
            for (char c : topic)
                if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '/'))
                    return false;
            return true;
        };
        // A DNS label, so it works as <hostname>.local (same rule as the web UI).
        auto validHostname = [](const std::string &name)
        {
            if (name.empty() || name.size() > 32 || name.front() == '-' || name.back() == '-')
                return false;
            for (char c : name)
                if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-'))
                    return false;
            return true;
        };
        if (!validHostname(wifi.hostname))
            return setError(error, "Hostname: use up to 32 letters, numbers and hyphens (not at either end)");

        if (mqtt.enabled && !validTopic(mqtt.topic))
            return setError(error, "MQTT topic: use letters, numbers, _ and -, with / between levels");
        if (mqtt.homeAssistant && !validTopic(mqtt.discoveryPrefix))
            return setError(error, "MQTT discovery prefix: use letters, numbers, _ and -, with / between levels");
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

        if (skyAveraging.windowSeconds < 10 || skyAveraging.windowSeconds > 300)
        {
            return setError(error, "Sky averaging window is invalid");
        }

        if (!std::isfinite(skyCalibration.sqmOffset) || skyCalibration.sqmOffset < -5.0F || skyCalibration.sqmOffset > 5.0F)
        {
            return setError(error, "Sky SQM calibration offset is invalid");
        }

        if (!std::isfinite(skyCalibration.darkVisibleOffset) || !std::isfinite(skyCalibration.darkFullOffset) ||
            !std::isfinite(skyCalibration.darkIrOffset) || skyCalibration.darkVisibleOffset < 0.0F ||
            skyCalibration.darkFullOffset < 0.0F || skyCalibration.darkIrOffset < 0.0F)
        {
            return setError(error, "Sky dark calibration offsets are invalid");
        }

        // Same ranges as the web UI (test/fixtures/config-ranges.json).
        if (!inRange(cloudDetection.clearSkyThreshold, -30.0F, 0.0F))
            return setError(error, "Cloud detection: clear-sky threshold must be between -30 and 0 degrees C");
        if (!inRange(cloudDetection.cloudyThreshold, -20.0F, 10.0F))
            return setError(error, "Cloud detection: cloudy threshold must be between -20 and 10 degrees C");
        if (!inRange(cloudDetection.humidityCorrection, 0.0F, 2.0F))
            return setError(error, "Cloud detection: humidity correction must be between 0 and 2");

        if (cloudDetection.clearSkyThreshold >= cloudDetection.cloudyThreshold)
        {
            return setError(error, "Cloud detection: clear-sky threshold must be less than cloudy threshold");
        }

        if (alpaca.staleAfterSeconds < 1 || alpaca.staleAfterSeconds > 3600)
        {
            return setError(error, "Alpaca: stale data threshold must be between 1 and 3600 seconds");
        }

        if (!std::isfinite(alpaca.cloudCoverUnsafePercent) || alpaca.cloudCoverUnsafePercent < 0.0F ||
            alpaca.cloudCoverUnsafePercent > 100.0F)
        {
            return setError(error, "Alpaca: cloud cover threshold must be between 0 and 100 percent");
        }

        if (!std::isfinite(alpaca.sqmMinSafe) || alpaca.sqmMinSafe < 0.0F || alpaca.sqmMinSafe > 30.0F)
        {
            return setError(error, "Alpaca: minimum SQM threshold must be between 0 and 30");
        }

        if (!std::isfinite(alpaca.humidityMaxSafe) || alpaca.humidityMaxSafe < 0.0F || alpaca.humidityMaxSafe > 100.0F)
        {
            return setError(error, "Alpaca: maximum humidity threshold must be between 0 and 100 percent");
        }

        if (!std::isfinite(alpaca.dewpointMarginMinC) || alpaca.dewpointMarginMinC < 0.0F || alpaca.dewpointMarginMinC > 20.0F)
        {
            return setError(error, "Alpaca: dewpoint margin threshold must be between 0 and 20 degrees C");
        }

        if (alpaca.safeDelaySeconds > 3600)
        {
            return setError(error, "Alpaca: safe delay must be between 0 and 3600 seconds");
        }

        if (!std::isfinite(alpaca.windSpeedUnsafeMs) || alpaca.windSpeedUnsafeMs <= 0.0F || alpaca.windSpeedUnsafeMs > 60.0F)
            return setError(error, "Alpaca: wind speed threshold must be between 0 and 60 m/s");
        if (!std::isfinite(alpaca.windGustUnsafeMs) || alpaca.windGustUnsafeMs <= 0.0F || alpaca.windGustUnsafeMs > 80.0F)
            return setError(error, "Alpaca: wind gust threshold must be between 0 and 80 m/s");

        if (wind.enabled)
        {
            if (!isValidGpio(wind.speedPin))
                return setError(error, "Wind: anemometer pin is not a valid GPIO");
            if (wind.directionEnabled && (wind.directionPin < 32 || wind.directionPin > 39))
                return setError(error, "Wind: vane pin must be an ADC1 pin (GPIO 32-39)");
            const int used[] = {
                sensor.i2cSDA,
                sensor.i2cSCL,
                gps.enabled ? gps.rxPin : -1,
                gps.enabled ? gps.txPin : -1,
                rain.enabled ? rain.rxPin : -1,
                rain.enabled ? rain.txPin : -1};
            for (int pin : used)
            {
                if (pin == wind.speedPin || (wind.directionEnabled && pin == wind.directionPin))
                    return setError(error, "Wind: pin is already used by I2C, GPS or the rain sensor");
            }
            if (wind.directionEnabled && wind.directionPin == wind.speedPin)
                return setError(error, "Wind: anemometer and vane need different pins");
        }
        if (!std::isfinite(wind.kmhPerHz) || wind.kmhPerHz <= 0.0F || wind.kmhPerHz > 20.0F)
            return setError(error, "Wind: km/h per Hz must be between 0 and 20");
        if (!std::isfinite(wind.directionOffsetDeg) || wind.directionOffsetDeg < -360.0F || wind.directionOffsetDeg > 360.0F)
            return setError(error, "Wind: direction offset must be between -360 and 360 degrees");
        if (!std::isfinite(wind.vanePullupOhms) || wind.vanePullupOhms < 1000.0F || wind.vanePullupOhms > 100000.0F)
            return setError(error, "Wind: vane pull-up must be between 1k and 100k ohms");

        if (!ble.passkey.empty())
        {
            uint32_t passkey = 0;
            if (!Ble::parsePasskey(ble.passkey, passkey))
                return setError(error, "Bluetooth: pairing passkey must be 6 digits (not 000000)");
        }

        auto isHttpUrl = [](const std::string &url) { return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0; };
        if (alerts.cooldownSeconds > 86400)
            return setError(error, "Alerts: cooldown must be between 0 and 86400 seconds");
        if (!std::isfinite(alerts.dewRiskMarginC) || alerts.dewRiskMarginC < 0.0F || alerts.dewRiskMarginC > 10.0F)
            return setError(error, "Alerts: dew risk margin must be between 0 and 10 degrees C");
        if (!std::isfinite(alerts.clearSkyCloudPercent) || alerts.clearSkyCloudPercent < 0.0F || alerts.clearSkyCloudPercent > 100.0F)
            return setError(error, "Alerts: clear sky threshold must be between 0 and 100 percent");
        if (!std::isfinite(alerts.cloudedOverCloudPercent) || alerts.cloudedOverCloudPercent < 0.0F ||
            alerts.cloudedOverCloudPercent > 100.0F || alerts.cloudedOverCloudPercent <= alerts.clearSkyCloudPercent)
            return setError(error, "Alerts: the clouded-over threshold must be above the clear threshold");
        if (!std::isfinite(alerts.nightSunAltitudeDeg) || alerts.nightSunAltitudeDeg < -20.0F || alerts.nightSunAltitudeDeg > 0.0F)
            return setError(error, "Alerts: night must start with the sun between 0 and -20 degrees");
        if (location.set && (!std::isfinite(location.latitude) || std::fabs(location.latitude) > 90.0 ||
                             !std::isfinite(location.longitude) || std::fabs(location.longitude) > 180.0))
            return setError(error, "Location: latitude must be -90..90 and longitude -180..180");
        for (const auto &entry : eventSettings(alerts))
        {
            if (entry.second->level > 4)
                return setError(error, "Alerts: event levels are 0 (off) to 4 (wake me)");
            if (entry.second->title.size() > AlertsConfig::MAX_TEMPLATE_TITLE ||
                entry.second->message.size() > AlertsConfig::MAX_TEMPLATE_MESSAGE)
                return setError(error, "Alerts: custom titles are up to 80 characters and messages up to 240");
        }
        if (alerts.sendMode != AlertsConfig::SendMode::Any && alerts.sendMode != AlertsConfig::SendMode::WhileConnected)
            return setError(error, "Alerts: when to send must be any or whileConnected");
        for (uint32_t seconds : {alerts.clientSilentSafetySeconds, alerts.clientSilentWeatherSeconds})
            if (seconds < AlertsConfig::MIN_CLIENT_SILENT_SECONDS || seconds > AlertsConfig::MAX_CLIENT_SILENT_SECONDS)
                return setError(error, "Alerts: silence times must be between 30 and 3600 seconds");
        // NVS strings top out just under 4000 bytes; each stored part has its own key.
        if (alertsToJson(false, AlertsPart::Main).size() > MAX_ALERTS_JSON_BYTES ||
            alertsToJson(false, AlertsPart::Client).size() > MAX_ALERTS_JSON_BYTES)
            return setError(error, "Alerts: the custom alert texts are too long in total - shorten some");
        if (alerts.pushoverEnabled && (alerts.pushoverUserKey.empty() || alerts.pushoverAppToken.empty()))
            return setError(error, "Alerts: Pushover needs both a user key and an application token");
        if (alerts.ntfyEnabled && (!isHttpUrl(alerts.ntfyServer) || alerts.ntfyTopic.empty()))
            return setError(error, "Alerts: ntfy needs an http(s):// server and a topic");
        if (alerts.webhookEnabled && !isHttpUrl(alerts.webhookUrl))
            return setError(error, "Alerts: webhook URL must start with http:// or https://");

        return true;
    }

    std::optional<Config> Config::fromJson(const std::string &json, const Config *baseConfig, std::string *error)
    {
        std::optional<Config> cfg(baseConfig != nullptr ? *baseConfig : createDefault());
        if (!applyJson(json, *cfg, baseConfig != nullptr, error))
            return std::nullopt;
        return cfg;
    }

    bool Config::applyJson(const std::string &json, Config &cfg, bool preserveSecretPlaceholders, std::string *errorOut)
    {
        // Parsing copies every string; custom alert texts can make the JSON
        // bigger than the old fixed 8 KB.
        DynamicJsonDocument doc(json.size() + 6144 > 8192 ? json.size() + 6144 : 8192);
        DeserializationError error = deserializeJson(doc, json);

        if (error)
        {
            setError(errorOut, std::string("Settings JSON couldn't be read: ") + error.c_str());
            return false;
        }
        if (!checkWholeNumbers(doc, errorOut))
            return false;

        if (doc.containsKey("deviceName"))
            cfg.deviceName = doc["deviceName"] | "SQM-ESP32";
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
                cfg.wifi.hostname = wifi["hostname"] | "sqmeter";
            if (wifi.containsKey("mdns"))
                cfg.wifi.mdns = wifi["mdns"] | true;
            if (wifi.containsKey("ipv6"))
                cfg.wifi.ipv6 = wifi["ipv6"] | true;
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
                cfg.mqtt.topic = mqtt["topic"] | "sqmeter";
            JsonObject publish = mqtt["publish"];
            if (!publish.isNull())
            {
                auto flag = [&publish](const char *key, bool &target)
                {
                    if (publish.containsKey(key))
                        target = publish[key] | target;
                };
                flag("sky", cfg.mqtt.publish.sky);
                flag("environment", cfg.mqtt.publish.environment);
                flag("clouds", cfg.mqtt.publish.clouds);
                flag("gps", cfg.mqtt.publish.gps);
                flag("rain", cfg.mqtt.publish.rain);
                flag("wind", cfg.mqtt.publish.wind);
                flag("safety", cfg.mqtt.publish.safety);
                flag("diagnostics", cfg.mqtt.publish.diagnostics);
            }
            JsonObject homeAssistant = mqtt["homeAssistant"];
            if (!homeAssistant.isNull())
            {
                if (homeAssistant.containsKey("enabled"))
                    cfg.mqtt.homeAssistant = homeAssistant["enabled"] | false;
                if (homeAssistant.containsKey("discoveryPrefix"))
                    cfg.mqtt.discoveryPrefix = homeAssistant["discoveryPrefix"] | "homeassistant";
            }
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

        JsonObject skyAveraging = doc["skyAveraging"];
        if (!skyAveraging.isNull())
        {
            if (skyAveraging.containsKey("windowSeconds"))
                cfg.skyAveraging.windowSeconds = skyAveraging["windowSeconds"] | 90;
        }

        JsonObject skyCalibration = doc["skyCalibration"];
        if (!skyCalibration.isNull())
        {
            if (skyCalibration.containsKey("enabled"))
                cfg.skyCalibration.enabled = skyCalibration["enabled"] | false;
            if (skyCalibration.containsKey("sqmOffset"))
                cfg.skyCalibration.sqmOffset = skyCalibration["sqmOffset"] | 0.0F;
            if (skyCalibration.containsKey("darkVisibleOffset"))
                cfg.skyCalibration.darkVisibleOffset = skyCalibration["darkVisibleOffset"] | 0.0F;
            if (skyCalibration.containsKey("darkFullOffset"))
                cfg.skyCalibration.darkFullOffset = skyCalibration["darkFullOffset"] | 0.0F;
            if (skyCalibration.containsKey("darkIrOffset"))
                cfg.skyCalibration.darkIrOffset = skyCalibration["darkIrOffset"] | 0.0F;
            if (skyCalibration.containsKey("darkSampleCount"))
                cfg.skyCalibration.darkSampleCount = skyCalibration["darkSampleCount"] | 0;
            if (skyCalibration.containsKey("darkCalibratedAt"))
                cfg.skyCalibration.darkCalibratedAt = skyCalibration["darkCalibratedAt"] | 0;
        }

        JsonObject cloudDetectionObj = doc["cloudDetection"];
        if (!cloudDetectionObj.isNull())
        {
            if (cloudDetectionObj.containsKey("clearSkyThreshold"))
                cfg.cloudDetection.clearSkyThreshold = cloudDetectionObj["clearSkyThreshold"] | -13.0f;
            if (cloudDetectionObj.containsKey("cloudyThreshold"))
                cfg.cloudDetection.cloudyThreshold = cloudDetectionObj["cloudyThreshold"] | -3.0f;
            if (cloudDetectionObj.containsKey("humidityCorrection"))
                cfg.cloudDetection.humidityCorrection = cloudDetectionObj["humidityCorrection"] | 0.75f;
        }

        JsonObject alpacaObj = doc["alpaca"];
        if (!alpacaObj.isNull())
        {
            if (alpacaObj.containsKey("enabled"))
                cfg.alpaca.enabled = alpacaObj["enabled"] | false;
            if (alpacaObj.containsKey("manualOverrideUnsafe"))
                cfg.alpaca.manualOverrideUnsafe = alpacaObj["manualOverrideUnsafe"] | false;
            if (alpacaObj.containsKey("staleAfterSeconds"))
                cfg.alpaca.staleAfterSeconds = alpacaObj["staleAfterSeconds"] | 30;
            if (alpacaObj.containsKey("cloudCoverEnabled"))
                cfg.alpaca.cloudCoverEnabled = alpacaObj["cloudCoverEnabled"] | true;
            if (alpacaObj.containsKey("cloudCoverUnsafePercent"))
                cfg.alpaca.cloudCoverUnsafePercent = alpacaObj["cloudCoverUnsafePercent"] | 90.0f;
            if (alpacaObj.containsKey("sqmMinEnabled"))
                cfg.alpaca.sqmMinEnabled = alpacaObj["sqmMinEnabled"] | false;
            if (alpacaObj.containsKey("sqmMinSafe"))
                cfg.alpaca.sqmMinSafe = alpacaObj["sqmMinSafe"] | 0.0f;
            if (alpacaObj.containsKey("humidityMaxEnabled"))
                cfg.alpaca.humidityMaxEnabled = alpacaObj["humidityMaxEnabled"] | false;
            if (alpacaObj.containsKey("humidityMaxSafe"))
                cfg.alpaca.humidityMaxSafe = alpacaObj["humidityMaxSafe"] | 100.0f;
            if (alpacaObj.containsKey("dewpointMarginEnabled"))
                cfg.alpaca.dewpointMarginEnabled = alpacaObj["dewpointMarginEnabled"] | false;
            if (alpacaObj.containsKey("dewpointMarginMinC"))
                cfg.alpaca.dewpointMarginMinC = alpacaObj["dewpointMarginMinC"] | 0.0f;
            if (alpacaObj.containsKey("rainUnsafeEnabled"))
                cfg.alpaca.rainUnsafeEnabled = alpacaObj["rainUnsafeEnabled"] | true;
            if (alpacaObj.containsKey("rainSensorRequired"))
                cfg.alpaca.rainSensorRequired = alpacaObj["rainSensorRequired"] | true;
            if (alpacaObj.containsKey("safeDelaySeconds"))
                cfg.alpaca.safeDelaySeconds = alpacaObj["safeDelaySeconds"] | 0U;
            if (alpacaObj.containsKey("windSpeedUnsafeEnabled"))
                cfg.alpaca.windSpeedUnsafeEnabled = alpacaObj["windSpeedUnsafeEnabled"] | false;
            if (alpacaObj.containsKey("windSpeedUnsafeMs"))
                cfg.alpaca.windSpeedUnsafeMs = alpacaObj["windSpeedUnsafeMs"] | 10.0f;
            if (alpacaObj.containsKey("windGustUnsafeEnabled"))
                cfg.alpaca.windGustUnsafeEnabled = alpacaObj["windGustUnsafeEnabled"] | false;
            if (alpacaObj.containsKey("windGustUnsafeMs"))
                cfg.alpaca.windGustUnsafeMs = alpacaObj["windGustUnsafeMs"] | 15.0f;
        }

        JsonObject alertsObj = doc["alerts"];
        if (!alertsObj.isNull())
        {
            AlertsConfig &a = cfg.alerts;
            if (alertsObj.containsKey("enabled"))
                a.enabled = alertsObj["enabled"] | false;
            JsonObject events = alertsObj["events"];
            if (!events.isNull())
            {
                applyEvents(events, a);
            }
            else
            {
                // Settings saved before per-event levels: on/off toggles.
                auto off = [&alertsObj](const char *key) { return alertsObj.containsKey(key) && !(alertsObj[key] | true); };
                auto on = [&alertsObj](const char *key) { return alertsObj[key] | false; };
                if (off("onSafetyChange"))
                    a.unsafe.level = a.safe.level = 0;
                if (off("onRain"))
                    a.rainStarted.level = a.rainStopped.level = 0;
                if (off("onSensorFault"))
                    a.sensorFault.level = a.sensorRecovered.level = 0;
                if (on("onDewRisk"))
                    a.dewRisk.level = 2;
                if (on("onClearSky"))
                    a.clearSky.level = 2;
                if (on("onCloudedOver"))
                    a.cloudedOver.level = 3;
            }
            if (alertsObj.containsKey("dewRiskMarginC"))
                a.dewRiskMarginC = alertsObj["dewRiskMarginC"] | 2.0f;
            if (alertsObj.containsKey("clearSkyCloudPercent"))
                a.clearSkyCloudPercent = alertsObj["clearSkyCloudPercent"] | 20.0f;
            if (alertsObj.containsKey("cloudedOverCloudPercent"))
                a.cloudedOverCloudPercent = alertsObj["cloudedOverCloudPercent"] | 70.0f;
            if (alertsObj.containsKey("skyNightOnly"))
                a.skyNightOnly = alertsObj["skyNightOnly"] | true;
            if (alertsObj.containsKey("safetyNightOnly"))
                a.safetyNightOnly = alertsObj["safetyNightOnly"] | true;
            if (!applySendMode(alertsObj, a, errorOut))
                return false;
            applyClientSilence(alertsObj, a);
            if (alertsObj.containsKey("nightSunAltitudeDeg"))
                a.nightSunAltitudeDeg = alertsObj["nightSunAltitudeDeg"] | -12.0f;
            if (alertsObj.containsKey("cooldownSeconds"))
                a.cooldownSeconds = alertsObj["cooldownSeconds"] | 300U;

            JsonObject pushover = alertsObj["pushover"];
            if (!pushover.isNull())
            {
                if (pushover.containsKey("enabled"))
                    a.pushoverEnabled = pushover["enabled"] | false;
                assignSecret(pushover, "userKey", a.pushoverUserKey, preserveSecretPlaceholders);
                assignSecret(pushover, "appToken", a.pushoverAppToken, preserveSecretPlaceholders);
                // Pasted keys often carry a stray space or newline, which
                // Pushover rejects as "not a valid user".
                trimInPlace(a.pushoverUserKey);
                trimInPlace(a.pushoverAppToken);
                if (pushover.containsKey("sound"))
                    a.pushoverSound = pushover["sound"] | "";
            }

            JsonObject ntfy = alertsObj["ntfy"];
            if (!ntfy.isNull())
            {
                if (ntfy.containsKey("enabled"))
                    a.ntfyEnabled = ntfy["enabled"] | false;
                if (ntfy.containsKey("server"))
                    a.ntfyServer = ntfy["server"] | "https://ntfy.sh";
                if (ntfy.containsKey("topic"))
                    a.ntfyTopic = ntfy["topic"] | "";
                assignSecret(ntfy, "token", a.ntfyToken, preserveSecretPlaceholders);
            }

            JsonObject webhook = alertsObj["webhook"];
            if (!webhook.isNull())
            {
                if (webhook.containsKey("enabled"))
                    a.webhookEnabled = webhook["enabled"] | false;
                if (webhook.containsKey("url"))
                    a.webhookUrl = webhook["url"] | "";
                assignSecret(webhook, "authHeader", a.webhookAuthHeader, preserveSecretPlaceholders);
                if (webhook.containsKey("insecureTls"))
                    a.webhookInsecureTls = webhook["insecureTls"] | false;
            }

            JsonObject mqtt = alertsObj["mqtt"];
            if (!mqtt.isNull() && mqtt.containsKey("enabled"))
                a.mqttEnabled = mqtt["enabled"] | false;
        }

        // The imaging-app alert settings as stored under their own NVS key
        // (ConfigStore splices them in as "alertsClient").
        JsonObject alertsClientObj = doc["alertsClient"];
        if (!alertsClientObj.isNull())
        {
            JsonObject events = alertsClientObj["events"];
            if (!events.isNull())
                applyEvents(events, cfg.alerts);
            if (!applySendMode(alertsClientObj, cfg.alerts, errorOut))
                return false;
            applyClientSilence(alertsClientObj, cfg.alerts);
        }
        cfg.alerts.armWithAlpaca = cfg.alerts.sendMode == AlertsConfig::SendMode::WhileConnected;

        JsonObject locationObj = doc["location"];
        if (!locationObj.isNull())
        {
            if (locationObj.containsKey("set"))
                cfg.location.set = locationObj["set"] | false;
            if (locationObj.containsKey("latitude"))
                cfg.location.latitude = locationObj["latitude"] | 0.0;
            if (locationObj.containsKey("longitude"))
                cfg.location.longitude = locationObj["longitude"] | 0.0;
            if (locationObj.containsKey("showSunMoon"))
                cfg.location.showSunMoon = locationObj["showSunMoon"] | true;
        }

        JsonObject windObj = doc["wind"];
        if (!windObj.isNull())
        {
            if (windObj.containsKey("enabled"))
                cfg.wind.enabled = windObj["enabled"] | false;
            if (windObj.containsKey("speedPin"))
                cfg.wind.speedPin = windObj["speedPin"] | 27;
            if (windObj.containsKey("directionEnabled"))
                cfg.wind.directionEnabled = windObj["directionEnabled"] | false;
            if (windObj.containsKey("directionPin"))
                cfg.wind.directionPin = windObj["directionPin"] | 35;
            if (windObj.containsKey("kmhPerHz"))
                cfg.wind.kmhPerHz = windObj["kmhPerHz"] | 2.4f;
            if (windObj.containsKey("directionOffsetDeg"))
                cfg.wind.directionOffsetDeg = windObj["directionOffsetDeg"] | 0.0f;
            if (windObj.containsKey("vanePullupOhms"))
                cfg.wind.vanePullupOhms = windObj["vanePullupOhms"] | 10000.0f;
        }

        JsonObject bleObj = doc["ble"];
        if (!bleObj.isNull())
        {
            if (bleObj.containsKey("enabled"))
                cfg.ble.enabled = bleObj["enabled"] | false;
            assignSecret(bleObj, "passkey", cfg.ble.passkey, preserveSecretPlaceholders);
        }

        normalizeTimeSources(cfg);

        if (!cfg.validate(errorOut))
            return false;

        // Host and URL forms (IPv6 literals, spec 015) are only enforced for
        // changes coming from the UI/API, like the key format below, so a
        // value stored by older firmware never stops the config from loading.
        if (preserveSecretPlaceholders && !validateAddresses(cfg, errorOut))
            return false;

        // Key format is only enforced for changes coming from the UI/API, so a
        // key stored by older firmware never stops the config from loading.
        if (preserveSecretPlaceholders && cfg.alerts.pushoverEnabled)
        {
            auto isPushoverKey = [](const std::string &key)
            {
                if (key.size() != 30)
                    return false;
                for (char c : key)
                    if (!std::isalnum(static_cast<unsigned char>(c)))
                        return false;
                return true;
            };
            if (!isPushoverKey(cfg.alerts.pushoverUserKey))
            {
                setError(errorOut, "Pushover user key must be 30 letters and digits");
                return false;
            }
            if (!isPushoverKey(cfg.alerts.pushoverAppToken))
            {
                setError(errorOut, "Pushover app token must be 30 letters and digits");
                return false;
            }
        }

        return true;
    }

} // namespace SQM
