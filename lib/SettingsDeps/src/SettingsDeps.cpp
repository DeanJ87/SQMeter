#include "SettingsDeps.h"

#include <cstring>
#include <initializer_list>

namespace SQM
{
    namespace Deps
    {
        namespace
        {
            // Same codes, texts and fix targets as catalogue.json (a native
            // test compares them).
            const std::vector<Reason> REASONS = {
                {"alerts-off", "Alerts are off", "alerts#alerts"},
                {"mqtt-off", "MQTT is off", "network#mqtt"},
                {"mqtt-disconnected", "Broker not connected", "network#mqtt"},
                {"wifi-disconnected", "Not connected to WiFi", "network#wifi"},
                {"rain-off", "Rain sensor is off", "sensors#rain"},
                {"wind-off", "Anemometer is off", "sensors#wind"},
                {"gps-off", "GPS is off", "time#time-sources"},
                {"gps-restart", "GPS starts after a restart", "restart"},
                {"gps-no-fix", "No GPS fix - using the location in Settings", "time#location"},
                {"light-missing", "TSL2591 not detected", "sensors#sky-sensors"},
                {"infrared-missing", "MLX90614 not detected", "sensors#sky-sensors"},
                {"environment-missing", "BME280 not detected", "sensors#sky-sensors"},
                {"location-unknown", "Needs your location", "time#location"},
                {"alpaca-off", "Alpaca is off", "safety#alpaca"},
                {"ble-build", "Needs the Bluetooth firmware build", "device#ble"},
                {"ble-off", "Bluetooth is off", "device#ble"},
                {"ble-restart", "Bluetooth starts after a restart", "restart"},
                {"no-passkey", "No pairing passkey set", "device#ble"},
                {"no-phones", "No phone paired", "device#ble"},
                {"clock-unset", "The device doesn't know the time yet", "time#time-sources"},
                {"ota-no-password", "Set an upload password", "device#security"},
            };

            const Reason *reason(const char *code)
            {
                for (const Reason &r : REASONS)
                    if (std::strcmp(r.code, code) == 0)
                        return &r;
                return nullptr;
            }

            // One link of a dependency chain: met, or the catalogue ID and
            // reason that make the setting inactive.
            struct Link
            {
                const char *id;
                const char *reason;
                bool met;
            };

            struct Builder
            {
                std::vector<Entry> &out;

                // `on`: the setting's own switch. The first unmet link decides.
                void add(const char *setting, const char *id, bool on, std::initializer_list<Link> chain, Unmet unmet = Unmet::None,
                         bool neutral = false)
                {
                    Entry e;
                    e.setting = setting;
                    e.id = id;
                    e.unmet = unmet;
                    e.neutral = neutral;
                    if (on)
                    {
                        e.state = State::Active;
                        for (const Link &link : chain)
                        {
                            if (link.met)
                                continue;
                            e.state = State::Inactive;
                            e.id = link.id;
                            e.reason = reason(link.reason);
                            break;
                        }
                    }
                    out.push_back(e);
                }
            };

            bool anyEventAt(const AlertsConfig &a, uint8_t level)
            {
                for (const AlertsConfig::EventSetting *e : {&a.unsafe, &a.safe, &a.rainStarted, &a.rainStopped, &a.sensorFault,
                                                            &a.sensorRecovered, &a.dewRisk, &a.clearSky, &a.cloudedOver})
                    if (e->level == level)
                        return true;
                return false;
            }
        } // namespace

