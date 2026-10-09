#pragma once

// Whether each setting that depends on another setting (or on hardware, or
// on the network) is actually in effect - decided once, here, for the
// firmware, the native tests and the browser demo
// (specs/020-settings-dependencies). The catalogue it implements is
// lib/SettingsDeps/catalogue.json; web/src/lib/settingsDeps.ts mirrors it for
// previews of unsaved changes, held to the same answers by
// test/fixtures/settings-deps/cases.json.
//
// A dependent setting whose dependency is unmet is kept as saved and
// reported inactive; the device doesn't act on it (FR-001, FR-002).

#include <ArduinoJson.h>

#include <cstdint>
#include <string>
#include <vector>

#include "Config.h"

namespace SQM
{
    namespace Deps
    {
        enum class State : uint8_t
        {
            Off,      // the setting itself is off
            Active,   // on, and everything it needs is there
            Inactive, // on, but something it needs isn't (see reason)
        };

        // Safety rules declare what an unmet dependency means (FR-009).
        enum class Unmet : uint8_t
        {
            None,     // not a safety rule
            Inactive, // the rule is ignored and listed under rulesNotInEffect
            FailSafe, // the verdict reports unsafe
        };

        // What the device knows at run time. Rain and wind apply as soon as
        // settings are saved; GPS and Bluetooth only start at boot.
        struct Facts
        {
            bool wifiConnected = false; // joined a network (not the setup hotspot)
            bool mqttConnected = false;
            bool clockSet = false; // NTP or GPS time
            bool gpsRunning = false;
            bool gpsFix = false;
            bool bluetoothBuild = false;
            bool bluetoothRunning = false;
            uint8_t pairedPhones = 0;
            bool lightDetected = false;       // TSL2591
            bool infraredDetected = false;    // MLX90614
            bool environmentDetected = false; // BME280
        };

        struct Reason
        {
            const char *code; // e.g. "mqtt-off"
            const char *text; // e.g. "MQTT is off"
            const char *fix;  // "tab#anchor", or "restart"
        };

        struct Entry
        {
            const char *setting; // config JSON path, e.g. "alerts.mqtt.enabled"
            const char *id;      // catalogue ID of the deciding link
            State state = State::Off;
            const Reason *reason = nullptr; // when Inactive
            Unmet unmet = Unmet::None;
            bool neutral = false; // inactive by default and harmless: shown muted
        };

        // Every reported setting, in catalogue order.
        std::vector<Entry> evaluate(const Config &cfg, const Facts &facts);
        // nullptr if `setting` isn't reported.
        const Entry *find(const std::vector<Entry> &entries, const char *setting);
        bool isActive(const std::vector<Entry> &entries, const char *setting);
        // The reason a reported setting is inactive, or nullptr.
        const Reason *reasonFor(const std::vector<Entry> &entries, const char *setting);

        // Every reason the catalogue can give (for tests and the checker).
        const std::vector<Reason> &reasons();

        // Safety rules that are on but ignored because what they need is off
        // (unmet behaviour "inactive"), e.g. "Unsafe while raining - rain
        // sensor is off". Config-only, so the safety document can list them.
        std::vector<std::string> rulesNotInEffect(const Config &cfg);

        const char *stateName(State state);
        const char *unmetName(Unmet unmet);

        // GET /api/settings/effective: {"facts": {...}, "settings": [...]}.
        void writeReport(JsonObject root, const std::vector<Entry> &entries, const Facts &facts);
        void writeFacts(JsonObject target, const Facts &facts);
        // JsonDocument capacity writeReport needs for `entries` (strings are
        // referenced, not copied).
        size_t reportCapacity(const std::vector<Entry> &entries);
    } // namespace Deps
} // namespace SQM
