#include "BleAlarm.h"
#include "Config.h"
#include "ConfigInternal.h"
#include "LanguageLogic.h"
#include "NetAddress.h"
#include <cctype>
#include <cmath>
#include <initializer_list>

// Config::validate(): every rule the device enforces, one function per
// settings area, checked in a fixed order so the first problem found is the
// one reported. The web UI applies the same rules
// (web/src/validation/configSchema.ts, test/fixtures/config-ranges.json).

namespace SQM
{
    using namespace ConfigDetail;

    namespace
    {
        // Letters, digits, _ and -, with / between levels (same rule as the web UI).
        bool validTopic(const std::string &topic)
        {
            if (topic.empty() || topic.front() == '/' || topic.back() == '/' || topic.find("//") != std::string::npos)
                return false;
            for (char c : topic)
                if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' || c == '/'))
                    return false;
            return true;
        }

        // A DNS label, so it works as <hostname>.local (same rule as the web UI).
        bool validHostname(const std::string &name)
        {
            if (name.empty() || name.size() > 32 || name.front() == '-' || name.back() == '-')
                return false;
            for (char c : name)
                if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-'))
                    return false;
            return true;
        }

        bool isHttpUrl(const std::string &url)
        {
            return url.rfind("http://", 0) == 0 || url.rfind("https://", 0) == 0;
        }

        bool validateIdentity(const Config &c, std::string *error)
        {
            if (c.deviceName.empty())
                return setError(error, "Device name is required");
            if (!Language::isSupported(std::string(c.language)))
                return setError(error, "Language: not a supported language");
            return true;
        }

        bool validateTimeSources(const Config &c, std::string *error)
        {
            if (c.primaryTimeSource != TimeSource::NTP && c.primaryTimeSource != TimeSource::GPS)
                return setError(error, "Primary time source is invalid");
            if (c.secondaryTimeSource != TimeSource::NTP && c.secondaryTimeSource != TimeSource::GPS)
                return setError(error, "Secondary time source is invalid");
            if (!c.ntp.enabled && !c.gps.enabled)
                return setError(error, "At least one time source must be enabled");
            if (!isTimeSourceEnabled(c, c.primaryTimeSource))
                return setError(error, "Primary time source is disabled");
            const bool both = c.ntp.enabled && c.gps.enabled;
            if (both && !isTimeSourceEnabled(c, c.secondaryTimeSource))
                return setError(error, "Secondary time source is disabled");
            if (both && c.primaryTimeSource == c.secondaryTimeSource)
                return setError(error, "Time sources must be different when both NTP and GPS are enabled");
            return true;
        }

        bool validReconnectDelays(const WiFiConfig &w)
        {
            constexpr uint32_t DAY_MS = 86400000;
            return w.reconnectDelayMs > 0 && w.maxReconnectDelayMs > 0 && w.reconnectDelayMs <= DAY_MS && w.maxReconnectDelayMs <= DAY_MS &&
                   w.reconnectDelayMs <= w.maxReconnectDelayMs;
        }

        bool validateWifiAndMqtt(const Config &c, std::string *error)
        {
            const WiFiConfig &w = c.wifi;
            if (!validReconnectDelays(w))
                return setError(error, "WiFi reconnect delays are invalid");
            if (c.mqtt.port == 0)
                return setError(error, "MQTT port is invalid");
            if (c.mqtt.enabled && (c.mqtt.broker.empty() || c.mqtt.topic.empty()))
                return setError(error, "MQTT broker and topic are required when MQTT is enabled");
            if (!validHostname(w.hostname))
                return setError(error, "Hostname: use up to 32 letters, numbers and hyphens (not at either end)");
            if (c.mqtt.enabled && !validTopic(c.mqtt.topic))
                return setError(error, "MQTT topic: use letters, numbers, _ and -, with / between levels");
            if (c.mqtt.homeAssistant && !validTopic(c.mqtt.discoveryPrefix))
                return setError(error, "MQTT discovery prefix: use letters, numbers, _ and -, with / between levels");
            if (c.mqtt.publishIntervalMs < 1000 || c.mqtt.publishIntervalMs > 86400000)
                return setError(error, "MQTT publish interval is invalid");
            return true;
        }

