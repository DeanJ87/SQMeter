#include "Config.h"
#include "ConfigInternal.h"
#include <ArduinoJson.h>
#include <cctype>
#include <cstring>

// Settings as JSON, in: applyJson(). One function per section; a key that's
// missing leaves the setting as it was.

namespace SQM
{
    using namespace ConfigDetail;

    namespace
    {
        bool isPlaceholderSecret(const char *value)
        {
            return value == nullptr || value[0] == '\0' || std::strcmp(value, SECRET_MASK) == 0 || std::strcmp(value, "***") == 0;
        }

        void assignSecret(JsonObject obj, const char *key, std::string &target, bool preservePlaceholders)
        {
            if (!obj.containsKey(key))
                return;
            JsonVariant value = obj[key];
            if (value.isNull())
            {
                target.clear();
                return;
            }
            const char *secret = value | "";
            if (preservePlaceholders && isPlaceholderSecret(secret))
                return;
            target = secret;
        }

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

        void normalizeTimeSources(Config &cfg)
        {
            if (!cfg.ntp.enabled && !cfg.gps.enabled)
                return;
            if (!isTimeSourceEnabled(cfg, cfg.primaryTimeSource))
                cfg.primaryTimeSource = cfg.ntp.enabled ? TimeSource::NTP : TimeSource::GPS;
            if (!isTimeSourceEnabled(cfg, cfg.secondaryTimeSource))
                cfg.secondaryTimeSource = cfg.gps.enabled && cfg.primaryTimeSource != TimeSource::GPS ? TimeSource::GPS : TimeSource::NTP;
            if (cfg.ntp.enabled && cfg.gps.enabled && cfg.primaryTimeSource == cfg.secondaryTimeSource)
                cfg.secondaryTimeSource = cfg.primaryTimeSource == TimeSource::NTP ? TimeSource::GPS : TimeSource::NTP;
            if (!cfg.ntp.enabled || !cfg.gps.enabled)
                cfg.secondaryTimeSource = cfg.primaryTimeSource;
        }

        bool applyRoot(JsonObject doc, Config &cfg, std::string *error)
        {
            take(doc, "deviceName", cfg.deviceName, "SQM-ESP32");
            if (doc.containsKey("language"))
            {
                const char *language = doc["language"] | "";
                if (std::strlen(language) >= sizeof(cfg.language))
                    return setError(error, "Language: not a supported language");
                std::strncpy(cfg.language, language, sizeof(cfg.language) - 1);
                cfg.language[sizeof(cfg.language) - 1] = '\0';
            }
            if (doc.containsKey("primaryTimeSource"))
                cfg.primaryTimeSource = static_cast<TimeSource>(doc["primaryTimeSource"] | 0); // 0 = NTP
            if (doc.containsKey("secondaryTimeSource"))
                cfg.secondaryTimeSource = static_cast<TimeSource>(doc["secondaryTimeSource"] | 1); // 1 = GPS
            return true;
        }

        void applyWifi(JsonObject wifi, WiFiConfig &w, bool keepSecrets)
        {
            take(wifi, "ssid", w.ssid, "");
            assignSecret(wifi, "password", w.password, keepSecrets);
            take(wifi, "hostname", w.hostname, "sqmeter");
            take(wifi, "mdns", w.mdns, true);
            take(wifi, "ipv6", w.ipv6, true);
            take(wifi, "autoReconnect", w.autoReconnect, true);
            take(wifi, "reconnectDelayMs", w.reconnectDelayMs, 1000);
            take(wifi, "maxReconnectDelayMs", w.maxReconnectDelayMs, 300000);
        }

        void applyMqtt(JsonObject mqtt, MQTTConfig &m, bool keepSecrets)
        {
            take(mqtt, "enabled", m.enabled, false);
            take(mqtt, "broker", m.broker, "");
            take(mqtt, "port", m.port, 1883);
            take(mqtt, "username", m.username, "");
            assignSecret(mqtt, "password", m.password, keepSecrets);
            take(mqtt, "topic", m.topic, "sqmeter");
            JsonObject publish = mqtt["publish"];
            take(publish, "sky", m.publish.sky, m.publish.sky);
            take(publish, "environment", m.publish.environment, m.publish.environment);
            take(publish, "clouds", m.publish.clouds, m.publish.clouds);
            take(publish, "gps", m.publish.gps, m.publish.gps);
            take(publish, "rain", m.publish.rain, m.publish.rain);
            take(publish, "wind", m.publish.wind, m.publish.wind);
            take(publish, "safety", m.publish.safety, m.publish.safety);
            take(publish, "diagnostics", m.publish.diagnostics, m.publish.diagnostics);
            JsonObject homeAssistant = mqtt["homeAssistant"];
            take(homeAssistant, "enabled", m.homeAssistant, false);
            take(homeAssistant, "discoveryPrefix", m.discoveryPrefix, "homeassistant");
            take(mqtt, "publishIntervalMs", m.publishIntervalMs, 60000);
        }

