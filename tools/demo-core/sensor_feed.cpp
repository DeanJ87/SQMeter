#include "sensor_feed.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include "DeviceCore.h"
#include "calculations/Dewpoint.h"
#include "calculations/SkyQuality.h"

namespace SQM
{
    void SensorFeed::restart()
    {
        latch = Rain::Latch{};
        lightSamples.clear();
        lastLightStep = 0;
        lastInputLux = 0.0f;
        policy = Battery::PowerPolicy{};
    }

    float SensorFeed::averagedLux(float inputLux, bool nightMode, uint32_t now)
    {
        const uint16_t window = static_cast<uint16_t>(std::max<uint32_t>(10, std::min<uint32_t>(cfg.skyAveraging.windowSeconds, 300)));
        if (window != lightWindow)
        {
            lightWindow = window;
            lightSamples.clear(); // the driver resets its samples too
        }
        if (lastInputLux > 0.0f && std::fabs(inputLux - lastInputLux) > 0.1f * lastInputLux)
            lastLightStep = now;
        lastInputLux = inputLux;
        lightSamples.emplace_back(now, inputLux);
        while (!lightSamples.empty() && now - lightSamples.front().first >= window * 1000UL)
            lightSamples.pop_front();
        if (!nightMode || lightSamples.empty())
            return inputLux;
        float total = 0.0f;
        for (const auto &sample : lightSamples)
            total += sample.second;
        return total / static_cast<float>(lightSamples.size());
    }

    // Simulated sensors -> the readings the drivers would produce.
    void SensorFeed::read(JsonObjectConst in)
    {
        snapshot.dataTimestamp = nowMs;
        snapshot.capturedAt = nowMs;
        readLight(in);
        lightDiagnostics();
        readEnvironment(in);
        readGps(in);
        readRain(in);
        rainDiagnostics();
        readWind(in);
        readBattery(in);
    }

    void SensorFeed::readLight(JsonObjectConst in)
    {
        // TSL2591s: detected at boot unless the demo says one is missing. The
        // SQM-L (spec 030) sees the same sky through a narrow lens: about
        // 0.25 mag/arcsec² darker, so 10^(-0.1) of the SQM's light.
        JsonObjectConst light = in["light"];
        const float lux = averagedLux(light["lux"] | 0.001f, light["nightMode"] | false, nowMs);
        readSky(SkyRole::Sqm, light, light, lux);
        readSky(SkyRole::Sqml, in["lightL"], light, lux * std::pow(10.0f, -SQML_DARKER_MAG / 2.5f));
    }

    // `sensor`: present / failed flags; `light`: the sky (counts, night mode).
    void SensorFeed::readSky(SkyRole role, JsonObjectConst sensor, JsonObjectConst light, float averagedRawLux)
    {
        const uint32_t now = nowMs;
        SkySensorSnapshot &sky = snapshot.skySensors[skyRoleIndex(role)];
        // A sensor that is off is never begun.
        sky.initialized = cfg.skyEnabled(role) && (sensor["present"] | true);
        TSL2591Reading &tsl = sky.reading;
        if (sky.initialized && !(sensor["failed"] | false))
        {
            const SkyCalibrationConfig &cal = cfg.calibration(role);
            tsl.status = SensorStatus::Ok;
            tsl.timestamp = now;
            sky.lastUpdate = now;
            tsl.rawLux = averagedRawLux;
            const CalibratedLight calibrated = SkyQuality::calibrate(tsl.rawLux, cal.enabled, cal.sqmOffset);
            tsl.rawSqm = calibrated.rawSqm;
            tsl.calibrated = cal.enabled;
            tsl.calibratedSqm = calibrated.calibratedSqm;
            tsl.lux = calibrated.lux;
            tsl.visible = light["visible"] | 0;
            tsl.infrared = light["infrared"] | 0;
            tsl.full = light["full"] | 0;
            tsl.nightMode = light["nightMode"] | false;
        }
        else if (sky.initialized)
        {
            tsl.status = SensorStatus::Timeout;
        }
        else
        {
            tsl = TSL2591Reading{};
        }
    }

