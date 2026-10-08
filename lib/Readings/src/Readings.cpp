#include "Readings.h"

#include <cmath>

namespace SQM
{
    namespace Readings
    {
        namespace
        {
            // Rounded so payloads read 21.48, not 21.47999954.
            double rounded(double value, int decimals)
            {
                const double scale = std::pow(10.0, decimals);
                return std::round(value * scale) / scale;
            }

            // A sensor that has never answered (missing) has no reading to be
            // old, so it gets no ageMs.
            JsonObject group(JsonObject root, const char *name, Status status, uint32_t ageMs)
            {
                JsonObject object = root.createNestedObject(name);
                object["status"] = statusName(status);
                if (status != Status::Missing)
                    object["ageMs"] = ageMs;
                return object;
            }
        } // namespace

        const char *statusName(Status status)
        {
            switch (status)
            {
            case Status::Ok:
                return "ok";
            case Status::Error:
                return "error";
            case Status::Stale:
                return "stale";
            case Status::Missing:
            default:
                return "missing";
            }
        }

        void write(JsonObject root, const Snapshot &s, const Groups &groups)
        {
            root["timestamp"] = s.timeValid ? s.timestamp : 0;
            root["timeValid"] = s.timeValid;
            root["dataAgeMs"] = s.dataAgeMs;
            root["dataStale"] = s.dataStale;

            if (groups.sky)
            {
                JsonObject light = group(root, "light", s.light.status, s.light.ageMs);
                JsonObject sky = root.createNestedObject("sky");
                sky["status"] = statusName(s.light.status);
                if (s.light.status == Status::Ok)
                {
                    light["lux"] = rounded(s.light.lux, 6);
                    light["visible"] = s.light.visible;
                    light["infrared"] = s.light.infrared;
                    light["full"] = s.light.full;
                    light["gain"] = s.light.gain;
                    light["gainFactor"] = rounded(s.light.gainFactor, 1);
                    light["integrationMs"] = s.light.integrationMs;
                    light["saturated"] = s.light.saturated;
                    light["nightMode"] = s.light.nightMode;

                    sky["sqm"] = rounded(s.sky.sqm, 2);
                    sky["rawSqm"] = rounded(s.sky.rawSqm, 2);
                    sky["nelm"] = rounded(s.sky.nelm, 1);
                    sky["bortle"] = s.sky.bortle;
                    sky["description"] = s.sky.description;
                    sky["calibrated"] = s.sky.calibrated;
                    sky["averagingWindowSeconds"] = s.sky.averagingWindowSeconds;
                }
            }

            if (groups.environment)
            {
                JsonObject environment = group(root, "environment", s.environment.status, s.environment.ageMs);
                if (s.environment.status == Status::Ok)
                {
                    environment["temperature"] = rounded(s.environment.temperature, 1);
                    environment["humidity"] = rounded(s.environment.humidity, 1);
                    environment["pressure"] = rounded(s.environment.pressure, 1);
                    environment["dewpoint"] = rounded(s.environment.dewpoint, 1);
                }
            }

            if (groups.clouds)
            {
                JsonObject infrared = group(root, "infrared", s.infrared.status, s.infrared.ageMs);
                JsonObject clouds = root.createNestedObject("clouds");
                clouds["status"] = statusName(s.infrared.status);
                if (s.infrared.status == Status::Ok)
                {
                    infrared["skyTemperature"] = rounded(s.infrared.skyTemperature, 1);
                    infrared["ambientTemperature"] = rounded(s.infrared.ambientTemperature, 1);

                    clouds["coverPercent"] = rounded(s.clouds.coverPercent, 0);
                    clouds["condition"] = s.clouds.condition;
                    clouds["description"] = s.clouds.description;
                    clouds["temperatureDelta"] = rounded(s.clouds.temperatureDelta, 1);
                    clouds["correctedDelta"] = rounded(s.clouds.correctedDelta, 1);
                    clouds["humidity"] = rounded(s.clouds.humidity, 1);
                    clouds["humiditySource"] = s.clouds.humidityMeasured ? "measured" : "assumed";
                }
            }

            if (groups.gps && s.gps.present)
            {
                JsonObject gps = group(root, "gps", s.gps.status, s.gps.ageMs);
                if (s.gps.status == Status::Ok)
                {
                    gps["fix"] = s.gps.fix;
                    gps["satellites"] = s.gps.satellites;
                    if (s.gps.fix)
                    {
                        gps["latitude"] = rounded(s.gps.latitude, 6);
                        gps["longitude"] = rounded(s.gps.longitude, 6);
                        gps["altitude"] = rounded(s.gps.altitude, 1);
                        gps["hdop"] = rounded(s.gps.hdop, 1);
                    }
                }
            }

            if (groups.rain && s.rain.present)
            {
                JsonObject rain = group(root, "rain", s.rain.status, s.rain.ageMs);
                if (s.rain.status == Status::Ok)
                {
                    rain["raining"] = s.rain.raining;
                    rain["rainingNow"] = s.rain.rainingNow;
                    rain["intensity"] = rounded(s.rain.intensity, 2);
                    rain["eventAccumulation"] = rounded(s.rain.eventAccumulation, 2);
                    rain["sensorEventAccumulation"] = rounded(s.rain.sensorEventAccumulation, 2);
                    rain["totalAccumulation"] = rounded(s.rain.totalAccumulation, 2);
                    rain["lensFault"] = s.rain.lensFault;
                    rain["emitterSaturated"] = s.rain.emitterSaturated;
                }
            }

            if (groups.wind && s.wind.present)
            {
                JsonObject wind = group(root, "wind", s.wind.status, s.wind.ageMs);
                if (s.wind.status == Status::Ok)
                {
                    wind["speed"] = rounded(s.wind.speed, 1);
                    wind["gust"] = rounded(s.wind.gust, 1);
                    if (s.wind.directionValid)
                        wind["direction"] = rounded(s.wind.direction, 0);
                    wind["vaneFault"] = s.wind.vaneFault;
                }
            }
        }