        bool validateAuthAndNtp(const Config &c, std::string *error)
        {
            if (c.auth.enabled && c.auth.password.empty())
                return setError(error, "HTTP auth password is required when auth is enabled");
            if (c.auth.enabled && c.auth.username.empty())
                return setError(error, "HTTP auth username is required when auth is enabled");
            if (c.ntp.enabled && c.ntp.server1.empty())
                return setError(error, "Primary NTP server is required when NTP is enabled");
            if (c.ntp.syncIntervalMs < 600000 || c.ntp.syncIntervalMs > 86400000)
                return setError(error, "NTP sync interval is invalid");
            if (!isValidGpio(c.gps.rxPin) || !isValidGpio(c.gps.txPin) || c.gps.rxPin == c.gps.txPin)
                return setError(error, "GPS pins are invalid");
            if (!isValidBaudRate(c.gps.baudRate))
                return setError(error, "GPS baud rate is invalid");
            return true;
        }

        bool oneOf(const std::string &value, std::initializer_list<const char *> allowed)
        {
            for (const char *option : allowed)
                if (value == option)
                    return true;
            return false;
        }

        bool validateRain(const RainConfig &r, std::string *error)
        {
            if (!isValidGpio(r.rxPin) || !isValidGpio(r.txPin) || (r.enabled && r.rxPin == r.txPin))
                return setError(error, "Rain sensor pins are invalid");
            if (!isValidBaudRate(r.baudRate))
                return setError(error, "Rain sensor baud rate is invalid");
            if (r.mode != "polling")
                return setError(error, "Rain sensor mode must be polling");
            if (!oneOf(r.resolution, {"high", "low", "switch"}))
                return setError(error, "Rain sensor resolution is invalid");
            if (!oneOf(r.units, {"metric", "imperial", "switch"}))
                return setError(error, "Rain sensor units are invalid");
            if (r.pollIntervalMs < 1000 || r.pollIntervalMs > 3600000)
                return setError(error, "Rain sensor poll interval is invalid");
            if (r.rainClearDelayMs < 60000 || r.rainClearDelayMs > 86400000)
                return setError(error, "Rain clear delay is invalid");
            if (r.dailyResetHour > 23 || r.dailyResetMinute > 59)
                return setError(error, "Rain daily reset time is invalid");
            return true;
        }

        bool validateSky(const Config &c, std::string *error)
        {
            const SensorConfig &s = c.sensor;
            if (s.readIntervalMs < 100 || s.readIntervalMs > 3600000)
                return setError(error, "Sensor read interval is invalid");
            if (!isValidGpio(s.i2cSDA) || !isValidGpio(s.i2cSCL) || s.i2cSDA == s.i2cSCL)
                return setError(error, "I2C pins are invalid");
            if (s.i2cFrequency < 10000 || s.i2cFrequency > 400000)
                return setError(error, "I2C frequency is invalid");
            if (c.skyAveraging.windowSeconds < 10 || c.skyAveraging.windowSeconds > 300)
                return setError(error, "Sky averaging window is invalid");
            const SkyCalibrationConfig &cal = c.skyCalibration;
            if (!inRange(cal.sqmOffset, -5.0F, 5.0F))
                return setError(error, "Sky SQM calibration offset is invalid");
            if (!inRange(cal.darkVisibleOffset, 0.0F, INFINITY) || !inRange(cal.darkFullOffset, 0.0F, INFINITY) ||
                !inRange(cal.darkIrOffset, 0.0F, INFINITY))
                return setError(error, "Sky dark calibration offsets are invalid");
            return true;
        }

        bool validateCloudDetection(const CloudDetectionConfig &d, std::string *error)
        {
            if (!inRange(d.clearSkyThreshold, -30.0F, 0.0F))
                return setError(error, "Cloud detection: clear-sky threshold must be between -30 and 0 degrees C");
            if (!inRange(d.cloudyThreshold, -20.0F, 10.0F))
                return setError(error, "Cloud detection: cloudy threshold must be between -20 and 10 degrees C");
            if (!inRange(d.humidityCorrection, 0.0F, 2.0F))
                return setError(error, "Cloud detection: humidity correction must be between 0 and 2");
            if (d.clearSkyThreshold >= d.cloudyThreshold)
                return setError(error, "Cloud detection: clear-sky threshold must be less than cloudy threshold");
            return true;
        }

