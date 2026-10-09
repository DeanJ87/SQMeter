#include "DeviceCore.h"

#include "SunPosition.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <ctime>

namespace SQM
{
    namespace Core
    {
        namespace
        {
            // Rounded so documents read 51.4779, not 51.47790146.
            double rounded(double value, int decimals)
            {
                const double scale = std::pow(10.0, decimals);
                return std::round(value * scale) / scale;
            }

            // ok unless the driver reports a problem or the last good reading is too old.
            Readings::Status readingStatus(SensorStatus status, bool initialized, uint32_t lastUpdate, uint32_t now, uint32_t staleAfter)
            {
                if (!initialized || status == SensorStatus::NotInitialized)
                    return Readings::Status::Missing;
                if (status != SensorStatus::Ok)
                    return Readings::Status::Error;
                if (lastUpdate == 0 || now - lastUpdate > staleAfter)
                    return Readings::Status::Stale;
                return Readings::Status::Ok;
            }

            const char *cloudConditionName(CloudCondition condition)
            {
                switch (condition)
                {
                case CloudCondition::CLEAR:
                    return "clear";
                case CloudCondition::CLOUDY:
                    return "cloudy";
                case CloudCondition::OVERCAST:
                    return "overcast";
                default:
                    return "unknown";
                }
            }

            const char *rg15StateName(RG15State state)
            {
                switch (state)
                {
                case RG15State::Disabled:
                    return "disabled";
                case RG15State::Configured:
                    return "configured";
                case RG15State::UartOpened:
                    return "uart_opened";
                case RG15State::Configuring:
                    return "configuring";
                case RG15State::CommandSent:
                    return "command_sent";
                case RG15State::AwaitingResponse:
                    return "awaiting_response";
                case RG15State::Acknowledged:
                    return "acknowledged";
                case RG15State::ReadingReceived:
                    return "reading_received";
                case RG15State::ParseError:
                    return "parse_error";
                case RG15State::Timeout:
                    return "timeout";
                case RG15State::Stale:
                    return "stale";
                case RG15State::Online:
                    return "online";
                default:
                    return "unknown";
                }
            }

            void putOptional(JsonObject obj, const char *key, const std::optional<std::string> &value)
            {
                if (value)
                    obj[key] = value->c_str();
                else
                    obj[key] = nullptr;
            }

            // Bring-up diagnostics: ages, not boot timestamps; camelCase.
            void writeRainDiagnostics(JsonObject root, const RG15Diagnostics &diag, uint32_t now)
            {
                auto age = [&root, now](const char *key, uint32_t at)
                {
                    if (at != 0)
                        root[key] = now - at;
                };
                root["state"] = rg15StateName(diag.state);
                root["uartOpened"] = diag.uartOpened;
                root["rxPin"] = diag.rxPin;
                root["txPin"] = diag.txPin;
                root["baudRate"] = diag.baudRate;
                root["uartPort"] = diag.uartPort;
                putOptional(root, "lastCommand", diag.lastCommand);
                putOptional(root, "lastAck", diag.lastAck);
                putOptional(root, "lastResponse", diag.lastRawResponse);
                putOptional(root, "lastError", diag.lastError);
                root["timeouts"] = diag.timeouts;
                root["parseErrors"] = diag.parseErrors;
                root["successfulReads"] = diag.successfulReads;
                age("lastPollAgeMs", diag.lastPollMs);
                age("lastResponseAgeMs", diag.lastResponseMs);
                age("lastSuccessfulReadAgeMs", diag.lastSuccessfulReadMs);
                age("lastRainDetectedAgeMs", diag.lastRainDetectedMs);
                age("lastTotalResetAgeMs", diag.lastTotalResetMs);
                age("lastRebootAgeMs", diag.lastRebootCommandMs);
                putOptional(root, "softwareVersion", diag.softwareVersion);
                putOptional(root, "softwareBuildDate", diag.softwareBuildDate);
                putOptional(root, "resetReason", diag.resetReason);
                if (diag.powerOnDays)
                    root["powerOnDays"] = *diag.powerOnDays;
                if (diag.emitterTotal)
                    root["emitterTotal"] = *diag.emitterTotal;
            }

