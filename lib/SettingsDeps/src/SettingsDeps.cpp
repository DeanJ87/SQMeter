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
                {"light-missing", "Light sensor not detected", "sensors#sky-sensors"},
                {"infrared-missing", "IR sky sensor not detected", "sensors#sky-sensors"},
                {"environment-missing", "Environment sensor not detected", "sensors#sky-sensors"},
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

            // How a reported setting is shown: safety rules declare their unmet
            // behaviour; harmless defaults are shown muted.
            struct Shown
            {
                Unmet unmet = Unmet::None;
                bool neutral = false;
            };
            constexpr Shown NEUTRAL{Unmet::None, true};
            constexpr Shown FAIL_SAFE{Unmet::FailSafe, false};

            // What every section needs, worked out once.
            struct Context
            {
                const Config &cfg;
                const Facts &f;
                std::vector<Entry> &out;
                bool locationKnown;
                bool phonesRing; // paired phones ring for Wake-level events, alerts on or not

                // `on`: the setting's own switch. The first unmet link decides.
                void add(const char *setting, const char *id, bool on, std::initializer_list<Link> chain, Shown shown = {}) const
                {
                    Entry e;
                    e.setting = setting;
                    e.id = id;
                    e.unmet = shown.unmet;
                    e.neutral = shown.neutral;
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
                for (const AlertsConfig::EventSetting *e :
                     {&a.unsafe,
                      &a.safe,
                      &a.rainStarted,
                      &a.rainStopped,
                      &a.sensorFault,
                      &a.sensorRecovered,
                      &a.dewRisk,
                      &a.clearSky,
                      &a.cloudedOver,
                      &a.clientLost,
                      &a.clientBack,
                      &a.clientDisconnected})
                    if (e->level == level)
                        return true;
                return false;
            }

            // Alert channels are push only: they need the master switch. Links
            // in catalogue order, so the channel's own dependency comes before
            // "alerts are off" - a test send ignores the latter.
            void addAlertChannels(const Context &c)
            {
                const AlertsConfig &a = c.cfg.alerts;
                const Link alertsOn{"D-04", "alerts-off", a.enabled};
                const Link wifi{"D-03", "wifi-disconnected", c.f.wifiConnected};
                c.add("alerts.pushover.enabled", "D-03", a.pushoverEnabled, {wifi, alertsOn});
                c.add("alerts.ntfy.enabled", "D-03", a.ntfyEnabled, {wifi, alertsOn});
                c.add("alerts.webhook.enabled", "D-03", a.webhookEnabled, {wifi, alertsOn});
                const Link mqttOn{"D-01", "mqtt-off", c.cfg.mqtt.enabled};
                const Link broker{"D-02", "mqtt-disconnected", c.f.mqttConnected};
                c.add("alerts.mqtt.enabled", "D-01", a.mqttEnabled, {mqttOn, broker, alertsOn});
            }

            void addAlertEvents(const Context &c)
            {
                const AlertsConfig &a = c.cfg.alerts;
                // Wake-level events still ring paired phones with push alerts off.
                auto event = [&](const AlertsConfig::EventSetting &e) -> Link
                { return {"D-04", "alerts-off", a.enabled || (e.level == 4 && c.phonesRing)}; };
                const Link rainOn{"D-05", "rain-off", c.cfg.rain.enabled};
                const Link bmeFound{"D-06", "environment-missing", c.f.environmentDetected};
                const Link mlxFound{"D-07", "infrared-missing", c.f.infraredDetected};
                c.add("alerts.events.unsafe.level", "D-04", a.unsafe.level != 0, {event(a.unsafe)});
                c.add("alerts.events.safe.level", "D-04", a.safe.level != 0, {event(a.safe)});
                c.add("alerts.events.rain_started.level", "D-05", a.rainStarted.level != 0, {event(a.rainStarted), rainOn});
                c.add("alerts.events.rain_stopped.level", "D-05", a.rainStopped.level != 0, {event(a.rainStopped), rainOn});
                c.add("alerts.events.sensor_fault.level", "D-04", a.sensorFault.level != 0, {event(a.sensorFault)});
                c.add("alerts.events.sensor_recovered.level", "D-04", a.sensorRecovered.level != 0, {event(a.sensorRecovered)});
                c.add("alerts.events.dew_risk.level", "D-06", a.dewRisk.level != 0, {event(a.dewRisk), bmeFound});
                c.add("alerts.events.clear_sky.level", "D-07", a.clearSky.level != 0, {event(a.clearSky), mlxFound});
                c.add("alerts.events.clouded_over.level", "D-07", a.cloudedOver.level != 0, {event(a.cloudedOver), mlxFound});
                // Imaging apps connect over Alpaca (spec 021).
                const Link alpacaOn{"D-37", "alpaca-off", c.cfg.alpaca.enabled};
                c.add("alerts.events.client_lost.level", "D-37", a.clientLost.level != 0, {event(a.clientLost), alpacaOn});
                c.add("alerts.events.client_back.level", "D-37", a.clientBack.level != 0, {event(a.clientBack), alpacaOn});
                c.add(
                    "alerts.events.client_disconnected.level",
                    "D-37",
                    a.clientDisconnected.level != 0,
                    {event(a.clientDisconnected), alpacaOn});
            }

            void addAlertOptions(const Context &c)
            {
                const AlertsConfig &a = c.cfg.alerts;
                const BleConfig &ble = c.cfg.ble;
                // "Wake me" also escalates Pushover and ntfy; only the phone ringing
                // needs Bluetooth. Shown muted: the default events at Wake me
                // shouldn't warn on a standard build.
                c.add(
                    "alerts.wakePhones",
                    "D-08",
                    anyEventAt(a, 4),
                    {{"D-08", "ble-build", c.f.bluetoothBuild},
                     {"D-08", "ble-off", ble.enabled},
                     {"D-35", "ble-restart", c.f.bluetoothRunning},
                     {"D-08", "no-passkey", !ble.passkey.empty()},
                     {"D-08", "no-phones", c.f.pairedPhones > 0}},
                    NEUTRAL);
                const Link alerting{"D-04", "alerts-off", a.enabled || c.phonesRing};
                c.add("alerts.skyNightOnly", "D-09", a.skyNightOnly, {alerting, {"D-09", "location-unknown", c.locationKnown}});
                c.add("alerts.safetyNightOnly", "D-10", a.safetyNightOnly, {alerting, {"D-10", "location-unknown", c.locationKnown}});
                c.add("alerts.nightSunAltitudeDeg", "D-11", a.skyNightOnly || a.safetyNightOnly, {alerting});
                c.add(
                    "alerts.sendMode",
                    "D-12",
                    a.sendMode == AlertsConfig::SendMode::WhileConnected,
                    {alerting, {"D-12", "alpaca-off", c.cfg.alpaca.enabled}});
            }

            void addMqtt(const Context &c)
            {
                const MQTTConfig &mqtt = c.cfg.mqtt;
                const Link mqttOn{"D-13", "mqtt-off", mqtt.enabled};
                c.add("mqtt.homeAssistant.enabled", "D-13", mqtt.homeAssistant, {mqttOn});
                const Link alerting{"D-13", "alerts-off", c.cfg.alerts.enabled || c.phonesRing};
                c.add("mqtt.homeAssistant.alertsSwitch", "D-13", mqtt.homeAssistant, {mqttOn, alerting});
                // Publishing a switched-off sensor sends nothing: harmless, shown muted.
                const Link publishMqtt{"D-14", "mqtt-off", mqtt.enabled};
                const Link gpsRunning{"D-35", "gps-restart", c.f.gpsRunning};
                c.add(
                    "mqtt.publish.gps",
                    "D-14",
                    mqtt.publish.gps,
                    {publishMqtt, {"D-14", "gps-off", c.cfg.gps.enabled}, gpsRunning},
                    NEUTRAL);
                c.add("mqtt.publish.rain", "D-14", mqtt.publish.rain, {publishMqtt, {"D-14", "rain-off", c.cfg.rain.enabled}}, NEUTRAL);
                c.add("mqtt.publish.wind", "D-14", mqtt.publish.wind, {publishMqtt, {"D-14", "wind-off", c.cfg.wind.enabled}}, NEUTRAL);
            }

            // Unmet behaviour confirmed from SafetyEvaluator (research R4).
            void addSafetyRules(const Context &c)
            {
                const AlpacaConfig &s = c.cfg.alpaca;
                // Both ship on while the rain sensor ships off: not in effect, shown muted.
                const Link rainOn{"D-15", "rain-off", c.cfg.rain.enabled};
                const Shown ignored{Unmet::Inactive, true};
                c.add("alpaca.rainUnsafeEnabled", "D-15", s.rainUnsafeEnabled, {rainOn}, ignored);
                c.add("alpaca.rainSensorRequired", "D-15", s.rainSensorRequired, {rainOn}, ignored);
                const Link windOn{"D-16", "wind-off", c.cfg.wind.enabled};
                c.add("alpaca.windSpeedUnsafeEnabled", "D-16", s.windSpeedUnsafeEnabled, {windOn}, FAIL_SAFE);
                c.add("alpaca.windGustUnsafeEnabled", "D-16", s.windGustUnsafeEnabled, {windOn}, FAIL_SAFE);
                c.add(
                    "alpaca.cloudCoverEnabled",
                    "D-17",
                    s.cloudCoverEnabled,
                    {{"D-17", "infrared-missing", c.f.infraredDetected}},
                    FAIL_SAFE);
                c.add("alpaca.sqmMinEnabled", "D-18", s.sqmMinEnabled, {{"D-18", "light-missing", c.f.lightDetected}}, FAIL_SAFE);
                const Link bme{"D-19", "environment-missing", c.f.environmentDetected};
                c.add("alpaca.humidityMaxEnabled", "D-19", s.humidityMaxEnabled, {bme}, FAIL_SAFE);
                c.add("alpaca.dewpointMarginEnabled", "D-19", s.dewpointMarginEnabled, {bme}, FAIL_SAFE);
            }

            void addSensorsAndTime(const Context &c)
            {
                const Config &cfg = c.cfg;
                c.add("wind.directionEnabled", "D-23", cfg.wind.directionEnabled, {{"D-23", "wind-off", cfg.wind.enabled}});
                const Link rainOn{"D-24", "rain-off", cfg.rain.enabled};
                c.add("rain.dailyResetEnabled", "D-24", cfg.rain.dailyResetEnabled, {rainOn, {"D-24", "clock-unset", c.f.clockSet}});
                c.add("location.showSunMoon", "D-25", cfg.location.showSunMoon, {{"D-25", "location-unknown", c.locationKnown}}, NEUTRAL);
                const Link gpsRunning{"D-35", "gps-restart", c.f.gpsRunning};
                c.add("gps.enabled", "D-26", cfg.gps.enabled, {gpsRunning, {"D-26", "gps-no-fix", c.f.gpsFix}});
                c.add("ntp.enabled", "D-28", cfg.ntp.enabled, {{"D-28", "wifi-disconnected", c.f.wifiConnected}});
                c.add("skyCalibration.enabled", "D-29", cfg.skyCalibration.enabled, {{"D-29", "light-missing", c.f.lightDetected}});
            }

            void addDevice(const Context &c)
            {
                const Config &cfg = c.cfg;
                const Link bleBuild{"D-30", "ble-build", c.f.bluetoothBuild};
                const Link bleRunning{"D-35", "ble-restart", c.f.bluetoothRunning};
                c.add("ble.enabled", "D-30", cfg.ble.enabled, {bleBuild, bleRunning});
                c.add("ble.phoneAlarm", "D-31", !cfg.ble.passkey.empty(), {bleBuild, {"D-31", "ble-off", cfg.ble.enabled}, bleRunning});
                c.add("ota.enabled", "D-32", cfg.ota.enabled, {{"D-32", "ota-no-password", !cfg.ota.password.empty()}});
                c.add("wifi.mdns", "D-36", cfg.wifi.mdns, {{"D-36", "wifi-disconnected", c.f.wifiConnected}});
                c.add("wifi.ipv6", "D-36", cfg.wifi.ipv6, {{"D-36", "wifi-disconnected", c.f.wifiConnected}});
            }
        } // namespace

        std::vector<Entry> evaluate(const Config &cfg, const Facts &f)
        {
            std::vector<Entry> out;
            out.reserve(44);
            // Paired phones ring for Wake-level events even with push alerts off
            // (WebServer::processAlerts), so events, arming and the night-only
            // options still matter to them.
            const bool phonesRing =
                f.bluetoothBuild && cfg.ble.enabled && f.bluetoothRunning && !cfg.ble.passkey.empty() && f.pairedPhones > 0;
            const Context c{cfg, f, out, cfg.location.set || (f.gpsRunning && f.gpsFix), phonesRing};
            addAlertChannels(c);
            addAlertEvents(c);
            addAlertOptions(c);
            addMqtt(c);
            addSafetyRules(c);
            addSensorsAndTime(c);
            addDevice(c);
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

        const std::vector<Reason> &reasons()
        {
            return REASONS;
        }

        std::vector<std::string> rulesNotInEffect(const Config &cfg)
        {
            std::vector<std::string> rules;
            if (cfg.rain.enabled)
                return rules;
            if (cfg.alpaca.rainUnsafeEnabled)
                rules.push_back("Unsafe while raining");
            if (cfg.alpaca.rainSensorRequired)
                rules.push_back("Unsafe if the rain sensor fails");
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