        // Wind limits must be above 0, so they're checked as "positive and at most max".
        bool positiveUpTo(float value, float max)
        {
            return std::isfinite(value) && value > 0.0F && value <= max;
        }

        bool validateAlpaca(const AlpacaConfig &a, std::string *error)
        {
            if (a.staleAfterSeconds < 1 || a.staleAfterSeconds > 3600)
                return setError(error, "Alpaca: stale data threshold must be between 1 and 3600 seconds");
            if (!inRange(a.cloudCoverUnsafePercent, 0.0F, 100.0F))
                return setError(error, "Alpaca: cloud cover threshold must be between 0 and 100 percent");
            if (!inRange(a.sqmMinSafe, 0.0F, 30.0F))
                return setError(error, "Alpaca: minimum SQM threshold must be between 0 and 30");
            if (!inRange(a.humidityMaxSafe, 0.0F, 100.0F))
                return setError(error, "Alpaca: maximum humidity threshold must be between 0 and 100 percent");
            if (!inRange(a.dewpointMarginMinC, 0.0F, 20.0F))
                return setError(error, "Alpaca: dewpoint margin threshold must be between 0 and 20 degrees C");
            if (a.safeDelaySeconds > 3600)
                return setError(error, "Alpaca: safe delay must be between 0 and 3600 seconds");
            if (!positiveUpTo(a.windSpeedUnsafeMs, 60.0F))
                return setError(error, "Alpaca: wind speed threshold must be between 0 and 60 m/s");
            if (!positiveUpTo(a.windGustUnsafeMs, 80.0F))
                return setError(error, "Alpaca: wind gust threshold must be between 0 and 80 m/s");
            return true;
        }

        bool validateWindPins(const Config &c, std::string *error)
        {
            const WindConfig &w = c.wind;
            if (!isValidGpio(w.speedPin))
                return setError(error, "Wind: anemometer pin is not a valid GPIO");
            if (w.directionEnabled && (w.directionPin < 32 || w.directionPin > 39))
                return setError(error, "Wind: vane pin must be an ADC1 pin (GPIO 32-39)");
            const int used[] = {
                c.sensor.i2cSDA,
                c.sensor.i2cSCL,
                c.gps.enabled ? c.gps.rxPin : -1,
                c.gps.enabled ? c.gps.txPin : -1,
                c.rain.enabled ? c.rain.rxPin : -1,
                c.rain.enabled ? c.rain.txPin : -1};
            for (int pin : used)
            {
                if (pin == w.speedPin || (w.directionEnabled && pin == w.directionPin))
                    return setError(error, "Wind: pin is already used by I2C, GPS or the rain sensor");
            }
            if (w.directionEnabled && w.directionPin == w.speedPin)
                return setError(error, "Wind: anemometer and vane need different pins");
            return true;
        }

        bool validateWindAndBle(const Config &c, std::string *error)
        {
            if (c.wind.enabled && !validateWindPins(c, error))
                return false;
            if (!positiveUpTo(c.wind.kmhPerHz, 20.0F))
                return setError(error, "Wind: km/h per Hz must be between 0 and 20");
            if (!inRange(c.wind.directionOffsetDeg, -360.0F, 360.0F))
                return setError(error, "Wind: direction offset must be between -360 and 360 degrees");
            if (!inRange(c.wind.vanePullupOhms, 1000.0F, 100000.0F))
                return setError(error, "Wind: vane pull-up must be between 1k and 100k ohms");
            uint32_t passkey = 0;
            if (!c.ble.passkey.empty() && !Ble::parsePasskey(c.ble.passkey, passkey))
                return setError(error, "Bluetooth: pairing passkey must be 6 digits (not 000000)");
            return true;
        }