    void SensorFeed::lightDiagnostics()
    {
        const uint32_t now = nowMs;
        for (size_t i = 0; i < SKY_ROLE_COUNT; ++i)
        {
            const SkyRole role = static_cast<SkyRole>(i);
            const TSL2591Reading &tsl = snapshot.skySensors[i].reading;
            TSL2591Diagnostics &d = snapshot.skySensors[i].diagnostics;
            d.gainName = tsl.nightMode ? "MAX" : "HIGH";
            d.gainFactor = tsl.nightMode ? 9876.0f : 428.0f;
            d.integrationMs = 600;
            d.averagingWindowSeconds = static_cast<uint16_t>(cfg.averaging(role).windowSeconds);
            const uint32_t sinceBoot = now - bootMs;
            d.sampleCount = static_cast<uint16_t>(std::min<uint32_t>(sinceBoot / 600, Core::windowSamples(d)));
            d.rollingVisible = tsl.visible;
            d.darkVisibleOffset = cfg.calibration(role).darkVisibleOffset;
            d.correctedVisible = std::max(0.0f, d.rollingVisible - d.darkVisibleOffset);
            d.rawSqm = tsl.rawSqm;
            d.calibratedSqm = tsl.calibratedSqm;
            d.nightMode = tsl.nightMode;
            d.calibrated = tsl.calibrated;
            d.saturated = false;
        }
    }

    void SensorFeed::readEnvironment(JsonObjectConst in)
    {
        const uint32_t now = nowMs;
        // BME280
        JsonObjectConst env = in["environment"];
        snapshot.bmeInitialized = env["present"] | true;
        if (snapshot.bmeInitialized && !(env["failed"] | false))
        {
            BME280Reading &bme = snapshot.bme;
            bme.status = SensorStatus::Ok;
            bme.timestamp = now;
            snapshot.bmeLastUpdate = now;
            bme.temperature = env["temperature"] | 10.0f;
            bme.humidity = env["humidity"] | 60.0f;
            bme.pressure = env["pressure"] | 1013.0f;
            bme.dewpoint = dewpointMagnus(bme.temperature, bme.humidity);
        }
        else if (snapshot.bmeInitialized)
        {
            snapshot.bme.status = SensorStatus::Timeout;
        }

        // MLX90614
        JsonObjectConst ir = in["infrared"];
        snapshot.mlxInitialized = ir["present"] | true;
        if (snapshot.mlxInitialized && !(ir["failed"] | false))
        {
            MLX90614Reading &mlx = snapshot.mlx;
            mlx.status = SensorStatus::Ok;
            mlx.timestamp = now;
            snapshot.mlxLastUpdate = now;
            mlx.objectTemp = ir["sky"] | -20.0f;
            mlx.ambientTemp = ir["ambient"] | 10.0f;
        }
        else if (snapshot.mlxInitialized)
        {
            snapshot.mlx.status = SensorStatus::Timeout;
        }
    }

    void SensorFeed::readGps(JsonObjectConst in)
    {
        const uint32_t now = nowMs;
        // GPS: the driver starts at boot, so it follows the settings at the last restart.
        snapshot.gpsInitialized = bootConfig.gps.enabled;
        GPSReading &gps = snapshot.gps;
        JsonObjectConst g = in["gps"];
        if (snapshot.gpsInitialized && (g["failed"] | false))
        {
            // No NMEA arriving: the driver reports a read error and the
            // reading stops updating (src/sensors/GPSSensor.cpp).
            gps.status = SensorStatus::ReadError;
            gps.hasFix = false;
        }
        else if (snapshot.gpsInitialized)
        {
            gps.status = SensorStatus::Ok;
            gps.timestamp = now;
            snapshot.gpsLastUpdate = now;
            gps.hasFix = g["fix"] | true;
            gps.latitude = g["latitude"] | 0.0;
            gps.longitude = g["longitude"] | 0.0;
            gps.altitude = g["altitude"] | 0.0;
            gps.satellites = g["satellites"] | 0u;
            gps.hdop = 110;
            gps.age = 800;
        }
        else
        {
            gps = GPSReading{};
        }
    }