            void writeLightDiagnostics(JsonObject root, const TSL2591Diagnostics &diag)
            {
                root["rollingVisible"] = diag.rollingVisible;
                root["correctedVisible"] = diag.correctedVisible;
                root["darkVisibleOffset"] = diag.darkVisibleOffset;
                root["sampleCount"] = diag.sampleCount;
                root["windowSamples"] = windowSamples(diag);
                root["nightMode"] = diag.nightMode;
                root["rejectedSamples"] = diag.rejectedSamples;
                root["consecutiveSaturatedSamples"] = diag.consecutiveSaturatedSamples;
                root["consecutiveLowSamples"] = diag.consecutiveLowSamples;
            }

            // The anemometer samples every second; allow a few missed ticks.
            constexpr uint32_t WIND_STALE_MS = 5000;

            bool windFresh(const WindReading &wind, uint32_t now)
            {
                return wind.status == SensorStatus::Ok && wind.timestamp != 0 && ageMs(now, wind.timestamp) <= WIND_STALE_MS;
            }
        } // namespace

        // One sample per integration, capped by the sensor's buffer.
        uint16_t windowSamples(const TSL2591Diagnostics &diag)
        {
            if (diag.integrationMs == 0)
                return 0;
            const uint32_t samples = static_cast<uint32_t>(diag.averagingWindowSeconds) * 1000UL / diag.integrationMs;
            return static_cast<uint16_t>(std::max<uint32_t>(1, std::min<uint32_t>(samples, 512)));
        }

        void derive(SensorSnapshot &snapshot, const Config &cfg)
        {
            snapshot.sky = SkyQuality::calculate(snapshot.tsl.lux);
            snapshot.humidityMeasured = snapshot.bmeInitialized && snapshot.bme.status == SensorStatus::Ok;
            snapshot.cloudHumidity = snapshot.humidityMeasured ? snapshot.bme.humidity : ASSUMED_HUMIDITY_PERCENT;
            snapshot.cloud = CloudDetection::calculate(
                snapshot.mlx.objectTemp,
                snapshot.mlx.ambientTemp,
                snapshot.cloudHumidity,
                {cfg.cloudDetection.clearSkyThreshold, cfg.cloudDetection.cloudyThreshold, cfg.cloudDetection.humidityCorrection});
        }

