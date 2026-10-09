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
                if (!initialized || status == SensorStatus::NOT_INITIALIZED)
                    return Readings::Status::Missing;
                if (status != SensorStatus::OK)
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
                case RG15State::RG15_DISABLED:
                    return "disabled";
                case RG15State::RG15_CONFIGURED:
                    return "configured";
                case RG15State::RG15_UART_OPENED:
                    return "uart_opened";
                case RG15State::RG15_CONFIGURING:
                    return "configuring";
                case RG15State::RG15_COMMAND_SENT:
                    return "command_sent";
                case RG15State::RG15_AWAITING_RESPONSE:
                    return "awaiting_response";
                case RG15State::RG15_ACKNOWLEDGED:
                    return "acknowledged";
                case RG15State::RG15_READING_RECEIVED:
                    return "reading_received";
                case RG15State::RG15_PARSE_ERROR:
                    return "parse_error";
                case RG15State::RG15_TIMEOUT:
                    return "timeout";
                case RG15State::RG15_STALE:
                    return "stale";
                case RG15State::RG15_ONLINE:
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
                return wind.status == SensorStatus::OK && wind.timestamp != 0 && ageMs(now, wind.timestamp) <= WIND_STALE_MS;
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
            snapshot.humidityMeasured = snapshot.bmeInitialized && snapshot.bme.status == SensorStatus::OK;
            snapshot.cloudHumidity = snapshot.humidityMeasured ? snapshot.bme.humidity : ASSUMED_HUMIDITY_PERCENT;
            snapshot.cloud = CloudDetection::calculate(
                snapshot.mlx.objectTemp,
                snapshot.mlx.ambientTemp,
                snapshot.cloudHumidity,
                cfg.cloudDetection.clearSkyThreshold,
                cfg.cloudDetection.cloudyThreshold,
                cfg.cloudDetection.humidityCorrection);
        }

        Readings::Snapshot buildReadings(const SensorSnapshot &snapshot, const Config &cfg, uint32_t now, int64_t epoch)
        {
            const uint32_t staleAfter = cfg.sensor.readIntervalMs + SENSOR_STALE_GRACE_MS;
            Readings::Snapshot r;

            r.timeValid = epoch >= CLOCK_VALID_EPOCH;
            r.timestamp = r.timeValid ? epoch : 0;
            r.dataAgeMs = ageMs(now, snapshot.dataTimestamp);
            r.dataStale = snapshot.dataTimestamp == 0 || r.dataAgeMs > staleAfter;

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
            r.sky.bortle = static_cast<int>(sky.bortle + 0.5f);
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

            r.gps.present = cfg.gps.enabled;
            if (r.gps.present)
            {
                const GPSReading &gps = snapshot.gps;
                r.gps.status = snapshot.gpsInitialized
                                   ? (gps.status == SensorStatus::OK || gps.status == SensorStatus::TIMEOUT ? Readings::Status::Ok
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

            r.wind.present = cfg.wind.enabled;
            if (r.wind.present)
            {
                const WindReading &wind = snapshot.wind;
                r.wind.status = wind.status == SensorStatus::OK                ? Readings::Status::Ok
                                : wind.status == SensorStatus::NOT_INITIALIZED ? Readings::Status::Missing
                                                                               : Readings::Status::Error;
                r.wind.ageMs = ageMs(now, wind.timestamp);
                r.wind.speed = wind.speedMs;
                r.wind.gust = wind.gustMs;
                r.wind.directionValid = cfg.wind.directionEnabled && wind.directionValid;
                r.wind.direction = wind.directionDeg;
                r.wind.vaneFault = wind.vaneFault;
            }
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
                    if (source.status == SensorStatus::OK)
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
            in.skyLightFault = snapshot.tsl.status != SensorStatus::OK;
            in.irSkyFault = snapshot.mlx.status != SensorStatus::OK;
            in.requiredSensorFault = snapshot.tsl.status != SensorStatus::OK || snapshot.mlx.status != SensorStatus::OK;

            in.sqm = snapshot.sky.sqm;
            in.cloudCoverPercent = snapshot.cloud.cloudCoverPercent;
            in.humidityPercent = snapshot.cloudHumidity;
            in.temperatureC = snapshot.bme.temperature;
            in.dewpointC = snapshot.bme.dewpoint;
            in.environmentSensorFault = !snapshot.humidityMeasured;

            in.rainSensorEnabled = cfg.rain.enabled;
            in.rainSensorHealthy =
                snapshot.rg15.online && !snapshot.rg15.stale && snapshot.rg15.status == SensorStatus::OK && !snapshot.rg15.lensBad;
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
                state.valid = present && status == SensorStatus::OK && lastUpdate != 0 && ageMs(now, lastUpdate) <= staleAfter;
                return state;
            };

            Alpaca::ObservingConditionsSnapshot snap;
            snap.skyLight = sourceState(true, snapshot.tsl.status, snapshot.tslLastUpdate);
            snap.irSky = sourceState(true, snapshot.mlx.status, snapshot.mlxLastUpdate);
            snap.environment = sourceState(true, snapshot.bme.status, snapshot.bmeLastUpdate);

            // The RG-15 polls on its own interval and tracks its own staleness.
            snap.rain.present = cfg.rain.enabled;
            snap.rain.ageSeconds = ageMs(now, snapshot.rg15.timestamp) / 1000.0;
            snap.rain.valid = cfg.rain.enabled && snapshot.rg15.online && !snapshot.rg15.stale &&
                              snapshot.rg15.status == SensorStatus::OK && snapshot.rg15.timestamp != 0;

            const bool fresh = windFresh(snapshot.wind, now);
            snap.wind.present = cfg.wind.enabled;
            snap.wind.valid = cfg.wind.enabled && fresh;
            snap.wind.ageSeconds = ageMs(now, snapshot.wind.timestamp) / 1000.0;
            snap.windVane.present = cfg.wind.enabled && cfg.wind.directionEnabled;
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

        Alerts::AlertInputs alertInputs(
            const SafetyStatus &status,
            const SensorSnapshot &snapshot,
            const Alpaca::ObservingConditionsSnapshot &obs,
            const Config &cfg,
            const NightState &n,
            uint32_t now)
        {
            Alerts::AlertInputs in;
            in.nowSeconds = now / 1000;
            in.safetyKnown = status.evaluatedAtMs != 0;
            in.safetySettling = (!status.isSafe && status.rawSafe) || (status.reasonFlags & Alpaca::UNSAFE_NO_DATA) != 0;
            in.isSafe = status.isSafe;
            in.unsafeReasons = status.reasons;

            in.rainEnabled = cfg.rain.enabled;
            in.raining = snapshot.rg15.isRaining || snapshot.rg15.rainLatched;
            in.rainRateMmPerHour = obs.rainRateMmPerHour;
            in.lensFault = snapshot.rg15.lensBad;

            in.sensors[0] = {"TSL2591 light", true, obs.skyLight.valid};
            in.sensors[1] = {"MLX90614 IR", true, obs.irSky.valid};
            in.sensors[2] = {"BME280 environment", true, obs.environment.valid};
            in.sensors[3] = {"RG-15 rain", cfg.rain.enabled, obs.rain.valid};
            in.sensors[4] = {"Wind", obs.wind.present, obs.wind.valid};

            in.environmentValid = obs.environment.valid;
            in.temperatureC = obs.temperatureC;
            in.dewpointC = obs.dewpointC;
            in.skyValid = obs.irSky.valid;
            in.cloudCoverPercent = obs.cloudCoverPercent;
            in.nightKnown = n.known;
            in.isNight = n.isNight;
            return in;
        }

        Alerts::AlertRules alertRules(const Config &cfg)
        {
            Alerts::AlertRules rules;
            const AlertsConfig &a = cfg.alerts;
            rules.onSafetyChange = a.unsafe.level || a.safe.level;
            rules.onRain = a.rainStarted.level || a.rainStopped.level;
            rules.onSensorFault = a.sensorFault.level || a.sensorRecovered.level;
            rules.onDewRisk = a.dewRisk.level != 0;
            rules.dewRiskMarginC = a.dewRiskMarginC;
            rules.onClearSky = a.clearSky.level != 0;
            rules.clearSkyCloudPercent = a.clearSkyCloudPercent;
            rules.onCloudedOver = a.cloudedOver.level != 0;
            rules.cloudedOverCloudPercent = a.cloudedOverCloudPercent;
            rules.skyNightOnly = a.skyNightOnly;
            rules.safetyNightOnly = a.safetyNightOnly;
            rules.cooldownSeconds = a.cooldownSeconds;
            rules.onClientLost = a.clientLost.level != 0;
            rules.onClientBack = a.clientBack.level != 0;
            rules.onClientDisconnected = a.clientDisconnected.level != 0;
            return rules;
        }

        const AlertsConfig::EventSetting *eventSettingFor(const AlertsConfig &a, Alerts::AlertType type)
        {
            switch (type)
            {
            case Alerts::AlertType::Unsafe:
                return &a.unsafe;
            case Alerts::AlertType::Safe:
                return &a.safe;
            case Alerts::AlertType::RainStarted:
                return &a.rainStarted;
            case Alerts::AlertType::RainStopped:
                return &a.rainStopped;
            case Alerts::AlertType::SensorFault:
            case Alerts::AlertType::LensFault:
                return &a.sensorFault;
            case Alerts::AlertType::SensorRecovered:
                return &a.sensorRecovered;
            case Alerts::AlertType::DewRisk:
                return &a.dewRisk;
            case Alerts::AlertType::ClearSky:
                return &a.clearSky;
            case Alerts::AlertType::CloudedOver:
                return &a.cloudedOver;
            case Alerts::AlertType::ClientLost:
                return &a.clientLost;
            case Alerts::AlertType::ClientBack:
                return &a.clientBack;
            case Alerts::AlertType::ClientDisconnected:
                return &a.clientDisconnected;
            default:
                return nullptr;
            }
        }

        std::vector<std::pair<std::string, std::string>> alertVars(
            const Config &cfg,
            const Alpaca::ObservingConditionsSnapshot &obs,
            const NightState &n,
            const Alerts::Alert &alert,
            const std::string &localTime,
            const std::string &localDate)
        {
            auto num = [](bool valid, double value, int decimals) -> std::string
            {
                if (!valid)
                    return "--";
                char buffer[24];
                std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
                return buffer;
            };
            const AlertsConfig &a = cfg.alerts;
            const AlpacaConfig &limits = cfg.alpaca;

            // Readings and settings first; the event's own values (reasons,
            // sensor, ...) come after and win on a name clash.
            std::vector<std::pair<std::string, std::string>> vars = {
                {"device", cfg.deviceName},
                {"event", Alerts::alertTypeName(alert.type)},
                {"level", Alerts::alertLevelName(alert.level)},
                {"time", localTime},
                {"date", localDate},
                {"sqm", num(obs.skyLight.valid, obs.skyQualityMagArcsec2, 2)},
                {"sqm_min", num(true, limits.sqmMinSafe, 2)},
                {"cloud", num(obs.irSky.valid, obs.cloudCoverPercent, 0)},
                {"cloud_max", num(true, limits.cloudCoverUnsafePercent, 0)},
                {"clear_below", num(true, a.clearSkyCloudPercent, 0)},
                {"cloudy_above", num(true, a.cloudedOverCloudPercent, 0)},
                {"sky_temp", num(obs.irSky.valid, obs.skyTemperatureC, 1)},
                {"temp", num(obs.environment.valid, obs.temperatureC, 1)},
                {"humidity", num(obs.environment.valid, obs.humidityPercent, 0)},
                {"humidity_max", num(true, limits.humidityMaxSafe, 0)},
                {"dewpoint", num(obs.environment.valid, obs.dewpointC, 1)},
                {"dew_margin", num(obs.environment.valid, obs.temperatureC - obs.dewpointC, 1)},
                {"pressure", num(obs.environment.valid, obs.pressureHPa, 0)},
                {"rain_rate", num(obs.rain.valid, obs.rainRateMmPerHour, 1)},
                {"wind", num(obs.wind.valid, obs.windSpeedMs, 1)},
                {"gust", num(obs.wind.valid, obs.windGustMs, 1)},
                {"sun_alt", num(n.known, n.sunAltitudeDeg, 1)},
            };
            vars.insert(vars.end(), alert.vars.begin(), alert.vars.end());
            return vars;
        }

        void applyAlertTemplate(
            Alerts::Alert &alert, const AlertsConfig::EventSetting &setting, const std::vector<std::pair<std::string, std::string>> &vars)
        {
            if (!setting.title.empty())
                alert.title = Alerts::renderTemplate(setting.title, vars);
            if (!setting.message.empty())
                alert.message = Alerts::renderTemplate(setting.message, vars);
            // Not needed past this point, and the recent-alerts list keeps alerts.
            alert.vars.clear();
            alert.vars.shrink_to_fit();
        }

        void clientSilenceMs(const Config &cfg, uint32_t (&out)[Alpaca::DEVICE_COUNT])
        {
            out[static_cast<size_t>(Alpaca::Device::SafetyMonitor)] = cfg.alerts.clientSilentSafetySeconds * 1000;
            out[static_cast<size_t>(Alpaca::Device::ObservingConditions)] = cfg.alerts.clientSilentWeatherSeconds * 1000;
        }

        std::string formatDuration(uint32_t seconds)
        {
            if (seconds < 60)
                return std::to_string(seconds) + " s";
            const uint32_t minutes = (seconds + 30) / 60;
            if (minutes < 60)
                return std::to_string(minutes) + " min";
            return std::to_string(minutes / 60) + " h" + (minutes % 60 != 0 ? " " + std::to_string(minutes % 60) + " min" : "");
        }

        namespace
        {
            const char *const CLIENT_DEVICE_NAMES[Alpaca::DEVICE_COUNT] = {"safety monitor", "weather device"};
            const char *const CLIENT_DEVICE_KEYS[Alpaca::DEVICE_COUNT] = {"safetymonitor", "observingconditions"};

            // The local clock time `ageSeconds` ago, from the current "HH:MM";
            // "N min ago" when there's no clock.
            std::string lastCheckedText(uint32_t ageSeconds, const std::string &localTime)
            {
                const bool clock = localTime.size() == 5 && localTime[2] == ':' && std::isdigit(static_cast<unsigned char>(localTime[0])) &&
                                   std::isdigit(static_cast<unsigned char>(localTime[1])) &&
                                   std::isdigit(static_cast<unsigned char>(localTime[3])) &&
                                   std::isdigit(static_cast<unsigned char>(localTime[4]));
                if (!clock)
                    return formatDuration(ageSeconds) + " ago";
                const int now = std::stoi(localTime.substr(0, 2)) * 60 + std::stoi(localTime.substr(3, 2));
                const int then = ((now - static_cast<int>(ageSeconds / 60)) % 1440 + 1440) % 1440;
                char buffer[8];
                std::snprintf(buffer, sizeof(buffer), "%02d:%02d", then / 60, then % 60);
                return buffer;
            }
        } // namespace

        void addClientInputs(
            Alerts::AlertInputs &inputs, const Alpaca::ClientWatch &watch, const Config &cfg, uint32_t nowMs, const std::string &localTime)
        {
            uint32_t silence[Alpaca::DEVICE_COUNT];
            clientSilenceMs(cfg, silence);
            for (size_t i = 0; i < Alpaca::DEVICE_COUNT; ++i)
            {
                const Alpaca::ClientState &state = watch.state(static_cast<Alpaca::Device>(i));
                Alerts::ClientInputs &client = inputs.clients[i];
                client.device = CLIENT_DEVICE_NAMES[i];
                client.watching = state.watching;
                client.silent = state.silent;
                client.disconnectedNow = state.disconnectedNow;
                client.silentFor = formatDuration(silence[i] / 1000);
                client.lastChecked = state.everRequested ? lastCheckedText((nowMs - state.lastRequestMs) / 1000, localTime) : "never";
                client.clientId = state.hasClientId ? std::to_string(state.clientId) : "";
            }
        }

        void writeClientWatch(JsonObject alpaca, const Alpaca::ClientWatch &watch, const Config &cfg, uint32_t nowMs)
        {
            alpaca["enabled"] = cfg.alpaca.enabled;
            JsonObject clients = alpaca.createNestedObject("clients");
            for (size_t i = 0; i < Alpaca::DEVICE_COUNT; ++i)
            {
                const Alpaca::ClientState &state = watch.state(static_cast<Alpaca::Device>(i));
                JsonObject client = clients.createNestedObject(CLIENT_DEVICE_KEYS[i]);
                client["connected"] = state.connected;
                client["watching"] = state.watching;
                client["silent"] = state.silent;
                if (state.everRequested)
                    client["lastCheckedAgeMs"] = nowMs - state.lastRequestMs;
                else
                    client["lastCheckedAgeMs"] = nullptr;
                if (state.hasClientId)
                    client["clientId"] = state.clientId;
                else
                    client["clientId"] = nullptr;
            }
        }

        Alerts::SendMode sendMode(const Config &cfg)
        {
            return cfg.alerts.sendMode == AlertsConfig::SendMode::WhileConnected ? Alerts::SendMode::WhileConnected : Alerts::SendMode::Any;
        }

        Alerts::SendMode effectiveSendMode(const Config &cfg)
        {
            // dep: D-12 - imaging apps connect over Alpaca: with it off, "only while
            // an imaging app is connected" isn't in effect and alerts go out any time.
            return cfg.alpaca.enabled ? sendMode(cfg) : Alerts::SendMode::Any;
        }

        void writeAlertSchedule(JsonObject target, const Alerts::ScheduleState &state, const Config &cfg, uint32_t nowMs)
        {
            target["armed"] = state.sending;
            target["armWithAlpaca"] = cfg.alerts.sendMode == AlertsConfig::SendMode::WhileConnected;
            target["mode"] = Alerts::sendModeName(sendMode(cfg));
            target["reason"] = Alerts::scheduleReasonName(state.reason);
            const std::string since = isoUtc(state.sinceEpoch);
            if (since.empty())
                target["since"] = nullptr;
            else
                target["since"] = since;
            if (state.sinceKnown)
                target["sinceAgeMs"] = nowMs - state.sinceMs;
            else
                target["sinceAgeMs"] = nullptr;
        }

        AlertStep runAlerts(
            Alerts::AlertEngine &engine,
            const Alerts::AlertInputs &inputs,
            const Alerts::AlertRules &rules,
            const Config &cfg,
            const Alpaca::ObservingConditionsSnapshot &obs,
            const NightState &n,
            const SafetyStatus &status,
            const std::string &localTime,
            const std::string &localDate)
        {
            AlertStep step;
            for (Alerts::Alert alert : engine.update(inputs, rules))
            {
                const AlertsConfig::EventSetting *setting = eventSettingFor(cfg.alerts, alert.type);
                if (setting == nullptr || setting->level == 0)
                    continue;
                alert.level = static_cast<Alerts::AlertLevel>(setting->level);
                alert.sound = setting->sound;
                applyAlertTemplate(alert, *setting, alertVars(cfg, obs, n, alert, localTime, localDate));
                if (alert.level == Alerts::AlertLevel::Wake)
                    step.alarmFlags |= status.reasonFlags | (alert.type == Alerts::AlertType::RainStarted ? Alpaca::UNSAFE_RAIN : 0u) |
                                       (alert.type == Alerts::AlertType::SensorFault || alert.type == Alerts::AlertType::LensFault
                                            ? Alpaca::UNSAFE_SENSOR_FAULT
                                            : 0u);
                step.outgoing.push_back(std::move(alert));
            }
            return step;
        }

        namespace
        {
            constexpr SampleAlert SAMPLE_ALERTS[] = {
                {"unsafe", Alerts::AlertType::Unsafe, "Observatory UNSAFE", "It turns unsafe", Alpaca::UNSAFE_CLOUD_COVER},
                {"safe", Alerts::AlertType::Safe, "Observatory safe", "It's safe again", 0},
                {"rain_started", Alerts::AlertType::RainStarted, "Rain detected", "Rain starts", Alpaca::UNSAFE_RAIN},
                {"rain_stopped", Alerts::AlertType::RainStopped, "Rain cleared", "Rain stops", 0},
                {"sensor_fault", Alerts::AlertType::SensorFault, "Sensor fault", "A sensor fails", Alpaca::UNSAFE_SENSOR_FAULT},
                {"sensor_recovered", Alerts::AlertType::SensorRecovered, "Sensor recovered", "A sensor recovers", 0},
                {"dew_risk", Alerts::AlertType::DewRisk, "Dew risk", "Dew risk", Alpaca::UNSAFE_DEWPOINT},
                {"clear_sky", Alerts::AlertType::ClearSky, "Dark and clear", "Skies clear up", 0},
                {"clouded_over", Alerts::AlertType::CloudedOver, "Clouded over", "Skies cloud over", Alpaca::UNSAFE_CLOUD_COVER},
                {"client_lost", Alerts::AlertType::ClientLost, "Imaging app stopped checking", "The imaging app stops checking", 0},
                {"client_back", Alerts::AlertType::ClientBack, "Imaging app is back", "The imaging app is back", 0},
                {"client_disconnected",
                 Alerts::AlertType::ClientDisconnected,
                 "Imaging app disconnected",
                 "The imaging app disconnects",
                 0},
            };
        } // namespace

        const SampleAlert *sampleAlert(const std::string &key)
        {
            for (const SampleAlert &sample : SAMPLE_ALERTS)
                if (key == sample.key)
                    return &sample;
            return nullptr;
        }

        Alerts::Alert buildTestAlert(
            const SampleAlert *sample,
            uint8_t level,
            const std::string &sound,
            const std::string &title,
            const std::string &message,
            const SafetyStatus &safety,
            const Config &cfg,
            const Alpaca::ObservingConditionsSnapshot &obs,
            const NightState &n,
            const std::string &localTime,
            const std::string &localDate)
        {
            Alerts::Alert test;
            if (sample == nullptr)
            {
                test.type = Alerts::AlertType::Test;
                test.level = Alerts::AlertLevel::Normal;
                test.title = "Test notification";
                test.message = "Alerts from this SQMeter are working.";
                return test;
            }
            test.type = sample->type;
            test.level = static_cast<Alerts::AlertLevel>(level);
            test.sound = sound;
            test.title = sample->title;
            test.message = std::string("This is how a \"") + sample->label + "\" alert arrives.";

            // Custom wording is filled in from live readings; values only a
            // real event has (the reasons, which sensor) are examples unless
            // they apply right now.
            if (sample->type == Alerts::AlertType::Unsafe)
            {
                const std::vector<std::string> reasons =
                    safety.isSafe ? std::vector<std::string>{"Cloud 62% >= 35% (example)"} : safety.reasons;
                std::string inline_;
                for (const std::string &reason : reasons)
                    inline_ += (inline_.empty() ? "" : "; ") + reason;
                test.vars = {
                    {"reasons", Alerts::joinReasons(reasons)},
                    {"reasons_inline", inline_},
                    {"reason_count", std::to_string(reasons.size())}};
            }
            else if (sample->type == Alerts::AlertType::SensorFault || sample->type == Alerts::AlertType::SensorRecovered)
                test.vars = {{"sensor", "TSL2591 light (example)"}};
            else if (
                sample->type == Alerts::AlertType::ClientLost || sample->type == Alerts::AlertType::ClientBack ||
                sample->type == Alerts::AlertType::ClientDisconnected)
                test.vars = {
                    {"device", "safety monitor"},
                    {"silent_for", formatDuration(cfg.alerts.clientSilentSafetySeconds)},
                    {"last_checked", "2 min ago (example)"},
                    {"client_id", "1234 (example)"}};
            AlertsConfig::EventSetting custom{level, sound, title, message};
            applyAlertTemplate(test, custom, alertVars(cfg, obs, n, test, localTime, localDate));
            test.title = "Test: " + test.title;
            return test;
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