    void SensorFeed::readRain(JsonObjectConst in)
    {
        const uint32_t now = nowMs;
        // RG-15: started and stopped with the setting.
        snapshot.rg15Initialized = cfg.rain.enabled;
        RG15Reading &rain = snapshot.rg15;
        JsonObjectConst r = in["rain"];
        if (cfg.rain.enabled && !(r["failed"] | false))
        {
            const float rateMm = r["rate"] | 0.0f;
            const bool imperial = cfg.rain.units == "imperial";
            const float scale = imperial ? 1.0f / 25.4f : 1.0f;
            const float dtHours = lastRainTickMs == 0 ? 0.0f : (now - lastRainTickMs) / 3600000.0f;
            lastRainTickMs = now;
            rain.status = SensorStatus::Ok;
            rain.online = true;
            rain.stale = false;
            rain.timestamp = now;
            snapshot.rg15LastUpdate = now;
            rain.imperial = imperial;
            rain.rInt = rateMm * scale;
            rain.acc = rateMm * dtHours * scale;
            rain.eventAcc = rateMm > 0 ? rain.eventAcc + rain.acc : rain.eventAcc;
            rain.totalAcc += rain.acc;
            rain.isRaining = rateMm > 0;
            rain.lensBad = r["lensFault"] | false;
            rain.emSat = false;
            Rain::observe(latch, rain.rInt, rain.acc, now, cfg.rain.rainClearDelayMs);
            rain.rainLatched = latch.latched;
            rain.lastRainMs = latch.lastRainMs;
            rain.localEventAcc = latch.eventAccumulation;
        }
        else
        {
            rain.online = false;
            rain.stale = cfg.rain.enabled;
            rain.status = cfg.rain.enabled ? SensorStatus::Timeout : SensorStatus::NotInitialized;
            lastRainTickMs = 0;
        }
    }

    void SensorFeed::rainDiagnostics()
    {
        const uint32_t now = nowMs;
        const RG15Reading &rain = snapshot.rg15;
        RG15Diagnostics &rd = snapshot.rg15Diagnostics;
        rd.state = cfg.rain.enabled ? (rain.online ? RG15State::Online : RG15State::Timeout) : RG15State::Disabled;
        rd.uartOpened = cfg.rain.enabled;
        rd.rxPin = cfg.rain.rxPin;
        rd.txPin = cfg.rain.txPin;
        rd.baudRate = cfg.rain.baudRate;
        rd.uartPort = 1;
        rd.lastCommand = std::string("R");
        if (rain.online)
        {
            rd.lastPollMs = rd.lastResponseMs = rd.lastSuccessfulReadMs = now;
            rd.successfulReads++;
            char line[96];
            std::snprintf(
                line,
                sizeof(line),
                "Acc %.2f mm, EventAcc %.2f mm, TotalAcc %.2f mm, RInt %.2f mmph",
                rain.acc / (rain.imperial ? 1 / 25.4f : 1.0f),
                rain.eventAcc,
                rain.totalAcc,
                rain.rInt);
            rd.lastRawResponse = std::string(line);
        }
        rd.lastRainDetectedMs = latch.lastRainMs;
    }

    void SensorFeed::readWind(JsonObjectConst in)
    {
        const uint32_t now = nowMs;
        // Anemometer + vane
        WindReading &wind = snapshot.wind;
        JsonObjectConst w = in["wind"];
        if (cfg.wind.enabled && !(w["failed"] | false))
        {
            wind.status = SensorStatus::Ok;
            wind.timestamp = now;
            wind.speedMs = w["speed"] | 0.0f;
            wind.gustMs = w["gust"] | wind.speedMs;
            wind.instantMs = wind.speedMs;
            wind.directionValid = cfg.wind.directionEnabled && wind.speedMs > 0.2f;
            wind.directionDeg = w["direction"] | 0.0f;
            wind.vaneFault = false;
            wind.samples = std::min<uint32_t>(600, (now - bootMs) / 1000);
        }
        else
        {
            wind = WindReading{};
            if (cfg.wind.enabled)
                wind.status = SensorStatus::Timeout;
        }
    }