        namespace
        {
            // Light, sky quality, environment, IR and clouds: the I2C sensors.
            void readSkyGroups(Readings::Snapshot &r, const SensorSnapshot &snapshot, uint32_t now, uint32_t staleAfter)
            {
                // Light sensor + sky quality. The TSL2591 samples on its own ~600 ms cadence.
                const TSL2591Reading &tsl = snapshot.tsl;
                r.light.status = readingStatus(tsl.status, snapshot.tslInitialized, snapshot.tslLastUpdate, now, staleAfter);
                r.light.ageMs = ageMs(now, tsl.timestamp);
                r.light.lux = tsl.lux;
                r.light.visible = tsl.visible;
                r.light.infrared = tsl.infrared;
                r.light.full = tsl.full;
                r.light.gain = snapshot.tslDiagnostics.gainName != nullptr ? snapshot.tslDiagnostics.gainName : "";
                r.light.gainFactor = snapshot.tslDiagnostics.gainFactor;
                r.light.integrationMs = snapshot.tslDiagnostics.integrationMs;
                r.light.saturated = snapshot.tslDiagnostics.saturated;
                r.light.nightMode = snapshot.tslDiagnostics.nightMode;
                const SkyQualityMetrics &sky = snapshot.sky;
                r.sky.sqm = sky.sqm;
                r.sky.rawSqm = tsl.rawSqm;
                r.sky.nelm = sky.nelm;
                r.sky.bortle = static_cast<int>(std::lround(sky.bortle));
                r.sky.description = SkyQuality::getBortleDescription(sky.bortle);
                r.sky.calibrated = snapshot.tslDiagnostics.calibrated;
                r.sky.averagingWindowSeconds = snapshot.tslDiagnostics.averagingWindowSeconds;

                const BME280Reading &bme = snapshot.bme;
                r.environment.status = readingStatus(bme.status, snapshot.bmeInitialized, snapshot.bmeLastUpdate, now, staleAfter);
                r.environment.ageMs = ageMs(now, bme.timestamp);
                r.environment.temperature = bme.temperature;
                r.environment.humidity = bme.humidity;
                r.environment.pressure = bme.pressure;
                r.environment.dewpoint = bme.dewpoint;

                const MLX90614Reading &mlx = snapshot.mlx;
                r.infrared.status = readingStatus(mlx.status, snapshot.mlxInitialized, snapshot.mlxLastUpdate, now, staleAfter);
                r.infrared.ageMs = ageMs(now, mlx.timestamp);
                r.infrared.skyTemperature = mlx.objectTemp;
                r.infrared.ambientTemperature = mlx.ambientTemp;
                // Without the BME280 the cloud model assumes 53% humidity, and says so.
                const CloudMetrics &cloud = snapshot.cloud;
                r.clouds.coverPercent = cloud.cloudCoverPercent;
                r.clouds.condition = cloudConditionName(cloud.condition);
                r.clouds.description = cloud.description;
                r.clouds.temperatureDelta = cloud.temperatureDelta;
                r.clouds.correctedDelta = cloud.correctedDelta;
                r.clouds.humidity = snapshot.cloudHumidity;
                r.clouds.humidityMeasured = snapshot.humidityMeasured;
            }

            void readGps(Readings::Snapshot &r, const SensorSnapshot &snapshot, const Config &cfg)
            {
                r.gps.present = cfg.gps.enabled;
                if (r.gps.present)
                {
                    const GPSReading &gps = snapshot.gps;
                    r.gps.status = snapshot.gpsInitialized
                                       ? (gps.status == SensorStatus::Ok || gps.status == SensorStatus::Timeout ? Readings::Status::Ok
                                                                                                                : Readings::Status::Error)
                                       : Readings::Status::Missing;
                    r.gps.ageMs = gps.age;
                    r.gps.fix = gps.hasFix;
                    r.gps.satellites = gps.satellites;
                    r.gps.latitude = gps.latitude;
                    r.gps.longitude = gps.longitude;
                    r.gps.altitude = gps.altitude;
                    r.gps.hdop = gps.hdop / 100.0;
                }
            }

            void readRain(Readings::Snapshot &r, const SensorSnapshot &snapshot, const Config &cfg, uint32_t now)
            {
                r.rain.present = cfg.rain.enabled;
                if (r.rain.present)
                {
                    const RG15Reading &rain = snapshot.rg15;
                    r.rain.status = !snapshot.rg15Initialized || !rain.online ? Readings::Status::Missing
                                    : rain.stale                              ? Readings::Status::Stale
                                                                              : Readings::Status::Ok;
                    r.rain.ageMs = ageMs(now, rain.timestamp);
                    // Always metric: the RG-15 can be switched to inches.
                    const double toMm = rain.imperial ? 25.4 : 1.0;
                    r.rain.raining = rain.isRaining || rain.rainLatched;
                    r.rain.rainingNow = rain.isRaining;
                    r.rain.intensity = rain.rInt * toMm;
                    r.rain.eventAccumulation = rain.localEventAcc * toMm;
                    r.rain.sensorEventAccumulation = rain.eventAcc * toMm;
                    r.rain.totalAccumulation = rain.totalAcc * toMm;
                    r.rain.lensFault = rain.lensBad;
                    r.rain.emitterSaturated = rain.emSat;
                }
            }