        void applyAccess(JsonObject doc, Config &cfg, bool keepSecrets)
        {
            JsonObject ota = doc["ota"];
            take(ota, "enabled", cfg.ota.enabled, false);
            assignSecret(ota, "password", cfg.ota.password, keepSecrets);

            JsonObject auth = doc["auth"];
            take(auth, "enabled", cfg.auth.enabled, false);
            take(auth, "username", cfg.auth.username, "admin");
            assignSecret(auth, "password", cfg.auth.password, keepSecrets);
        }

        void applyTime(JsonObject doc, Config &cfg)
        {
            JsonObject ntp = doc["ntp"];
            take(ntp, "enabled", cfg.ntp.enabled, true);
            take(ntp, "server1", cfg.ntp.server1, "pool.ntp.org");
            take(ntp, "server2", cfg.ntp.server2, "time.nist.gov");
            take(ntp, "timezone", cfg.ntp.timezone, "UTC0");
            take(ntp, "syncIntervalMs", cfg.ntp.syncIntervalMs, 3600000);

            JsonObject gps = doc["gps"];
            take(gps, "enabled", cfg.gps.enabled, false);
            take(gps, "rxPin", cfg.gps.rxPin, 17);
            take(gps, "txPin", cfg.gps.txPin, 16);
            take(gps, "baudRate", cfg.gps.baudRate, 9600);

            JsonObject location = doc["location"];
            take(location, "set", cfg.location.set, false);
            take(location, "latitude", cfg.location.latitude, 0.0);
            take(location, "longitude", cfg.location.longitude, 0.0);
            take(location, "showSunMoon", cfg.location.showSunMoon, true);
        }

        void applyRain(JsonObject rain, RainConfig &r)
        {
            if (rain.isNull())
                return;
            take(rain, "enabled", r.enabled, false);
            take(rain, "rxPin", r.rxPin, 18);
            take(rain, "txPin", r.txPin, 19);
            take(rain, "baudRate", r.baudRate, 9600);
            take(rain, "debugUart", r.debugUart, false);
            take(rain, "mode", r.mode, "polling");
            if (r.mode != "polling")
                r.mode = "polling";
            take(rain, "resolution", r.resolution, "high");
            take(rain, "units", r.units, "metric");
            take(rain, "pollIntervalMs", r.pollIntervalMs, 5000);
            take(rain, "rainClearDelayMs", r.rainClearDelayMs, 15UL * 60UL * 1000UL);
            take(rain, "dailyResetEnabled", r.dailyResetEnabled, false);
            take(rain, "dailyResetHour", r.dailyResetHour, 0);
            take(rain, "dailyResetMinute", r.dailyResetMinute, 0);
        }

        void applySky(JsonObject doc, Config &cfg)
        {
            JsonObject sensor = doc["sensor"];
            take(sensor, "readIntervalMs", cfg.sensor.readIntervalMs, 5000);
            take(sensor, "i2cSDA", cfg.sensor.i2cSDA, 21);
            take(sensor, "i2cSCL", cfg.sensor.i2cSCL, 22);
            take(sensor, "i2cFrequency", cfg.sensor.i2cFrequency, 100000);

            take(doc["skyAveraging"].as<JsonObject>(), "windowSeconds", cfg.skyAveraging.windowSeconds, 90);

            JsonObject cal = doc["skyCalibration"];
            take(cal, "enabled", cfg.skyCalibration.enabled, false);
            take(cal, "sqmOffset", cfg.skyCalibration.sqmOffset, 0.0F);
            take(cal, "darkVisibleOffset", cfg.skyCalibration.darkVisibleOffset, 0.0F);
            take(cal, "darkFullOffset", cfg.skyCalibration.darkFullOffset, 0.0F);
            take(cal, "darkIrOffset", cfg.skyCalibration.darkIrOffset, 0.0F);
            take(cal, "darkSampleCount", cfg.skyCalibration.darkSampleCount, 0);
            take(cal, "darkCalibratedAt", cfg.skyCalibration.darkCalibratedAt, 0);

            JsonObject cloud = doc["cloudDetection"];
            take(cloud, "clearSkyThreshold", cfg.cloudDetection.clearSkyThreshold, -13.0f);
            take(cloud, "cloudyThreshold", cfg.cloudDetection.cloudyThreshold, -3.0f);
            take(cloud, "humidityCorrection", cfg.cloudDetection.humidityCorrection, 0.75f);
        }