    namespace
    {
        // A Li-ion cell's resting voltage against its charge, roughly (3.3 V empty, 4.2 V full).
        float cellVoltage(float percent)
        {
            const float p = std::max(0.0f, std::min(100.0f, percent)) / 100.0f;
            return 3.3f + 0.6f * p + 0.3f * p * p * p;
        }
    } // namespace

    // The travel board's fuel gauge and charger (spec 032, research D13): a
    // 7 Ah pack whose charge the demo moves (web/src/demo/conditions.ts);
    // the charger's pins are wired, so every state can be shown.
    void SensorFeed::readBattery(JsonObjectConst in)
    {
        BatterySnapshot &b = snapshot.battery;
        BatteryReading &r = b.reading;
        JsonObjectConst p = in["battery"];
        b.detected = p["present"] | false;
        const bool failed = p["failed"] | false;
        b.initialized = cfg.battery.enabled && b.detected && !failed;
        if (!cfg.battery.enabled || !b.detected)
        {
            r = BatteryReading{};
            policy = Battery::PowerPolicy{};
            b.power = Battery::PowerState::Normal;
            b.shutdownInSeconds = 0;
            return;
        }
        const bool usb = p["usb"] | false;
        const bool fault = p["fault"] | false;
        const float percent = std::max(0.0f, std::min(100.0f, p["percent"] | 50.0f));
        // Charger lines, active low: PG with USB; STAT1/STAT2 as the MCP73871 drives them.
        Battery::ChargerPins pins;
        pins.pg = !usb;
        pins.stat1 = !(usb && (fault || percent < 99.5f));
        pins.stat2 = !(usb && (fault || percent >= 99.5f));
        const Battery::ChargerOutput charger = Battery::decodeCharger(pins);
        r.chargerPins = cfg.battery.chargerPins();
        r.stat1 = pins.stat1;
        r.stat2 = pins.stat2;
        r.pg = pins.pg;
        r.chargeState = charger.state;
        r.fault = charger.fault;
        r.usbPower = r.chargerPins ? std::optional<bool>(charger.usbPower) : std::nullopt;
        if (failed)
        {
            r.status = SensorStatus::Timeout;
            batteryPolicy();
            return;
        }
        const float rate = charger.state == Battery::ChargeState::Charging      ? 12.0f
                           : charger.state == Battery::ChargeState::Discharging ? -6.2f
                                                                                : 0.0f;
        r.status = SensorStatus::Ok;
        r.timestamp = nowMs;
        b.lastUpdate = nowMs;
        r.version = 0x0012;
        r.percent = percent;
        r.socRaw = static_cast<uint16_t>(percent * 256.0f);
        r.voltage = cellVoltage(percent);
        r.vcellRaw = static_cast<uint16_t>(r.voltage / 78.125e-6f);
        r.estimating = false;
        r.rateValid = true;
        r.ratePercentPerHour = rate;
        r.crateRaw = static_cast<int16_t>(rate / 0.208f);
        batteryPolicy();
    }

    // As PowerManager::tick, without the sleep: the demo device stays awake.
    void SensorFeed::batteryPolicy()
    {
        BatterySnapshot &b = snapshot.battery;
        const BatteryReading &r = b.reading;
        Battery::PolicyInput in;
        in.newRead = true;
        in.valid = b.initialized && r.status == SensorStatus::Ok && !r.estimating;
        in.percent = r.percent;
        in.state = r.chargeState;
        in.usbPower = r.usbPower;
        in.powerSaving = cfg.battery.powerSaving;
        in.criticalPercent = cfg.battery.criticalPercent;
        in.shutdownPercent = cfg.battery.shutdownPercent;
        in.graceSeconds = cfg.battery.shutdownGraceSeconds;
        in.nowMs = nowMs;
        const Battery::PolicyEvents events = policy.update(in);
        shutdownStarted = shutdownStarted || events.shutdownStarted;
        powerRestored = powerRestored || events.powerRestored;
        b.power = policy.state();
        b.shutdownInSeconds = policy.secondsLeft(nowMs, cfg.battery.shutdownGraceSeconds);
    }
} // namespace SQM