        std::vector<Entry> evaluate(const Config &cfg, const Facts &f)
        {
            std::vector<Entry> out;
            out.reserve(44);
            Builder b{out};
            const AlertsConfig &a = cfg.alerts;

            // Links shared by several settings.
            const Link alertsOn{"D-04", "alerts-off", a.enabled};
            const Link wifi{"D-03", "wifi-disconnected", f.wifiConnected};
            const bool locationKnown = cfg.location.set || (f.gpsRunning && f.gpsFix);
            const bool phoneAlarm = !cfg.ble.passkey.empty();
            // Paired phones ring for Wake-level events even with push alerts
            // off (WebServer::processAlerts), so events, arming and the
            // night-only options still matter to them.
            const bool phonesRing = f.bluetoothBuild && cfg.ble.enabled && f.bluetoothRunning && phoneAlarm && f.pairedPhones > 0;
            const Link alerting{"D-04", "alerts-off", a.enabled || phonesRing};
            auto event = [&](const AlertsConfig::EventSetting &e) -> Link
            { return {"D-04", "alerts-off", a.enabled || (e.level == 4 && phonesRing)}; };

            // Alert channels (push only: they need the master switch). Links
            // in catalogue order, so the channel's own dependency comes
            // before "alerts are off" - a test send ignores the latter.
            b.add("alerts.pushover.enabled", "D-03", a.pushoverEnabled, {wifi, alertsOn});
            b.add("alerts.ntfy.enabled", "D-03", a.ntfyEnabled, {wifi, alertsOn});
            b.add("alerts.webhook.enabled", "D-03", a.webhookEnabled, {wifi, alertsOn});
            b.add("alerts.mqtt.enabled", "D-01", a.mqttEnabled,
                  {{"D-01", "mqtt-off", cfg.mqtt.enabled}, {"D-02", "mqtt-disconnected", f.mqttConnected}, alertsOn});

            // Alert events.
            const Link rainOn{"D-05", "rain-off", cfg.rain.enabled};
            const Link bmeFound{"D-06", "environment-missing", f.environmentDetected};
            const Link mlxFound{"D-07", "infrared-missing", f.infraredDetected};
            b.add("alerts.events.unsafe.level", "D-04", a.unsafe.level != 0, {event(a.unsafe)});
            b.add("alerts.events.safe.level", "D-04", a.safe.level != 0, {event(a.safe)});
            b.add("alerts.events.rain_started.level", "D-05", a.rainStarted.level != 0, {event(a.rainStarted), rainOn});
            b.add("alerts.events.rain_stopped.level", "D-05", a.rainStopped.level != 0, {event(a.rainStopped), rainOn});
            b.add("alerts.events.sensor_fault.level", "D-04", a.sensorFault.level != 0, {event(a.sensorFault)});
            b.add("alerts.events.sensor_recovered.level", "D-04", a.sensorRecovered.level != 0, {event(a.sensorRecovered)});
            b.add("alerts.events.dew_risk.level", "D-06", a.dewRisk.level != 0, {event(a.dewRisk), bmeFound});
            b.add("alerts.events.clear_sky.level", "D-07", a.clearSky.level != 0, {event(a.clearSky), mlxFound});
            b.add("alerts.events.clouded_over.level", "D-07", a.cloudedOver.level != 0, {event(a.cloudedOver), mlxFound});
            // "Wake me" also escalates Pushover and ntfy; only the phone ringing needs Bluetooth.
            b.add("alerts.wakePhones", "D-08", anyEventAt(a, 4),
                  {{"D-08", "ble-build", f.bluetoothBuild},
                   {"D-08", "ble-off", cfg.ble.enabled},
                   {"D-35", "ble-restart", f.bluetoothRunning},
                   {"D-08", "no-passkey", phoneAlarm},
                   {"D-08", "no-phones", f.pairedPhones > 0}});
            b.add("alerts.skyNightOnly", "D-09", a.skyNightOnly, {alerting, {"D-09", "location-unknown", locationKnown}});
            b.add("alerts.safetyNightOnly", "D-10", a.safetyNightOnly, {alerting, {"D-10", "location-unknown", locationKnown}});
            b.add("alerts.nightSunAltitudeDeg", "D-11", a.skyNightOnly || a.safetyNightOnly, {alerting});
            b.add("alerts.armWithAlpaca", "D-12", a.armWithAlpaca, {alerting, {"D-12", "alpaca-off", cfg.alpaca.enabled}});

            // MQTT and Home Assistant.
            const Link mqttOn{"D-13", "mqtt-off", cfg.mqtt.enabled};
            b.add("mqtt.homeAssistant.enabled", "D-13", cfg.mqtt.homeAssistant, {mqttOn});
            b.add("mqtt.homeAssistant.alertsSwitch", "D-13", cfg.mqtt.homeAssistant, {mqttOn, {"D-13", "alerts-off", a.enabled || phonesRing}});
            const Link publishMqtt{"D-14", "mqtt-off", cfg.mqtt.enabled};
            b.add("mqtt.publish.gps", "D-14", cfg.mqtt.publish.gps,
                  {publishMqtt, {"D-14", "gps-off", cfg.gps.enabled}, {"D-35", "gps-restart", f.gpsRunning}}, Unmet::None, true);
            b.add("mqtt.publish.rain", "D-14", cfg.mqtt.publish.rain, {publishMqtt, {"D-14", "rain-off", cfg.rain.enabled}}, Unmet::None, true);
            b.add("mqtt.publish.wind", "D-14", cfg.mqtt.publish.wind, {publishMqtt, {"D-14", "wind-off", cfg.wind.enabled}}, Unmet::None, true);

            // Safety rules (unmet behaviour confirmed from SafetyEvaluator).
            const AlpacaConfig &s = cfg.alpaca;
            const Link rainRule{"D-15", "rain-off", cfg.rain.enabled};
            b.add("alpaca.rainUnsafeEnabled", "D-15", s.rainUnsafeEnabled, {rainRule}, Unmet::Inactive);
            b.add("alpaca.rainSensorRequired", "D-15", s.rainSensorRequired, {rainRule}, Unmet::Inactive);
            const Link windOn{"D-16", "wind-off", cfg.wind.enabled};
            b.add("alpaca.windSpeedUnsafeEnabled", "D-16", s.windSpeedUnsafeEnabled, {windOn}, Unmet::FailSafe);
            b.add("alpaca.windGustUnsafeEnabled", "D-16", s.windGustUnsafeEnabled, {windOn}, Unmet::FailSafe);
            b.add("alpaca.cloudCoverEnabled", "D-17", s.cloudCoverEnabled, {{"D-17", "infrared-missing", f.infraredDetected}}, Unmet::FailSafe);
            b.add("alpaca.sqmMinEnabled", "D-18", s.sqmMinEnabled, {{"D-18", "light-missing", f.lightDetected}}, Unmet::FailSafe);
            const Link bme{"D-19", "environment-missing", f.environmentDetected};
            b.add("alpaca.humidityMaxEnabled", "D-19", s.humidityMaxEnabled, {bme}, Unmet::FailSafe);
            b.add("alpaca.dewpointMarginEnabled", "D-19", s.dewpointMarginEnabled, {bme}, Unmet::FailSafe);

            // Sensors, time and location.
            b.add("wind.directionEnabled", "D-23", cfg.wind.directionEnabled, {{"D-23", "wind-off", cfg.wind.enabled}});
            b.add("rain.dailyResetEnabled", "D-24", cfg.rain.dailyResetEnabled,
                  {{"D-24", "rain-off", cfg.rain.enabled}, {"D-24", "clock-unset", f.clockSet}});
            b.add("location.showSunMoon", "D-25", cfg.location.showSunMoon, {{"D-25", "location-unknown", locationKnown}}, Unmet::None, true);
            b.add("gps.enabled", "D-26", cfg.gps.enabled, {{"D-35", "gps-restart", f.gpsRunning}, {"D-26", "gps-no-fix", f.gpsFix}});
            b.add("ntp.enabled", "D-28", cfg.ntp.enabled, {{"D-28", "wifi-disconnected", f.wifiConnected}});
            b.add("skyCalibration.enabled", "D-29", cfg.skyCalibration.enabled, {{"D-29", "light-missing", f.lightDetected}});

            // Device.
            const Link bleBuild{"D-30", "ble-build", f.bluetoothBuild};
            const Link bleRunning{"D-35", "ble-restart", f.bluetoothRunning};
            b.add("ble.enabled", "D-30", cfg.ble.enabled, {bleBuild, bleRunning});
            b.add("ble.phoneAlarm", "D-31", phoneAlarm, {bleBuild, {"D-31", "ble-off", cfg.ble.enabled}, bleRunning});
            b.add("ota.enabled", "D-32", cfg.ota.enabled, {{"D-32", "ota-no-password", !cfg.ota.password.empty()}});
            b.add("wifi.mdns", "D-36", cfg.wifi.mdns, {{"D-36", "wifi-disconnected", f.wifiConnected}});
            return out;
        }

