#pragma once

#include <ArduinoJson.h>

#include <cstdint>
#include <functional>
#include <string>

namespace SQM
{
    // The readings document served by GET /api/sensors, /ws/sensors and MQTT
    // <base>/state - one serializer, so every interface uses the same names,
    // units and presence rules (specs/013-data-interfaces/contracts/readings.md).
    namespace Readings
    {
        enum class Status : uint8_t
        {
            Ok,
            Missing, // not detected / not responding
            Error,   // read error or invalid data
            Stale,   // last good reading too old
        };

        const char *statusName(Status status);

        struct Light
        {
            Status status = Status::Missing;
            uint32_t ageMs = 0;
            double lux = 0.0;
            uint32_t visible = 0;
            uint32_t infrared = 0;
            uint32_t full = 0;
            const char *gain = "";
            double gainFactor = 0.0;
            uint32_t integrationMs = 0;
            bool saturated = false;
            bool nightMode = false;
        };

        // Derived from the light sensor; shares its status.
        struct Sky
        {
            double sqm = 0.0;
            double rawSqm = 0.0;
            double nelm = 0.0;
            int bortle = 0;
            const char *description = "";
            bool calibrated = false;
            uint32_t averagingWindowSeconds = 0;
        };

        struct Environment
        {
            Status status = Status::Missing;
            uint32_t ageMs = 0;
            double temperature = 0.0;
            double humidity = 0.0;
            double pressure = 0.0;
            double dewpoint = 0.0;
        };

        struct Infrared
        {
            Status status = Status::Missing;
            uint32_t ageMs = 0;
            double skyTemperature = 0.0;
            double ambientTemperature = 0.0;
        };

        // Derived from the IR sensor; shares its status.
        struct Clouds
        {
            double coverPercent = 0.0;
            const char *condition = ""; // clear | cloudy | overcast
            const char *description = "";
            double temperatureDelta = 0.0;
            double correctedDelta = 0.0;
            double humidity = 0.0;
            bool humidityMeasured = false;
        };

        struct Gps
        {
            bool present = false; // GPS enabled
            Status status = Status::Missing;
            uint32_t ageMs = 0;
            bool fix = false;
            uint32_t satellites = 0;
            double latitude = 0.0;
            double longitude = 0.0;
            double altitude = 0.0;
            double hdop = 0.0;
        };

        struct Rain
        {
            bool present = false; // rain sensor enabled
            Status status = Status::Missing;
            uint32_t ageMs = 0;
            bool raining = false;                 // latched for the clear delay
            bool rainingNow = false;              // instantaneous
            double intensity = 0.0;               // mm/h
            double eventAccumulation = 0.0;       // mm, device event (clear delay)
            double sensorEventAccumulation = 0.0; // mm, RG-15's own event
            double totalAccumulation = 0.0;       // mm
            bool lensFault = false;
            bool emitterSaturated = false;
        };

        struct Wind
        {
            bool present = false; // anemometer enabled
            Status status = Status::Missing;
            uint32_t ageMs = 0;
            double speed = 0.0; // m/s, 2-minute mean
            double gust = 0.0;  // m/s
            bool directionValid = false;
            double direction = 0.0; // degrees, 0 = north
            bool vaneFault = false;
        };

        struct Snapshot
        {
            int64_t timestamp = 0; // Unix seconds, 0 when the clock isn't set
            bool timeValid = false;
            uint32_t dataAgeMs = 0;
            bool dataStale = true;
            Light light;
            Sky sky;
            Environment environment;
            Infrared infrared;
            Clouds clouds;
            Gps gps;
            Rain rain;
            Wind wind;
        };

        // Which groups to include (MQTT publish settings). REST/WebSocket use all.
        struct Groups
        {
            bool sky = true; // light + sky
            bool environment = true;
            bool clouds = true; // infrared + clouds
            bool gps = true;
            bool rain = true;
            bool wind = true;
            // Home Assistant's Alerts switch: only while alerts can go out
            // (specs/020-settings-dependencies D-13).
            bool alertsSwitch = true;
        };

        // Writes the document's fields into `root`. Groups of a sensor that isn't
        // ok carry only status (and ageMs) - never zeros that look like readings.
        void write(JsonObject root, const Snapshot &snapshot, const Groups &groups = Groups{});

        // Home Assistant MQTT discovery.
        struct DiscoveryDevice
        {
            std::string id;        // "sqmeter_<mac hex>"
            std::string name;      // device name
            std::string version;   // firmware version
            std::string baseTopic; // MQTT base topic
            std::string prefix;    // discovery prefix, usually "homeassistant"
        };

        // Calls `publish(topic, payload)` for every entity: its config when its
        // group is enabled, an empty payload (removes the entity) when not.
        void forEachDiscovery(
            const DiscoveryDevice &device,
            const Groups &groups,
            bool safety,
            const std::function<void(const std::string &topic, const std::string &payload)> &publish);

    } // namespace Readings
} // namespace SQM