        bool validateAlertThresholds(const Config &c, std::string *error)
        {
            const AlertsConfig &a = c.alerts;
            if (a.cooldownSeconds > 86400)
                return setError(error, "Alerts: cooldown must be between 0 and 86400 seconds");
            if (!inRange(a.dewRiskMarginC, 0.0F, 10.0F))
                return setError(error, "Alerts: dew risk margin must be between 0 and 10 degrees C");
            if (!inRange(a.clearSkyCloudPercent, 0.0F, 100.0F))
                return setError(error, "Alerts: clear sky threshold must be between 0 and 100 percent");
            if (!inRange(a.cloudedOverCloudPercent, 0.0F, 100.0F) || a.cloudedOverCloudPercent <= a.clearSkyCloudPercent)
                return setError(error, "Alerts: the clouded-over threshold must be above the clear threshold");
            if (!inRange(a.nightSunAltitudeDeg, -20.0F, 0.0F))
                return setError(error, "Alerts: night must start with the sun between 0 and -20 degrees");
            const LocationConfig &l = c.location;
            if (l.set && (!std::isfinite(l.latitude) || std::fabs(l.latitude) > 90.0 || !std::isfinite(l.longitude) ||
                          std::fabs(l.longitude) > 180.0))
                return setError(error, "Location: latitude must be -90..90 and longitude -180..180");
            return true;
        }

        bool validateAlertEvents(const Config &c, std::string *error)
        {
            const AlertsConfig &a = c.alerts;
            for (const auto &entry : eventSettings(a))
            {
                if (entry.second->level > 4)
                    return setError(error, "Alerts: event levels are 0 (off) to 4 (wake me)");
                if (entry.second->title.size() > AlertsConfig::MAX_TEMPLATE_TITLE ||
                    entry.second->message.size() > AlertsConfig::MAX_TEMPLATE_MESSAGE)
                    return setError(error, "Alerts: custom titles are up to 80 characters and messages up to 240");
            }
            if (a.sendMode != AlertsConfig::SendMode::Any && a.sendMode != AlertsConfig::SendMode::WhileConnected)
                return setError(error, "Alerts: when to send must be any or whileConnected");
            for (uint32_t seconds : {a.clientSilentSafetySeconds, a.clientSilentWeatherSeconds})
                if (seconds < AlertsConfig::MIN_CLIENT_SILENT_SECONDS || seconds > AlertsConfig::MAX_CLIENT_SILENT_SECONDS)
                    return setError(error, "Alerts: silence times must be between 30 and 3600 seconds");
            // NVS strings top out just under 4000 bytes; each stored part has its own key.
            if (c.alertsToJson(false, Config::AlertsPart::Main).size() > Config::MAX_ALERTS_JSON_BYTES ||
                c.alertsToJson(false, Config::AlertsPart::Client).size() > Config::MAX_ALERTS_JSON_BYTES)
                return setError(error, "Alerts: the custom alert texts are too long in total - shorten some");
            return true;
        }

        bool validateAlertChannels(const AlertsConfig &a, std::string *error)
        {
            if (a.pushoverEnabled && (a.pushoverUserKey.empty() || a.pushoverAppToken.empty()))
                return setError(error, "Alerts: Pushover needs both a user key and an application token");
            if (a.ntfyEnabled && (!isHttpUrl(a.ntfyServer) || a.ntfyTopic.empty()))
                return setError(error, "Alerts: ntfy needs an http(s):// server and a topic");
            if (a.webhookEnabled && !isHttpUrl(a.webhookUrl))
                return setError(error, "Alerts: webhook URL must start with http:// or https://");
            return true;
        }
    } // namespace

    namespace ConfigDetail
    {
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
    } // namespace ConfigDetail

    bool Config::validate(std::string *error) const
    {
        using Check = bool (*)(const Config &, std::string *);
        static const Check CHECKS[] = {
            validateIdentity,
            validateTimeSources,
            validateWifiAndMqtt,
            validateAuthAndNtp,
            [](const Config &c, std::string *e) { return validateRain(c.rain, e); },
            validateSky,
            [](const Config &c, std::string *e) { return validateCloudDetection(c.cloudDetection, e); },
            [](const Config &c, std::string *e) { return validateAlpaca(c.alpaca, e); },
            validateWindAndBle,
            validateAlertThresholds,
            validateAlertEvents,
            [](const Config &c, std::string *e) { return validateAlertChannels(c.alerts, e); },
        };
        for (Check check : CHECKS)
            if (!check(*this, error))
                return false;
        return true;
    }
} // namespace SQM