        const Entry *find(const std::vector<Entry> &entries, const char *setting)
        {
            for (const Entry &e : entries)
                if (std::strcmp(e.setting, setting) == 0)
                    return &e;
            return nullptr;
        }

        bool isActive(const std::vector<Entry> &entries, const char *setting)
        {
            const Entry *e = find(entries, setting);
            return e != nullptr && e->state == State::Active;
        }

        const Reason *reasonFor(const std::vector<Entry> &entries, const char *setting)
        {
            const Entry *e = find(entries, setting);
            return e != nullptr && e->state == State::Inactive ? e->reason : nullptr;
        }

        const std::vector<Reason> &reasons() { return REASONS; }

        std::vector<std::string> rulesNotInEffect(const Config &cfg)
        {
            std::vector<std::string> rules;
            if (cfg.rain.enabled)
                return rules;
            if (cfg.alpaca.rainUnsafeEnabled)
                rules.push_back("Unsafe while raining - rain sensor is off");
            if (cfg.alpaca.rainSensorRequired)
                rules.push_back("Unsafe if the rain sensor fails - rain sensor is off");
            return rules;
        }

        const char *stateName(State state)
        {
            switch (state)
            {
            case State::Active:
                return "active";
            case State::Inactive:
                return "inactive";
            default:
                return "off";
            }
        }