        void applyAlpaca(JsonObject alpaca, AlpacaConfig &a)
        {
            take(alpaca, "enabled", a.enabled, false);
            take(alpaca, "manualOverrideUnsafe", a.manualOverrideUnsafe, false);
            take(alpaca, "staleAfterSeconds", a.staleAfterSeconds, 30);
            take(alpaca, "cloudCoverEnabled", a.cloudCoverEnabled, true);
            take(alpaca, "cloudCoverUnsafePercent", a.cloudCoverUnsafePercent, 90.0f);
            take(alpaca, "sqmMinEnabled", a.sqmMinEnabled, false);
            take(alpaca, "sqmMinSafe", a.sqmMinSafe, 0.0f);
            take(alpaca, "humidityMaxEnabled", a.humidityMaxEnabled, false);
            take(alpaca, "humidityMaxSafe", a.humidityMaxSafe, 100.0f);
            take(alpaca, "dewpointMarginEnabled", a.dewpointMarginEnabled, false);
            take(alpaca, "dewpointMarginMinC", a.dewpointMarginMinC, 0.0f);
            take(alpaca, "rainUnsafeEnabled", a.rainUnsafeEnabled, true);
            take(alpaca, "rainSensorRequired", a.rainSensorRequired, true);
            take(alpaca, "safeDelaySeconds", a.safeDelaySeconds, 0U);
            take(alpaca, "windSpeedUnsafeEnabled", a.windSpeedUnsafeEnabled, false);
            take(alpaca, "windSpeedUnsafeMs", a.windSpeedUnsafeMs, 10.0f);
            take(alpaca, "windGustUnsafeEnabled", a.windGustUnsafeEnabled, false);
            take(alpaca, "windGustUnsafeMs", a.windGustUnsafeMs, 15.0f);
        }

        // Applies an "events" object ({"rain_started": {"level": 4, ...}, ...}).
        void applyEvents(JsonObject events, AlertsConfig &a)
        {
            for (const auto &entry : eventSettings(a))
            {
                JsonObject event = events[entry.first];
                AlertsConfig::EventSetting &target = *entry.second;
                take(event, "level", target.level, target.level);
                take(event, "sound", target.sound, "");
                take(event, "title", target.title, "");
                take(event, "message", target.message, "");
            }
        }

        // Settings saved before per-event levels: on/off toggles.
        void applyLegacyToggles(JsonObject alerts, AlertsConfig &a)
        {
            auto off = [&alerts](const char *key) { return alerts.containsKey(key) && !(alerts[key] | true); };
            auto on = [&alerts](const char *key) { return alerts[key] | false; };
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
            take(obj, "clientSilentSafetySeconds", a.clientSilentSafetySeconds, 120U);
            take(obj, "clientSilentWeatherSeconds", a.clientSilentWeatherSeconds, 600U);
        }

        void applyAlertChannels(JsonObject alerts, AlertsConfig &a, bool keepSecrets)
        {
            JsonObject pushover = alerts["pushover"];
            if (!pushover.isNull())
            {
                take(pushover, "enabled", a.pushoverEnabled, false);
                assignSecret(pushover, "userKey", a.pushoverUserKey, keepSecrets);
                assignSecret(pushover, "appToken", a.pushoverAppToken, keepSecrets);
                // Pasted keys often carry a stray space or newline, which
                // Pushover rejects as "not a valid user".
                trimInPlace(a.pushoverUserKey);
                trimInPlace(a.pushoverAppToken);
                take(pushover, "sound", a.pushoverSound, "");
            }

            JsonObject ntfy = alerts["ntfy"];
            take(ntfy, "enabled", a.ntfyEnabled, false);
            take(ntfy, "server", a.ntfyServer, "https://ntfy.sh");
            take(ntfy, "topic", a.ntfyTopic, "");
            assignSecret(ntfy, "token", a.ntfyToken, keepSecrets);

            JsonObject webhook = alerts["webhook"];
            take(webhook, "enabled", a.webhookEnabled, false);
            take(webhook, "url", a.webhookUrl, "");
            assignSecret(webhook, "authHeader", a.webhookAuthHeader, keepSecrets);
            take(webhook, "insecureTls", a.webhookInsecureTls, false);

            take(alerts["mqtt"].as<JsonObject>(), "enabled", a.mqttEnabled, false);
        }