            void readWind(Readings::Snapshot &r, const SensorSnapshot &snapshot, const Config &cfg, uint32_t now)
            {
                r.wind.present = cfg.wind.enabled;
                if (r.wind.present)
                {
                    const WindReading &wind = snapshot.wind;
                    r.wind.status = wind.status == SensorStatus::Ok               ? Readings::Status::Ok
                                    : wind.status == SensorStatus::NotInitialized ? Readings::Status::Missing
                                                                                  : Readings::Status::Error;
                    r.wind.ageMs = ageMs(now, wind.timestamp);
                    r.wind.speed = wind.speedMs;
                    r.wind.gust = wind.gustMs;
                    r.wind.directionValid = cfg.wind.directionEnabled && wind.directionValid;
                    r.wind.direction = wind.directionDeg;
                    r.wind.vaneFault = wind.vaneFault;
                }
            }
        } // namespace

        Readings::Snapshot buildReadings(const SensorSnapshot &snapshot, const Config &cfg, uint32_t now, int64_t epoch)
        {
            const uint32_t staleAfter = cfg.sensor.readIntervalMs + SENSOR_STALE_GRACE_MS;
            Readings::Snapshot r;
            r.timeValid = epoch >= CLOCK_VALID_EPOCH;
            r.timestamp = r.timeValid ? epoch : 0;
            r.dataAgeMs = ageMs(now, snapshot.dataTimestamp);
            r.dataStale = snapshot.dataTimestamp == 0 || r.dataAgeMs > staleAfter;
            readSkyGroups(r, snapshot, now, staleAfter);
            readGps(r, snapshot, cfg);
            readRain(r, snapshot, cfg, now);
            readWind(r, snapshot, cfg, now);
            return r;
        }

        void writeDiagnostics(JsonObject root, const SensorSnapshot &snapshot, const Config &cfg, uint32_t now)
        {
            writeLightDiagnostics(root.createNestedObject("light"), snapshot.tslDiagnostics);
            if (cfg.rain.enabled)
                writeRainDiagnostics(root.createNestedObject("rain"), snapshot.rg15Diagnostics, now);
        }

        void writeSensorHealth(JsonObject sensors, const Readings::Snapshot &readings, const Config &cfg)
        {
            auto health = [&sensors](const char *name, Readings::Status status, uint32_t age) -> JsonObject
            {
                JsonObject sensor = sensors.createNestedObject(name);
                sensor["status"] = Readings::statusName(status);
                if (status != Readings::Status::Missing) // never answered: no age
                    sensor["ageMs"] = age;
                return sensor;
            };
            health("light", readings.light.status, readings.light.ageMs);
            health("environment", readings.environment.status, readings.environment.ageMs);
            health("infrared", readings.infrared.status, readings.infrared.ageMs);
            if (readings.gps.present)
                health("gps", readings.gps.status, readings.gps.ageMs);
            if (readings.rain.present)
                health("rain", readings.rain.status, readings.rain.ageMs);
            if (readings.wind.present)
            {
                JsonObject wind = health("wind", readings.wind.status, readings.wind.ageMs);
                wind["vaneStatus"] = !cfg.wind.directionEnabled ? "off" : readings.wind.vaneFault ? "fault" : "ok";
            }
        }

        void sensorFacts(Deps::Facts &facts, const SensorSnapshot &snapshot, const Readings::Snapshot &readings)
        {
            // Found and answering; a stale reading still means it's there.
            auto detected = [](Readings::Status status) { return status == Readings::Status::Ok || status == Readings::Status::Stale; };
            facts.lightDetected = detected(readings.light.status);
            facts.infraredDetected = detected(readings.infrared.status);
            facts.environmentDetected = detected(readings.environment.status);
            facts.gpsRunning = snapshot.gpsInitialized;
            facts.gpsFix = snapshot.gpsInitialized && snapshot.gps.hasFix;
        }

