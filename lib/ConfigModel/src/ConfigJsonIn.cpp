#include "Config.h"
#include "ConfigInternal.h"
#include <ArduinoJson.h>
#include <cctype>
#include <cstring>

// Settings as JSON, in: applyJson(). One function per section; a key that's
// missing leaves the setting as it was, and a value of the wrong type is
// never swapped for a default (JsonReader in ConfigInternal.h).

namespace SQM
{
    using namespace ConfigDetail;

    namespace
    {
        bool isPlaceholderSecret(const char *value)
        {
            return value == nullptr || value[0] == '\0' || std::strcmp(value, SECRET_MASK) == 0 || std::strcmp(value, "***") == 0;
        }

        // A secret: null clears it; from the UI/API the mask (or "") keeps it.
        void assignSecret(const Section &obj, const char *key, std::string &target, bool preservePlaceholders)
        {
            if (!obj.has(key))
                return;
            JsonVariant value = obj[key];
            if (value.isNull())
            {
                target.clear();
                return;
            }
            if (!obj.check<std::string>(key))
                return;
            const char *secret = value.as<const char *>();
            if (preservePlaceholders && isPlaceholderSecret(secret))
                return;
            target = secret;
        }

        bool isBlank(const std::string &value)
        {
            return value.find_first_not_of(" \t\r\n") == std::string::npos;
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
                {"skyAveragingL", "windowSeconds"},
                {"skyCalibrationL", "darkSampleCount"},
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

        bool typeError(const JsonReader &reader, std::string *error)
        {
            const std::string &field = reader.field();
            switch (reader.expected())
            {
            case Expected::String:
                return setError(error, "Settings: " + field + " must be a string");
            case Expected::Boolean:
                return setError(error, "Settings: " + field + " must be true or false");
            case Expected::Number:
                return setError(error, "Settings: " + field + " must be a number");
            case Expected::WholeNumber:
                return setError(error, "Settings: " + field + " must be a whole number in range");
            case Expected::Object:
                break;
            }
            return setError(error, "Settings: " + field + " must be an object");
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

        void takeTimeSource(const Section &doc, const char *key, TimeSource &target)
        {
            if (doc.check<int>(key))
                target = static_cast<TimeSource>(doc[key].as<int>()); // 0 = NTP, 1 = GPS; validate() checks the range
        }

        bool applyRoot(const Section &doc, Config &cfg, std::string *error)
        {
            doc.take("deviceName", cfg.deviceName);
            if (doc.check<std::string>("language"))
            {
                const char *language = doc["language"].as<const char *>();
                if (std::strlen(language) >= sizeof(cfg.language))
                    return setError(error, "Language: not a supported language");
                std::strncpy(cfg.language, language, sizeof(cfg.language) - 1);
                cfg.language[sizeof(cfg.language) - 1] = '\0';
            }
            takeTimeSource(doc, "primaryTimeSource", cfg.primaryTimeSource);
            takeTimeSource(doc, "secondaryTimeSource", cfg.secondaryTimeSource);
            return true;
        }

        void applyWifi(const Section &wifi, WiFiConfig &w, bool keepSecrets)
        {
            wifi.take("ssid", w.ssid);
            assignSecret(wifi, "password", w.password, keepSecrets);
            wifi.take("hostname", w.hostname);
            wifi.take("mdns", w.mdns);
            wifi.take("ipv6", w.ipv6);
            wifi.take("autoReconnect", w.autoReconnect);
            wifi.take("reconnectDelayMs", w.reconnectDelayMs);
            wifi.take("maxReconnectDelayMs", w.maxReconnectDelayMs);
        }

        void applyMqtt(const Section &mqtt, MQTTConfig &m, bool keepSecrets)
        {
            mqtt.take("enabled", m.enabled);
            mqtt.take("broker", m.broker);
            mqtt.take("port", m.port);
            mqtt.take("username", m.username);
            assignSecret(mqtt, "password", m.password, keepSecrets);
            mqtt.take("topic", m.topic);
            const Section publish = mqtt.child("publish");
            publish.take("sky", m.publish.sky);
            publish.take("environment", m.publish.environment);
            publish.take("clouds", m.publish.clouds);
            publish.take("gps", m.publish.gps);
            publish.take("rain", m.publish.rain);
            publish.take("wind", m.publish.wind);
            publish.take("battery", m.publish.battery);
            publish.take("safety", m.publish.safety);
            publish.take("diagnostics", m.publish.diagnostics);
            const Section homeAssistant = mqtt.child("homeAssistant");
            homeAssistant.take("enabled", m.homeAssistant);
            homeAssistant.take("discoveryPrefix", m.discoveryPrefix);
            mqtt.take("publishIntervalMs", m.publishIntervalMs);
        }

        void applyAccess(const Section &doc, Config &cfg, bool keepSecrets)
        {
            const Section ota = doc.child("ota");
            ota.take("enabled", cfg.ota.enabled);
            assignSecret(ota, "password", cfg.ota.password, keepSecrets);

            const Section auth = doc.child("auth");
            auth.take("enabled", cfg.auth.enabled);
            auth.take("username", cfg.auth.username);
            assignSecret(auth, "password", cfg.auth.password, keepSecrets);
        }

        void applyTime(const Section &doc, Config &cfg)
        {
            const Section ntp = doc.child("ntp");
            ntp.take("enabled", cfg.ntp.enabled);
            ntp.take("server1", cfg.ntp.server1);
            ntp.take("server2", cfg.ntp.server2);
            ntp.take("timezone", cfg.ntp.timezone);
            ntp.take("syncIntervalMs", cfg.ntp.syncIntervalMs);

            const Section gps = doc.child("gps");
            gps.take("enabled", cfg.gps.enabled);
            gps.take("rxPin", cfg.gps.rxPin);
            gps.take("txPin", cfg.gps.txPin);
            gps.take("baudRate", cfg.gps.baudRate);

            const Section location = doc.child("location");
            location.take("set", cfg.location.set);
            location.take("latitude", cfg.location.latitude);
            location.take("longitude", cfg.location.longitude);
            location.take("showSunMoon", cfg.location.showSunMoon);
        }

        void applyRain(const Section &rain, RainConfig &r)
        {
            if (rain.isNull())
                return;
            rain.take("enabled", r.enabled);
            rain.take("rxPin", r.rxPin);
            rain.take("txPin", r.txPin);
            rain.take("baudRate", r.baudRate);
            rain.take("debugUart", r.debugUart);
            rain.take("mode", r.mode);
            if (r.mode != "polling")
                r.mode = "polling";
            rain.take("resolution", r.resolution);
            rain.take("units", r.units);
            rain.take("pollIntervalMs", r.pollIntervalMs);
            rain.take("rainClearDelayMs", r.rainClearDelayMs);
            rain.take("dailyResetEnabled", r.dailyResetEnabled);
            rain.take("dailyResetHour", r.dailyResetHour);
            rain.take("dailyResetMinute", r.dailyResetMinute);
        }

        void applySkySections(const Section &doc, const char *averagingKey, const char *calibrationKey, Config &cfg, SkyRole role)
        {
            doc.child(averagingKey).take("windowSeconds", cfg.averaging(role).windowSeconds);

            SkyCalibrationConfig &target = cfg.calibration(role);
            const Section cal = doc.child(calibrationKey);
            cal.take("enabled", target.enabled);
            cal.take("sqmOffset", target.sqmOffset);
            cal.take("darkVisibleOffset", target.darkVisibleOffset);
            cal.take("darkFullOffset", target.darkFullOffset);
            cal.take("darkIrOffset", target.darkIrOffset);
            cal.take("darkSampleCount", target.darkSampleCount);
            cal.take("darkCalibratedAt", target.darkCalibratedAt);
        }

        // Which sky sensors run (spec 030). An unknown authoritative name is
        // rejected; settings stored by older firmware never have one.
        bool applySkySensors(const Section &doc, Config &cfg, std::string *error)
        {
            const Section sky = doc.child("sky");
            sky.take("sqmEnabled", cfg.sky.sqmEnabled);
            sky.take("sqmlEnabled", cfg.sky.sqmlEnabled);
            sky.take("showOther", cfg.sky.showOther);
            if (sky.has("authoritative"))
            {
                JsonVariant value = sky["authoritative"];
                if (!parseSkyRole(value.is<const char *>() ? value.as<const char *>() : nullptr, cfg.sky.authoritative))
                    return setError(error, "Authoritative sky reading must be sqm or sqml");
            }
            return true;
        }

        bool applySky(const Section &doc, Config &cfg, std::string *error)
        {
            const Section sensor = doc.child("sensor");
            sensor.take("readIntervalMs", cfg.sensor.readIntervalMs);
            sensor.take("i2cSDA", cfg.sensor.i2cSDA);
            sensor.take("i2cSCL", cfg.sensor.i2cSCL);
            sensor.take("i2cFrequency", cfg.sensor.i2cFrequency);
            sensor.take("i2c2SDA", cfg.sensor.i2c2SDA);
            sensor.take("i2c2SCL", cfg.sensor.i2c2SCL);

            applySkySections(doc, "skyAveraging", "skyCalibration", cfg, SkyRole::Sqm);
            applySkySections(doc, "skyAveragingL", "skyCalibrationL", cfg, SkyRole::Sqml);

            const Section cloud = doc.child("cloudDetection");
            cloud.take("clearSkyThreshold", cfg.cloudDetection.clearSkyThreshold);
            cloud.take("cloudyThreshold", cfg.cloudDetection.cloudyThreshold);
            cloud.take("humidityCorrection", cfg.cloudDetection.humidityCorrection);
            return applySkySensors(doc, cfg, error);
        }

        void applyAlpaca(const Section &alpaca, AlpacaConfig &a)
        {
            alpaca.take("enabled", a.enabled);
            alpaca.take("manualOverrideUnsafe", a.manualOverrideUnsafe);
            alpaca.take("staleAfterSeconds", a.staleAfterSeconds);
            alpaca.take("cloudCoverEnabled", a.cloudCoverEnabled);
            alpaca.take("cloudCoverUnsafePercent", a.cloudCoverUnsafePercent);
            alpaca.take("sqmMinEnabled", a.sqmMinEnabled);
            alpaca.take("sqmMinSafe", a.sqmMinSafe);
            alpaca.take("humidityMaxEnabled", a.humidityMaxEnabled);
            alpaca.take("humidityMaxSafe", a.humidityMaxSafe);
            alpaca.take("dewpointMarginEnabled", a.dewpointMarginEnabled);
            alpaca.take("dewpointMarginMinC", a.dewpointMarginMinC);
            alpaca.take("rainUnsafeEnabled", a.rainUnsafeEnabled);
            alpaca.take("rainSensorRequired", a.rainSensorRequired);
            alpaca.take("safeDelaySeconds", a.safeDelaySeconds);
            alpaca.take("windSpeedUnsafeEnabled", a.windSpeedUnsafeEnabled);
            alpaca.take("windSpeedUnsafeMs", a.windSpeedUnsafeMs);
            alpaca.take("windGustUnsafeEnabled", a.windGustUnsafeEnabled);
            alpaca.take("windGustUnsafeMs", a.windGustUnsafeMs);
            alpaca.take("batteryEnabled", a.batteryEnabled);
            alpaca.take("batteryMinPercent", a.batteryMinPercent);
            alpaca.take("batteryAlert", a.batteryAlert);
            alpaca.take("batteryPhoneAlarm", a.batteryPhoneAlarm);
        }

        // Applies an "events" object ({"rain_started": {"level": 4, ...}, ...}).
        void applyEvents(const Section &events, AlertsConfig &a)
        {
            for (const auto &entry : eventSettings(a))
            {
                const Section event = events.child(entry.first);
                AlertsConfig::EventSetting &target = *entry.second;
                event.take("level", target.level);
                event.take("sound", target.sound);
                event.take("title", target.title);
                event.take("message", target.message);
            }
        }

        // Settings saved before per-event levels: on/off toggles.
        void applyLegacyToggles(const Section &alerts, AlertsConfig &a)
        {
            auto on = [&alerts](const char *key) { return alerts.check<bool>(key) && alerts[key].as<bool>(); };
            auto off = [&alerts](const char *key) { return alerts.check<bool>(key) && !alerts[key].as<bool>(); };
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
        bool applySendMode(const Section &obj, AlertsConfig &a, std::string *error)
        {
            if (obj.has("sendMode"))
            {
                const std::string mode = obj["sendMode"].is<const char *>() ? obj["sendMode"].as<const char *>() : "";
                if (mode == "any")
                    a.sendMode = AlertsConfig::SendMode::Any;
                else if (mode == "whileConnected")
                    a.sendMode = AlertsConfig::SendMode::WhileConnected;
                else
                    return setError(error, "Alerts: when to send must be any or whileConnected");
            }
            else if (obj.check<bool>("armWithAlpaca"))
            {
                // Settings and scripts from before sendMode.
                a.sendMode = obj["armWithAlpaca"].as<bool>() ? AlertsConfig::SendMode::WhileConnected : AlertsConfig::SendMode::Any;
            }
            return true;
        }

        void applyClientSilence(const Section &obj, AlertsConfig &a)
        {
            obj.take("clientSilentSafetySeconds", a.clientSilentSafetySeconds);
            obj.take("clientSilentWeatherSeconds", a.clientSilentWeatherSeconds);
        }

        void applyAlertChannels(const Section &alerts, AlertsConfig &a, bool keepSecrets)
        {
            const Section pushover = alerts.child("pushover");
            if (!pushover.isNull())
            {
                pushover.take("enabled", a.pushoverEnabled);
                assignSecret(pushover, "userKey", a.pushoverUserKey, keepSecrets);
                assignSecret(pushover, "appToken", a.pushoverAppToken, keepSecrets);
                // Pasted keys often carry a stray space or newline, which
                // Pushover rejects as "not a valid user".
                trimInPlace(a.pushoverUserKey);
                trimInPlace(a.pushoverAppToken);
                pushover.take("sound", a.pushoverSound);
            }

            const Section ntfy = alerts.child("ntfy");
            ntfy.take("enabled", a.ntfyEnabled);
            ntfy.take("server", a.ntfyServer);
            ntfy.take("topic", a.ntfyTopic);
            assignSecret(ntfy, "token", a.ntfyToken, keepSecrets);

            const Section webhook = alerts.child("webhook");
            webhook.take("enabled", a.webhookEnabled);
            webhook.take("url", a.webhookUrl);
            assignSecret(webhook, "authHeader", a.webhookAuthHeader, keepSecrets);
            webhook.take("insecureTls", a.webhookInsecureTls);

            alerts.child("mqtt").take("enabled", a.mqttEnabled);
        }

        bool applyAlerts(const Section &alerts, AlertsConfig &a, bool keepSecrets, std::string *error)
        {
            if (alerts.isNull())
                return true;
            alerts.take("enabled", a.enabled);
            const Section events = alerts.child("events");
            if (!events.isNull())
                applyEvents(events, a);
            else
                applyLegacyToggles(alerts, a);
            alerts.take("dewRiskMarginC", a.dewRiskMarginC);
            alerts.take("clearSkyCloudPercent", a.clearSkyCloudPercent);
            alerts.take("cloudedOverCloudPercent", a.cloudedOverCloudPercent);
            alerts.take("skyNightOnly", a.skyNightOnly);
            alerts.take("safetyNightOnly", a.safetyNightOnly);
            if (!applySendMode(alerts, a, error))
                return false;
            applyClientSilence(alerts, a);
            alerts.take("nightSunAltitudeDeg", a.nightSunAltitudeDeg);
            alerts.take("cooldownSeconds", a.cooldownSeconds);
            applyAlertChannels(alerts, a, keepSecrets);
            return true;
        }

        // The imaging-app alert settings as stored under their own NVS key
        // (ConfigStore splices them in as "alertsClient").
        bool applyAlertsClient(const Section &client, AlertsConfig &a, std::string *error)
        {
            if (client.isNull())
                return true;
            const Section events = client.child("events");
            if (!events.isNull())
                applyEvents(events, a);
            if (!applySendMode(client, a, error))
                return false;
            applyClientSilence(client, a);
            return true;
        }

        // The battery events as stored under their own NVS key (ConfigStore
        // splices them in as "alertsBattery", spec 032).
        void applyAlertsBattery(const Section &battery, AlertsConfig &a)
        {
            const Section events = battery.child("events");
            if (!events.isNull())
                applyEvents(events, a);
        }

        void applyHardware(const Section &doc, Config &cfg, bool keepSecrets)
        {
            const Section wind = doc.child("wind");
            wind.take("enabled", cfg.wind.enabled);
            wind.take("speedPin", cfg.wind.speedPin);
            wind.take("directionEnabled", cfg.wind.directionEnabled);
            wind.take("directionPin", cfg.wind.directionPin);
            wind.take("kmhPerHz", cfg.wind.kmhPerHz);
            wind.take("directionOffsetDeg", cfg.wind.directionOffsetDeg);
            wind.take("vanePullupOhms", cfg.wind.vanePullupOhms);

            const Section ble = doc.child("ble");
            ble.take("enabled", cfg.ble.enabled);
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
        // Every section, in order. False (with the reason) on a value that
        // can never be stored.
        bool applySections(const Section &root, Config &cfg, bool keep, std::string *error)
        {
            if (!applyRoot(root, cfg, error))
                return false;
            applyWifi(root.child("wifi"), cfg.wifi, keep);
            applyMqtt(root.child("mqtt"), cfg.mqtt, keep);
            applyAccess(root, cfg, keep);
            applyTime(root, cfg);
            applyRain(root.child("rain"), cfg.rain);
            // The battery rule's limit defaults to the critical level and
            // follows it while the two are equal (spec 032 FR-008).
            const bool minFollowsCritical =
                !root.child("alpaca").has("batteryMinPercent") && cfg.alpaca.batteryMinPercent == cfg.battery.criticalPercent;
            applyAlpaca(root.child("alpaca"), cfg.alpaca);
            if (!applySky(root, cfg, error) || !applyAlerts(root.child("alerts"), cfg.alerts, keep, error) ||
                !applyAlertsClient(root.child("alertsClient"), cfg.alerts, error))
                return false;
            cfg.alerts.armWithAlpaca = cfg.alerts.sendMode == AlertsConfig::SendMode::WhileConnected;
            applyAlertsBattery(root.child("alertsBattery"), cfg.alerts);
            applyHardware(root, cfg, keep);
            applyBattery(root.child("battery"), cfg.battery);
            // An out-of-range critical level is left to its own check's message.
            if (minFollowsCritical && cfg.battery.criticalPercent >= 2 && cfg.battery.criticalPercent <= 25)
                cfg.alpaca.batteryMinPercent = cfg.battery.criticalPercent;
            return applyDisplay(root.child("display"), cfg.display, error);
        }
    } // namespace

    bool Config::applyJson(const std::string &json, Config &cfg, bool preserveSecretPlaceholders, std::string *errorOut)
    {
        // Parsing copies every string; custom alert texts can make the JSON
        // bigger than the old fixed 8 KB (+1 KB for the battery, spec 032).
        DynamicJsonDocument doc(json.size() + 7168 > 9216 ? json.size() + 7168 : 9216);
        const DeserializationError error = deserializeJson(doc, json);
        if (error)
            return setError(errorOut, std::string("Settings JSON couldn't be read: ") + error.c_str());
        if (!checkWholeNumbers(doc, errorOut))
            return false;

        const bool keep = preserveSecretPlaceholders;
        // Changes from the UI/API are strict about types; stored settings
        // keep going (see JsonReader).
        JsonReader reader(preserveSecretPlaceholders);
        if (reader.strict() && !doc.is<JsonObject>())
            return setError(errorOut, "Settings must be a JSON object");
        const Section root(reader, doc.as<JsonVariant>(), "");
        const std::string previousSsid = cfg.wifi.ssid;
        if (!applySections(root, cfg, keep, errorOut))
            return false;
        if (reader.failed())
            return typeError(reader, errorOut);
        // Saving settings never takes the device off its network: changing
        // networks is WiFi setup's job (POST /api/wifi/connect).
        if (reader.strict() && isBlank(cfg.wifi.ssid) && !isBlank(previousSsid))
            return setError(errorOut, "WiFi network can't be blank; use WiFi setup to change networks");

        normalizeTimeSources(cfg);
        if (!cfg.validate(errorOut))
            return false;
        return !preserveSecretPlaceholders || validateIncoming(cfg, errorOut);
    }

    std::optional<std::string> Config::mqttTestPassword(const MQTTConfig &saved, const MqttTestTarget &target)
    {
        if (!isPlaceholderSecret(target.password.c_str()) || saved.password.empty())
            return target.password;
        if (target.broker == saved.broker && target.port == saved.port && target.username == saved.username)
            return saved.password;
        if (target.password.empty())
            return target.password; // testing another broker without a password
        return std::nullopt;
    }
} // namespace SQM