        bool applyAlerts(JsonObject alerts, AlertsConfig &a, bool keepSecrets, std::string *error)
        {
            if (alerts.isNull())
                return true;
            take(alerts, "enabled", a.enabled, false);
            JsonObject events = alerts["events"];
            if (!events.isNull())
                applyEvents(events, a);
            else
                applyLegacyToggles(alerts, a);
            take(alerts, "dewRiskMarginC", a.dewRiskMarginC, 2.0f);
            take(alerts, "clearSkyCloudPercent", a.clearSkyCloudPercent, 20.0f);
            take(alerts, "cloudedOverCloudPercent", a.cloudedOverCloudPercent, 70.0f);
            take(alerts, "skyNightOnly", a.skyNightOnly, true);
            take(alerts, "safetyNightOnly", a.safetyNightOnly, true);
            if (!applySendMode(alerts, a, error))
                return false;
            applyClientSilence(alerts, a);
            take(alerts, "nightSunAltitudeDeg", a.nightSunAltitudeDeg, -12.0f);
            take(alerts, "cooldownSeconds", a.cooldownSeconds, 300U);
            applyAlertChannels(alerts, a, keepSecrets);
            return true;
        }

        // The imaging-app alert settings as stored under their own NVS key
        // (ConfigStore splices them in as "alertsClient").
        bool applyAlertsClient(JsonObject client, AlertsConfig &a, std::string *error)
        {
            if (client.isNull())
                return true;
            JsonObject events = client["events"];
            if (!events.isNull())
                applyEvents(events, a);
            if (!applySendMode(client, a, error))
                return false;
            applyClientSilence(client, a);
            return true;
        }

        void applyHardware(JsonObject doc, Config &cfg, bool keepSecrets)
        {
            JsonObject wind = doc["wind"];
            take(wind, "enabled", cfg.wind.enabled, false);
            take(wind, "speedPin", cfg.wind.speedPin, 27);
            take(wind, "directionEnabled", cfg.wind.directionEnabled, false);
            take(wind, "directionPin", cfg.wind.directionPin, 35);
            take(wind, "kmhPerHz", cfg.wind.kmhPerHz, 2.4f);
            take(wind, "directionOffsetDeg", cfg.wind.directionOffsetDeg, 0.0f);
            take(wind, "vanePullupOhms", cfg.wind.vanePullupOhms, 10000.0f);

            JsonObject ble = doc["ble"];
            take(ble, "enabled", cfg.ble.enabled, false);
            assignSecret(ble, "passkey", cfg.ble.passkey, keepSecrets);
        }

        bool isPushoverKey(const std::string &key)
        {
            if (key.size() != 30)
                return false;
            for (char c : key)
                if (!std::isalnum(static_cast<unsigned char>(c)))
                    return false;
            return true;
        }

        // Rules only enforced for changes coming from the UI/API, so a value
        // stored by older firmware never stops the config from loading: host
        // and URL forms (IPv6 literals, spec 015) and Pushover key format.
        bool validateIncoming(const Config &cfg, std::string *error)
        {
            if (!validateAddresses(cfg, error))
                return false;
            if (!cfg.alerts.pushoverEnabled)
                return true;
            if (!isPushoverKey(cfg.alerts.pushoverUserKey))
                return setError(error, "Pushover user key must be 30 letters and digits");
            if (!isPushoverKey(cfg.alerts.pushoverAppToken))
                return setError(error, "Pushover app token must be 30 letters and digits");
            return true;
        }
    } // namespace

    bool Config::applyJson(const std::string &json, Config &cfg, bool preserveSecretPlaceholders, std::string *errorOut)
    {
        // Parsing copies every string; custom alert texts can make the JSON
        // bigger than the old fixed 8 KB.
        DynamicJsonDocument doc(json.size() + 6144 > 8192 ? json.size() + 6144 : 8192);
        const DeserializationError error = deserializeJson(doc, json);
        if (error)
            return setError(errorOut, std::string("Settings JSON couldn't be read: ") + error.c_str());
        if (!checkWholeNumbers(doc, errorOut))
            return false;

        JsonObject root = doc.as<JsonObject>();
        const bool keep = preserveSecretPlaceholders;
        if (!applyRoot(root, cfg, errorOut))
            return false;
        applyWifi(root["wifi"], cfg.wifi, keep);
        applyMqtt(root["mqtt"], cfg.mqtt, keep);
        applyAccess(root, cfg, keep);
        applyTime(root, cfg);
        applyRain(root["rain"], cfg.rain);
        applySky(root, cfg);
        applyAlpaca(root["alpaca"], cfg.alpaca);
        if (!applyAlerts(root["alerts"], cfg.alerts, keep, errorOut) || !applyAlertsClient(root["alertsClient"], cfg.alerts, errorOut))
            return false;
        cfg.alerts.armWithAlpaca = cfg.alerts.sendMode == AlertsConfig::SendMode::WhileConnected;
        applyHardware(root, cfg, keep);

        normalizeTimeSources(cfg);
        if (!cfg.validate(errorOut))
            return false;
        return !preserveSecretPlaceholders || validateIncoming(cfg, errorOut);
    }
} // namespace SQM