        namespace
        {
            // Age of the sky data the verdict rests on: the older of the two
            // required sensors' last successful reads (TSL2591, MLX90614), not the
            // time the read loop last ran - a sensor that keeps reporting OK
            // without a fresh read is stale (specs/006 US1/AC4). A sensor that is
            // faulted is reported as a fault instead, so only answering sensors
            // count; with neither answering, the youngest successful read does.
            struct RequiredDataAge
            {
                bool everRead = false;
                uint32_t ageMs = 0;
            };

            RequiredDataAge requiredDataAge(const SensorSnapshot &snapshot, uint32_t now)
            {
                struct Source
                {
                    SensorStatus status;
                    uint32_t lastUpdate;
                };
                const Source sources[] = {{snapshot.tsl.status, snapshot.tslLastUpdate}, {snapshot.mlx.status, snapshot.mlxLastUpdate}};
                RequiredDataAge result;
                bool anyAnswering = false;
                uint32_t oldestAnswering = 0;
                uint32_t youngestRead = UINT32_MAX;
                for (const Source &source : sources)
                {
                    if (source.lastUpdate == 0)
                        continue;
                    result.everRead = true;
                    const uint32_t age = ageMs(now, source.lastUpdate);
                    youngestRead = std::min(youngestRead, age);
                    if (source.status == SensorStatus::Ok)
                    {
                        anyAnswering = true;
                        oldestAnswering = std::max(oldestAnswering, age);
                    }
                }
                if (result.everRead)
                    result.ageMs = anyAnswering ? oldestAnswering : youngestRead;
                return result;
            }
        } // namespace

        Alpaca::SafetyInputs safetyInputs(const SensorSnapshot &snapshot, const Config &cfg, uint32_t now)
        {
            Alpaca::SafetyInputs in;
            const RequiredDataAge data = requiredDataAge(snapshot, now);
            in.hasEverHadGoodData = data.everRead;
            in.secondsSinceLastGoodData = data.ageMs / 1000;
            in.skyLightFault = snapshot.tsl.status != SensorStatus::Ok;
            in.irSkyFault = snapshot.mlx.status != SensorStatus::Ok;
            in.requiredSensorFault = snapshot.tsl.status != SensorStatus::Ok || snapshot.mlx.status != SensorStatus::Ok;

            in.sqm = snapshot.sky.sqm;
            in.cloudCoverPercent = snapshot.cloud.cloudCoverPercent;
            in.humidityPercent = snapshot.cloudHumidity;
            in.temperatureC = snapshot.bme.temperature;
            in.dewpointC = snapshot.bme.dewpoint;
            in.environmentSensorFault = !snapshot.humidityMeasured;

            in.rainSensorEnabled = cfg.rain.enabled;
            in.rainSensorHealthy =
                snapshot.rg15.online && !snapshot.rg15.stale && snapshot.rg15.status == SensorStatus::Ok && !snapshot.rg15.lensBad;
            // rainLatched holds for rain.rainClearDelayMs after the last drop -
            // the hold-off before a roof should re-open.
            in.raining = snapshot.rg15.isRaining || snapshot.rg15.rainLatched;

            in.windSensorEnabled = cfg.wind.enabled;
            in.windSensorHealthy = windFresh(snapshot.wind, now);
            in.windSpeedMs = snapshot.wind.speedMs;
            in.windGustMs = snapshot.wind.gustMs;
            return in;
        }

