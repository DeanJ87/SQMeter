#include "Config.h"
#include "ConfigInternal.h"
#include <ArduinoJson.h>

// Settings as JSON, out: toJson() and alertsToJson(). One function per
// section, in the order the sections appear in the document.

namespace SQM
{
    using namespace ConfigDetail;

    namespace
    {
        const char *secretOut(const std::string &value, bool redact)
        {
            return redact && !value.empty() ? SECRET_MASK : value.c_str();
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

        void appendClientSilence(JsonObject alerts, const AlertsConfig &a)
        {
            alerts["sendMode"] = sendModeName(a.sendMode);
            alerts["clientSilentSafetySeconds"] = a.clientSilentSafetySeconds;
            alerts["clientSilentWeatherSeconds"] = a.clientSilentWeatherSeconds;
        }

        void appendAlertChannels(JsonObject alerts, const AlertsConfig &a, bool redact)
        {
            JsonObject pushover = alerts.createNestedObject("pushover");
            pushover["enabled"] = a.pushoverEnabled;
            pushover["userKey"] = secretOut(a.pushoverUserKey, redact);
            pushover["appToken"] = secretOut(a.pushoverAppToken, redact);
            pushover["sound"] = a.pushoverSound.c_str();

            JsonObject ntfy = alerts.createNestedObject("ntfy");
            ntfy["enabled"] = a.ntfyEnabled;
            ntfy["server"] = a.ntfyServer.c_str();
            ntfy["topic"] = a.ntfyTopic.c_str();
            ntfy["token"] = secretOut(a.ntfyToken, redact);

            JsonObject webhook = alerts.createNestedObject("webhook");
            webhook["enabled"] = a.webhookEnabled;
            webhook["url"] = a.webhookUrl.c_str();
            webhook["authHeader"] = secretOut(a.webhookAuthHeader, redact);
            webhook["insecureTls"] = a.webhookInsecureTls;

            JsonObject mqtt = alerts.createNestedObject("mqtt");
            mqtt["enabled"] = a.mqttEnabled;
        }

        void appendAlerts(JsonObject alerts, const AlertsConfig &a, bool redactSecrets, Config::AlertsPart part)
        {
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
                appendClientSilence(alerts, a);
            alerts["dewRiskMarginC"] = a.dewRiskMarginC;
            alerts["clearSkyCloudPercent"] = a.clearSkyCloudPercent;
            alerts["cloudedOverCloudPercent"] = a.cloudedOverCloudPercent;
            alerts["skyNightOnly"] = a.skyNightOnly;
            alerts["safetyNightOnly"] = a.safetyNightOnly;
            // Older firmware reads this instead of sendMode.
            alerts["armWithAlpaca"] = a.sendMode == AlertsConfig::SendMode::WhileConnected;
            alerts["nightSunAltitudeDeg"] = a.nightSunAltitudeDeg;
            alerts["cooldownSeconds"] = a.cooldownSeconds;
            appendAlertChannels(alerts, a, redactSecrets);
        }

        void appendNetwork(JsonDocument &doc, const Config &c, bool redact)
        {
            JsonObject wifi = doc.createNestedObject("wifi");
            wifi["ssid"] = c.wifi.ssid;
            wifi["password"] = secretOut(c.wifi.password, redact);
            wifi["hostname"] = c.wifi.hostname;
            wifi["mdns"] = c.wifi.mdns;
            wifi["ipv6"] = c.wifi.ipv6;
            wifi["autoReconnect"] = c.wifi.autoReconnect;
            wifi["reconnectDelayMs"] = c.wifi.reconnectDelayMs;
            wifi["maxReconnectDelayMs"] = c.wifi.maxReconnectDelayMs;

            JsonObject mqtt = doc.createNestedObject("mqtt");
            mqtt["enabled"] = c.mqtt.enabled;
            mqtt["broker"] = c.mqtt.broker;
            mqtt["port"] = c.mqtt.port;
            mqtt["username"] = c.mqtt.username;
            mqtt["password"] = secretOut(c.mqtt.password, redact);
            mqtt["topic"] = c.mqtt.topic;
            mqtt["publishIntervalMs"] = c.mqtt.publishIntervalMs;
            JsonObject publish = mqtt.createNestedObject("publish");
            publish["sky"] = c.mqtt.publish.sky;
            publish["environment"] = c.mqtt.publish.environment;
            publish["clouds"] = c.mqtt.publish.clouds;
            publish["gps"] = c.mqtt.publish.gps;
            publish["rain"] = c.mqtt.publish.rain;
            publish["wind"] = c.mqtt.publish.wind;
            publish["safety"] = c.mqtt.publish.safety;
            publish["diagnostics"] = c.mqtt.publish.diagnostics;
            JsonObject homeAssistant = mqtt.createNestedObject("homeAssistant");
            homeAssistant["enabled"] = c.mqtt.homeAssistant;
            homeAssistant["discoveryPrefix"] = c.mqtt.discoveryPrefix;

            JsonObject ota = doc.createNestedObject("ota");
            ota["enabled"] = c.ota.enabled;
            ota["password"] = secretOut(c.ota.password, redact);

            JsonObject auth = doc.createNestedObject("auth");
            auth["enabled"] = c.auth.enabled;
            auth["username"] = c.auth.username;
            auth["password"] = secretOut(c.auth.password, redact);
        }

        void appendTimeAndSerial(JsonDocument &doc, const Config &c)
        {
            JsonObject ntp = doc.createNestedObject("ntp");
            ntp["enabled"] = c.ntp.enabled;
            ntp["server1"] = c.ntp.server1;
            ntp["server2"] = c.ntp.server2;
            ntp["timezone"] = c.ntp.timezone;
            ntp["syncIntervalMs"] = c.ntp.syncIntervalMs;

            JsonObject gps = doc.createNestedObject("gps");
            gps["enabled"] = c.gps.enabled;
            gps["rxPin"] = c.gps.rxPin;
            gps["txPin"] = c.gps.txPin;
            gps["baudRate"] = c.gps.baudRate;

            JsonObject rain = doc.createNestedObject("rain");
            rain["enabled"] = c.rain.enabled;
            rain["rxPin"] = c.rain.rxPin;
            rain["txPin"] = c.rain.txPin;
            rain["baudRate"] = c.rain.baudRate;
            rain["debugUart"] = c.rain.debugUart;
            rain["mode"] = c.rain.mode;
            rain["resolution"] = c.rain.resolution;
            rain["units"] = c.rain.units;
            rain["pollIntervalMs"] = c.rain.pollIntervalMs;
            rain["rainClearDelayMs"] = c.rain.rainClearDelayMs;
            rain["dailyResetEnabled"] = c.rain.dailyResetEnabled;
            rain["dailyResetHour"] = c.rain.dailyResetHour;
            rain["dailyResetMinute"] = c.rain.dailyResetMinute;
        }

        void appendSky(JsonDocument &doc, const Config &c)
        {
            JsonObject sensor = doc.createNestedObject("sensor");
            sensor["readIntervalMs"] = c.sensor.readIntervalMs;
            sensor["i2cSDA"] = c.sensor.i2cSDA;
            sensor["i2cSCL"] = c.sensor.i2cSCL;
            sensor["i2cFrequency"] = c.sensor.i2cFrequency;

            JsonObject skyAveraging = doc.createNestedObject("skyAveraging");
            skyAveraging["windowSeconds"] = c.skyAveraging.windowSeconds;

            JsonObject skyCalibration = doc.createNestedObject("skyCalibration");
            skyCalibration["enabled"] = c.skyCalibration.enabled;
            skyCalibration["sqmOffset"] = c.skyCalibration.sqmOffset;
            skyCalibration["darkVisibleOffset"] = c.skyCalibration.darkVisibleOffset;
            skyCalibration["darkFullOffset"] = c.skyCalibration.darkFullOffset;
            skyCalibration["darkIrOffset"] = c.skyCalibration.darkIrOffset;
            skyCalibration["darkSampleCount"] = c.skyCalibration.darkSampleCount;
            skyCalibration["darkCalibratedAt"] = c.skyCalibration.darkCalibratedAt;

            JsonObject cloudDetection = doc.createNestedObject("cloudDetection");
            cloudDetection["clearSkyThreshold"] = c.cloudDetection.clearSkyThreshold;
            cloudDetection["cloudyThreshold"] = c.cloudDetection.cloudyThreshold;
            cloudDetection["humidityCorrection"] = c.cloudDetection.humidityCorrection;
        }

        void appendAlpaca(JsonDocument &doc, const AlpacaConfig &a)
        {
            JsonObject alpaca = doc.createNestedObject("alpaca");
            alpaca["enabled"] = a.enabled;
            alpaca["manualOverrideUnsafe"] = a.manualOverrideUnsafe;
            alpaca["staleAfterSeconds"] = a.staleAfterSeconds;
            alpaca["cloudCoverEnabled"] = a.cloudCoverEnabled;
            alpaca["cloudCoverUnsafePercent"] = a.cloudCoverUnsafePercent;
            alpaca["sqmMinEnabled"] = a.sqmMinEnabled;
            alpaca["sqmMinSafe"] = a.sqmMinSafe;
            alpaca["humidityMaxEnabled"] = a.humidityMaxEnabled;
            alpaca["humidityMaxSafe"] = a.humidityMaxSafe;
            alpaca["dewpointMarginEnabled"] = a.dewpointMarginEnabled;
            alpaca["dewpointMarginMinC"] = a.dewpointMarginMinC;
            alpaca["rainUnsafeEnabled"] = a.rainUnsafeEnabled;
            alpaca["rainSensorRequired"] = a.rainSensorRequired;
            alpaca["safeDelaySeconds"] = a.safeDelaySeconds;
            alpaca["windSpeedUnsafeEnabled"] = a.windSpeedUnsafeEnabled;
            alpaca["windSpeedUnsafeMs"] = a.windSpeedUnsafeMs;
            alpaca["windGustUnsafeEnabled"] = a.windGustUnsafeEnabled;
            alpaca["windGustUnsafeMs"] = a.windGustUnsafeMs;
        }

        void appendPlace(JsonDocument &doc, const Config &c, bool redact)
        {
            JsonObject ble = doc.createNestedObject("ble");
            ble["enabled"] = c.ble.enabled;
            ble["passkey"] = secretOut(c.ble.passkey, redact);

            JsonObject location = doc.createNestedObject("location");
            location["set"] = c.location.set;
            location["latitude"] = c.location.latitude;
            location["longitude"] = c.location.longitude;
            location["showSunMoon"] = c.location.showSunMoon;

            JsonObject wind = doc.createNestedObject("wind");
            wind["enabled"] = c.wind.enabled;
            wind["speedPin"] = c.wind.speedPin;
            wind["directionEnabled"] = c.wind.directionEnabled;
            wind["directionPin"] = c.wind.directionPin;
            wind["kmhPerHz"] = c.wind.kmhPerHz;
            wind["directionOffsetDeg"] = c.wind.directionOffsetDeg;
            wind["vanePullupOhms"] = c.wind.vanePullupOhms;
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
        doc["language"] = language;
        doc["primaryTimeSource"] = static_cast<int>(primaryTimeSource);
        doc["secondaryTimeSource"] = static_cast<int>(secondaryTimeSource);
        appendNetwork(doc, *this, redactSecrets);
        appendTimeAndSerial(doc, *this);
        appendSky(doc, *this);
        appendAlpaca(doc, alpaca);
        appendPlace(doc, *this, redactSecrets);
        if (includeAlerts)
            appendAlerts(doc.createNestedObject("alerts"), alerts, redactSecrets, AlertsPart::All);

        std::string output;
        serializeJson(doc, output);
        return output;
    }

} // namespace SQM