        const char *unmetName(Unmet unmet)
        {
            switch (unmet)
            {
            case Unmet::Inactive:
                return "inactive";
            case Unmet::FailSafe:
                return "fail-safe";
            default:
                return nullptr;
            }
        }

        void writeFacts(JsonObject target, const Facts &f)
        {
            target["wifiConnected"] = f.wifiConnected;
            target["mqttConnected"] = f.mqttConnected;
            target["clockSet"] = f.clockSet;
            target["gpsRunning"] = f.gpsRunning;
            target["gpsFix"] = f.gpsFix;
            target["bluetoothBuild"] = f.bluetoothBuild;
            target["bluetoothRunning"] = f.bluetoothRunning;
            target["pairedPhones"] = f.pairedPhones;
            target["lightDetected"] = f.lightDetected;
            target["infraredDetected"] = f.infraredDetected;
            target["environmentDetected"] = f.environmentDetected;
        }

        size_t reportCapacity(const std::vector<Entry> &entries)
        {
            return JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(11) + JSON_ARRAY_SIZE(entries.size()) + entries.size() * JSON_OBJECT_SIZE(8);
        }

        void writeReport(JsonObject root, const std::vector<Entry> &entries, const Facts &facts)
        {
            writeFacts(root.createNestedObject("facts"), facts);
            JsonArray list = root.createNestedArray("settings");
            for (const Entry &e : entries)
            {
                JsonObject item = list.createNestedObject();
                item["id"] = e.id;
                item["setting"] = e.setting;
                item["state"] = stateName(e.state);
                if (e.state == State::Inactive && e.reason != nullptr)
                {
                    item["reason"] = e.reason->code;
                    item["text"] = e.reason->text;
                    item["fix"] = e.reason->fix;
                }
                if (e.unmet != Unmet::None)
                    item["unmet"] = unmetName(e.unmet);
                if (e.neutral)
                    item["neutral"] = true;
            }
        }
    } // namespace Deps
} // namespace SQM