        Alpaca::SafetyThresholds safetyThresholds(const Config &cfg)
        {
            Alpaca::SafetyThresholds t;
            t.manualOverrideUnsafe = cfg.alpaca.manualOverrideUnsafe;
            t.staleAfterSeconds = cfg.alpaca.staleAfterSeconds;
            t.cloudCoverEnabled = cfg.alpaca.cloudCoverEnabled;
            t.cloudCoverUnsafePercent = cfg.alpaca.cloudCoverUnsafePercent;
            t.sqmMinEnabled = cfg.alpaca.sqmMinEnabled;
            t.sqmMinSafe = cfg.alpaca.sqmMinSafe;
            t.humidityMaxEnabled = cfg.alpaca.humidityMaxEnabled;
            t.humidityMaxSafe = cfg.alpaca.humidityMaxSafe;
            t.dewpointMarginEnabled = cfg.alpaca.dewpointMarginEnabled;
            t.dewpointMarginMinC = cfg.alpaca.dewpointMarginMinC;
            t.rainUnsafeEnabled = cfg.alpaca.rainUnsafeEnabled;
            t.rainSensorRequired = cfg.alpaca.rainSensorRequired;
            t.windSpeedUnsafeEnabled = cfg.alpaca.windSpeedUnsafeEnabled;
            t.windSpeedUnsafeMs = cfg.alpaca.windSpeedUnsafeMs;
            t.windGustUnsafeEnabled = cfg.alpaca.windGustUnsafeEnabled;
            t.windGustUnsafeMs = cfg.alpaca.windGustUnsafeMs;
            return t;
        }

        Alpaca::ObservingConditionsSnapshot observingConditions(const SensorSnapshot &snapshot, const Config &cfg, uint32_t now)
        {
            const uint32_t staleAfter = cfg.sensor.readIntervalMs + SENSOR_STALE_GRACE_MS;
            auto sourceState = [now, staleAfter](bool present, SensorStatus status, uint32_t lastUpdate)
            {
                Alpaca::SourceState state;
                state.present = present;
                state.ageSeconds = ageMs(now, lastUpdate) / 1000.0;
                state.valid = present && status == SensorStatus::Ok && lastUpdate != 0 && ageMs(now, lastUpdate) <= staleAfter;
                return state;
            };

            // A sensor that isn't there - not detected at boot, or switched on
            // but never answered since - is NotImplemented over Alpaca; one
            // that answered and then failed or went stale is a driver error
            // (spec 007 FR-004).
            Alpaca::ObservingConditionsSnapshot snap;
            snap.skyLight = sourceState(snapshot.tslInitialized, snapshot.tsl.status, snapshot.tslLastUpdate);
            snap.irSky = sourceState(snapshot.mlxInitialized, snapshot.mlx.status, snapshot.mlxLastUpdate);
            snap.environment = sourceState(snapshot.bmeInitialized, snapshot.bme.status, snapshot.bmeLastUpdate);

            // The RG-15 polls on its own interval and tracks its own staleness.
            snap.rain.present = cfg.rain.enabled && snapshot.rg15.timestamp != 0;
            snap.rain.ageSeconds = ageMs(now, snapshot.rg15.timestamp) / 1000.0;
            snap.rain.valid = cfg.rain.enabled && snapshot.rg15.online && !snapshot.rg15.stale &&
                              snapshot.rg15.status == SensorStatus::Ok && snapshot.rg15.timestamp != 0;

            const bool fresh = windFresh(snapshot.wind, now);
            snap.wind.present = cfg.wind.enabled && snapshot.wind.timestamp != 0;
            snap.wind.valid = cfg.wind.enabled && fresh;
            snap.wind.ageSeconds = ageMs(now, snapshot.wind.timestamp) / 1000.0;
            snap.windVane.present = snap.wind.present && cfg.wind.directionEnabled;
            // Calm is valid (direction reported as 0); only a vane fault isn't.
            snap.windVane.valid = snap.windVane.present && fresh && !snapshot.wind.vaneFault;
            snap.windVane.ageSeconds = snap.wind.ageSeconds;
            snap.windSpeedMs = snapshot.wind.speedMs;
            snap.windGustMs = snapshot.wind.gustMs;
            snap.windDirectionDeg = snapshot.wind.directionValid ? snapshot.wind.directionDeg : 0.0f;

            // Cloud cover may use the assumed humidity when the BME280 is down -
            // it only shifts the correction term - but Alpaca's Humidity
            // property must never report that made-up value.
            snap.cloudCoverPercent = snapshot.cloud.cloudCoverPercent;
            snap.skyQualityMagArcsec2 = snapshot.sky.sqm;
            snap.skyBrightnessLux = snapshot.tsl.lux;
            snap.skyTemperatureC = snapshot.mlx.objectTemp;
            snap.temperatureC = snapshot.bme.temperature;
            snap.humidityPercent = snapshot.bme.humidity;
            snap.dewpointC = snapshot.bme.dewpoint;
            snap.pressureHPa = snapshot.bme.pressure;
            snap.rainRateMmPerHour = Alpaca::rainRateToMmPerHour(snapshot.rg15.rInt, snapshot.rg15.imperial);
            return snap;
        }