        namespace
        {
            enum class Group : uint8_t
            {
                Sky,
                Environment,
                Clouds,
                Rain,
                Wind,
                Safety,
                Always,
            };

            struct Entity
            {
                const char *component;
                const char *object;
                const char *name;
                Group group;
                const char *jsonGroup;     // readings group whose status gates availability
                const char *value;         // value_template body (state topic) or nullptr
                const char *unit;
                const char *deviceClass;
                const char *icon;
            };

            constexpr Entity ENTITIES[] = {
                {"sensor", "sqm", "Sky quality", Group::Sky, "sky", "value_json.sky.sqm", "mag/arcsec²", nullptr, "mdi:star-four-points"},
                {"sensor", "nelm", "Limiting magnitude", Group::Sky, "sky", "value_json.sky.nelm", "mag", nullptr, "mdi:eye"},
                {"sensor", "bortle", "Bortle class", Group::Sky, "sky", "value_json.sky.bortle", nullptr, nullptr, "mdi:weather-night"},
                {"sensor", "illuminance", "Illuminance", Group::Sky, "light", "value_json.light.lux", "lx", "illuminance", nullptr},
                {"sensor", "temperature", "Temperature", Group::Environment, "environment", "value_json.environment.temperature", "°C", "temperature", nullptr},
                {"sensor", "humidity", "Humidity", Group::Environment, "environment", "value_json.environment.humidity", "%", "humidity", nullptr},
                {"sensor", "pressure", "Pressure", Group::Environment, "environment", "value_json.environment.pressure", "hPa", "atmospheric_pressure", nullptr},
                {"sensor", "dewpoint", "Dew point", Group::Environment, "environment", "value_json.environment.dewpoint", "°C", "temperature", nullptr},
                {"sensor", "sky_temperature", "Sky temperature", Group::Clouds, "infrared", "value_json.infrared.skyTemperature", "°C", "temperature", "mdi:thermometer-low"},
                {"sensor", "cloud_cover", "Cloud cover", Group::Clouds, "clouds", "value_json.clouds.coverPercent", "%", nullptr, "mdi:weather-cloudy"},
                {"binary_sensor", "raining", "Raining", Group::Rain, "rain", nullptr, nullptr, "moisture", nullptr},
                {"sensor", "rain_intensity", "Rain intensity", Group::Rain, "rain", "value_json.rain.intensity", "mm/h", "precipitation_intensity", nullptr},
                {"sensor", "wind_speed", "Wind speed", Group::Wind, "wind", "value_json.wind.speed", "m/s", "wind_speed", nullptr},
                {"sensor", "wind_gust", "Wind gust", Group::Wind, "wind", "value_json.wind.gust", "m/s", "wind_speed", nullptr},
                {"sensor", "wind_direction", "Wind direction", Group::Wind, "wind", "value_json.wind.direction", "°", nullptr, "mdi:compass"},
                {"binary_sensor", "safety", "Observatory", Group::Safety, nullptr, nullptr, nullptr, "safety", nullptr},
                {"switch", "alerts", "Alerts", Group::Always, nullptr, nullptr, nullptr, nullptr, "mdi:bell-ring"},
            };