        bool updateSafety(
            SafetyStatus &status, Alpaca::SafeDelayFilter &filter, const Alpaca::SafetyResult &result, const Config &cfg, uint32_t now)
        {
            const bool reportedSafe = filter.update(result.isSafe, now, cfg.alpaca.safeDelaySeconds);
            const bool changed = reportedSafe != status.isSafe || status.evaluatedAtMs == 0;
            if (changed)
                status.changedAtMs = now;
            status.isSafe = reportedSafe;
            status.rawSafe = result.isSafe;
            status.reasonFlags = result.reasonFlags;
            status.reasons = result.unsafeReasons;
            status.secondsUntilSafe = filter.secondsUntilSafe();
            status.evaluatedAtMs = now;
            return changed;
        }

        void writeSafety(JsonObject target, const SafetyStatus &status, const Config &cfg, uint32_t now)
        {
            target["safe"] = status.isSafe;
            target["rawSafe"] = status.rawSafe;
            target["alpacaEnabled"] = cfg.alpaca.enabled;
            target["reasonFlags"] = status.reasonFlags;
            JsonArray reasons = target.createNestedArray("reasons");
            for (const std::string &reason : status.reasons)
                reasons.add(reason);
            target["secondsUntilSafe"] = status.secondsUntilSafe;
            // Rules switched on but ignored because their sensor is off
            // (specs/020-settings-dependencies FR-009).
            JsonArray notInEffect = target.createNestedArray("rulesNotInEffect");
            for (const std::string &rule : Deps::rulesNotInEffect(cfg))
                notInEffect.add(rule);
            target["evaluatedAgeMs"] = ageMs(now, status.evaluatedAtMs);
            target["changedAgeMs"] = ageMs(now, status.changedAtMs);
        }

        NightState night(const SensorSnapshot &snapshot, const Config &cfg, int64_t epoch)
        {
            NightState n;
            if (snapshot.gps.hasFix)
            {
                n.source = "gps";
                n.latitude = snapshot.gps.latitude;
                n.longitude = snapshot.gps.longitude;
            }
            else if (cfg.location.set)
            {
                n.source = "manual";
                n.latitude = cfg.location.latitude;
                n.longitude = cfg.location.longitude;
            }
            if (epoch < CLOCK_VALID_EPOCH || n.source == nullptr)
                return n; // no clock or no location: unknown
            n.known = true;
            n.sunAltitudeDeg = Astro::sunElevationDeg(epoch, n.latitude, n.longitude);
            n.isNight = n.sunAltitudeDeg < cfg.alerts.nightSunAltitudeDeg;
            return n;
        }

        void writeSky(JsonObject sky, const NightState &n)
        {
            sky["locationSource"] = n.source != nullptr ? n.source : "none";
            sky["nightKnown"] = n.known;
            if (n.source != nullptr)
            {
                sky["latitude"] = rounded(n.latitude, 4);
                sky["longitude"] = rounded(n.longitude, 4);
            }
            if (n.known)
            {
                sky["isNight"] = n.isNight;
                sky["sunAltitudeDeg"] = rounded(n.sunAltitudeDeg, 1);
            }
        }

        std::string isoUtc(int64_t epoch)
        {
            if (epoch < CLOCK_VALID_EPOCH)
                return "";
            const time_t t = static_cast<time_t>(epoch);
            struct tm utc;
            gmtime_r(&t, &utc);
            char buffer[32];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
            return buffer;
        }
    } // namespace Core
} // namespace SQM