            bool enabled(Group group, const Groups &groups, bool safety)
            {
                switch (group)
                {
                case Group::Sky:
                    return groups.sky;
                case Group::Environment:
                    return groups.environment;
                case Group::Clouds:
                    return groups.clouds;
                case Group::Rain:
                    return groups.rain;
                case Group::Wind:
                    return groups.wind;
                case Group::Safety:
                    return safety;
                case Group::Always:
                default:
                    return true;
                }
            }
        } // namespace

        void forEachDiscovery(const DiscoveryDevice &device, const Groups &groups, bool safety,
                              const std::function<void(const std::string &topic, const std::string &payload)> &publish)
        {
            const std::string base = device.baseTopic;
            for (const Entity &entity : ENTITIES)
            {
                const std::string topic = device.prefix + "/" + entity.component + "/" + device.id + "/" + entity.object + "/config";
                if (!enabled(entity.group, groups, safety))
                {
                    publish(topic, "");
                    continue;
                }

                DynamicJsonDocument doc(1536);
                doc["name"] = entity.name;
                doc["unique_id"] = device.id + "_" + entity.object;
                doc["object_id"] = device.id + "_" + entity.object;

                JsonArray availability = doc.createNestedArray("availability");
                JsonObject online = availability.createNestedObject();
                online["topic"] = base + "/availability";
                if (entity.jsonGroup != nullptr)
                {
                    JsonObject groupOk = availability.createNestedObject();
                    groupOk["topic"] = base + "/state";
                    groupOk["value_template"] =
                        std::string("{{ 'online' if value_json.") + entity.jsonGroup + ".status == 'ok' else 'offline' }}";
                    doc["availability_mode"] = "all";
                }

                if (entity.group == Group::Safety)
                {
                    // device_class safety: "on" means unsafe.
                    doc["state_topic"] = base + "/safe";
                    doc["payload_on"] = "0";
                    doc["payload_off"] = "1";
                }
                else if (entity.group == Group::Always)
                {
                    doc["state_topic"] = base + "/alerts/armed";
                    doc["command_topic"] = base + "/alerts/armed/set";
                    doc["payload_on"] = "1";
                    doc["payload_off"] = "0";
                    doc["state_on"] = "1";
                    doc["state_off"] = "0";
                }
                else if (entity.value == nullptr) // raining
                {
                    doc["state_topic"] = base + "/state";
                    doc["value_template"] = "{{ 'ON' if value_json.rain.raining else 'OFF' }}";
                }
                else
                {
                    doc["state_topic"] = base + "/state";
                    // Values can be absent (e.g. wind direction when calm): report unknown, not an error.
                    doc["value_template"] = std::string("{{ ") + entity.value + " if " + entity.value + " is defined else none }}";
                    doc["state_class"] = "measurement";
                }
                if (entity.unit != nullptr)
                    doc["unit_of_measurement"] = entity.unit;
                if (entity.deviceClass != nullptr)
                    doc["device_class"] = entity.deviceClass;
                if (entity.icon != nullptr)
                    doc["icon"] = entity.icon;

                JsonObject dev = doc.createNestedObject("device");
                dev.createNestedArray("identifiers").add(device.id);
                dev["name"] = device.name;
                dev["manufacturer"] = "SQMeter";
                dev["model"] = "SQMeter";
                dev["sw_version"] = device.version;

                std::string payload;
                serializeJson(doc, payload);
                publish(topic, payload);
            }
        }

    } // namespace Readings
} // namespace SQM
