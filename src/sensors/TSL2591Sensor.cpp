#include "sensors/TSL2591Sensor.h"
#include "calculations/SkyQuality.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <algorithm>
#include <new>

namespace SQM
{
    namespace
    {
        constexpr float TSL2591_LUX_COEFFICIENT = 408.0F;
        constexpr uint16_t SATURATION_THRESHOLD = 60000;
        constexpr uint16_t LOW_LIGHT_THRESHOLD = 750;
        constexpr uint8_t RANGE_CHANGE_SAMPLES = 4;

        constexpr tsl2591Gain_t GAIN_LEVELS[] = {
            TSL2591_GAIN_LOW,
            TSL2591_GAIN_MED,
            TSL2591_GAIN_HIGH,
            TSL2591_GAIN_MAX,
        };

        constexpr float GAIN_FACTORS[] = {
            1.0F,
            25.0F,
            428.0F,
            9876.0F,
        };

        constexpr const char *GAIN_NAMES[] = {
            "LOW",
            "MEDIUM",
            "HIGH",
            "MAX",
        };

        constexpr tsl2591IntegrationTime_t INTEGRATION_LEVELS[] = {
            TSL2591_INTEGRATIONTIME_100MS,
            TSL2591_INTEGRATIONTIME_200MS,
            TSL2591_INTEGRATIONTIME_300MS,
            TSL2591_INTEGRATIONTIME_400MS,
            TSL2591_INTEGRATIONTIME_500MS,
            TSL2591_INTEGRATIONTIME_600MS,
        };

        constexpr uint16_t INTEGRATION_MS[] = {
            100,
            200,
            300,
            400,
            500,
            600,
        };

        // Registers (TSL2591 datasheet): command bit + normal transaction.
        constexpr uint8_t I2C_ADDRESS = TSL2591_ADDR;
        constexpr uint8_t COMMAND = 0xA0;
        constexpr uint8_t REG_ENABLE = 0x00;
        constexpr uint8_t REG_CHAN0_LOW = 0x14; // C0DATAL, C0DATAH, C1DATAL, C1DATAH
        constexpr uint8_t ENABLE_ADC = 0x03;    // PON | AEN
        constexpr uint8_t ENABLE_OFF = 0x00;
    } // namespace

    TSL2591Sensor::TSL2591Sensor(TwoWire &bus, SkyRole role)
        : bus(bus),
          skyRole(role),
          sensor(std::make_unique<Adafruit_TSL2591>(2591))
    {
    }

    bool TSL2591Sensor::begin()
    {
        const uint32_t now = millis();
        if (!sensor->begin(&bus))
        {
            if (!beginFailureLogged)
                Logger::error(tag(), "Not found on its I2C bus; retrying in the background");
            beginFailureLogged = true;
            cycle.beginDone(now, false);
            return false;
        }
        if (!samples)
        {
            samples.reset(new (std::nothrow) SampleBuffers());
            if (!samples)
            {
                Logger::error(tag(), "No memory for the averaging window");
                cycle.beginDone(now, false);
                return false;
            }
        }

        // MAX gain (9876x) and the longest integration for dark skies.
        gainIndex = 3;
        integrationIndex = 5;
        applyRangeSettings();
        resetRollingSamples();

        initialized = true;
        beginFailureLogged = false;
        cycle.beginDone(now, true);
        Logger::info(tag(), "Initialized (MAX gain 9876x, 600ms integration)");
        return true;
    }

    void TSL2591Sensor::tick(uint32_t nowMs)
    {
        switch (cycle.next(nowMs, currentIntegrationMs()))
        {
        case SkyReadCycle::Action::Begin:
            rebegin(nowMs);
            break;
        case SkyReadCycle::Action::Start:
            startIntegration(nowMs);
            break;
        case SkyReadCycle::Action::Collect:
            collect(nowMs);
            break;
        case SkyReadCycle::Action::None:
            break;
        }
    }

    void TSL2591Sensor::rebegin(uint32_t nowMs)
    {
        (void)nowMs;
        if (!begin() && !initialized)
            reading.status = SensorStatus::NotInitialized;
    }

    bool TSL2591Sensor::writeEnable(uint8_t value)
    {
        bus.beginTransmission(I2C_ADDRESS);
        bus.write(static_cast<uint8_t>(COMMAND | REG_ENABLE));
        bus.write(value);
        return bus.endTransmission() == 0;
    }

    // CHAN0 then CHAN1 in one read: reading C0DATAL latches all four bytes.
    bool TSL2591Sensor::readChannels(uint16_t &full, uint16_t &ir)
    {
        bus.beginTransmission(I2C_ADDRESS);
        bus.write(static_cast<uint8_t>(COMMAND | REG_CHAN0_LOW));
        if (bus.endTransmission(false) != 0)
            return false;
        if (bus.requestFrom(I2C_ADDRESS, static_cast<uint8_t>(4)) != 4)
            return false;
        uint8_t bytes[4];
        for (uint8_t &b : bytes)
            b = static_cast<uint8_t>(bus.read());
        full = static_cast<uint16_t>(bytes[1] << 8 | bytes[0]);
        ir = static_cast<uint16_t>(bytes[3] << 8 | bytes[2]);
        return true;
    }

    void TSL2591Sensor::startIntegration(uint32_t nowMs)
    {
        if (writeEnable(ENABLE_ADC))
        {
            cycle.started(nowMs);
            return;
        }
        reading.status = SensorStatus::ReadError;
        cycle.startFailed(nowMs);
        if (cycle.state() == SkyReadCycle::State::Absent)
            Logger::warn(tag(), "Not responding; retrying in the background");
    }

    void TSL2591Sensor::collect(uint32_t nowMs)
    {
        uint16_t full = 0;
        uint16_t ir = 0;
        const bool ok = readChannels(full, ir);
        writeEnable(ENABLE_OFF); // power down until the next start; a failure shows on the next read
        cycle.collected(nowMs, ok);
        if (!ok)
        {
            reading.status = SensorStatus::ReadError;
            if (cycle.state() == SkyReadCycle::State::Absent)
                Logger::warn(tag(), "Not responding; retrying in the background");
            return;
        }
        processCounts(full, ir);
        reading.status = SensorStatus::Ok;
        lastUpdateTime = millis();
    }

    void TSL2591Sensor::setGain(tsl2591Gain_t gain)
    {
        // Never before a successful begin(): the library would begin() itself on Wire.
        if (sensor && initialized)
        {
            sensor->setGain(gain);
        }
    }

    void TSL2591Sensor::setIntegrationTime(tsl2591IntegrationTime_t time)
    {
        if (sensor && initialized)
        {
            sensor->setTiming(time);
        }
    }

    void TSL2591Sensor::configureSkyMeasurement(const SkyAveragingConfig &averaging, const SkyCalibrationConfig &calibration)
    {
        const uint16_t clampedWindow = std::max<uint16_t>(10, std::min<uint16_t>(averaging.windowSeconds, 300));
        if (clampedWindow != averagingWindowSeconds)
        {
            averagingWindowSeconds = clampedWindow;
            resetRollingSamples();
        }

        calibrationEnabled = calibration.enabled;
        sqmOffset = calibration.sqmOffset;
        darkVisibleOffset = std::max(0.0F, calibration.darkVisibleOffset);
        reading.averagingWindowSeconds = averagingWindowSeconds;
        reading.darkVisibleOffset = darkVisibleOffset;
        reading.calibrated = calibrationEnabled;
    }

    // Lux from the latest counts: the visible channel (CH0 - CH1) over CPL.
    float TSL2591Sensor::luxFromCounts(bool saturated, float cpl) const
    {
        if (saturated)
        {
            // Sensor saturated - clamp to bright daylight value (~100,000 lux)
            Logger::debug(
                tag(),
                "Sensor saturated (bright daylight) - Full: %u, IR: %u, Lux clamped to %.0f",
                reading.full,
                reading.infrared,
                100000.0F);
            return 100000.0F;
        }
        if (reading.full == 0)
            return 0.0F;
        if (reading.infrared > reading.full)
            return (float)reading.full / cpl; // IR overflow - use fallback with full spectrum
        // Allow reporting of EXTREMELY low values - no artificial floor (dark
        // skies can be 0.00001 lux or lower); only prevent negative values.
        const float lux = ((float)reading.full - (float)reading.infrared) / cpl;
        return lux < 0.0F ? 0.0F : lux;
    }

    void TSL2591Sensor::processCounts(uint16_t full, uint16_t ir)
    {
        reading.infrared = ir;
        reading.full = full;
        reading.visible = reading.full >= reading.infrared ? reading.full - reading.infrared : 0;

        const float cpl = currentCpl();

        // Check for saturation first - normal for outdoor sensors during daytime
        bool saturated = (reading.full >= 0xFFFF || reading.infrared >= 0xFFFF);
        reading.saturated = saturated || reading.full >= SATURATION_THRESHOLD || reading.infrared >= SATURATION_THRESHOLD;

        const float luxCalc = luxFromCounts(saturated, cpl);

        reading.rawLux = luxCalc;
        reading.lux = luxCalc;
        reading.timestamp = millis();
        reading.gainIndex = gainIndex;
        reading.gainFactor = currentGainFactor();
        reading.integrationMs = currentIntegrationMs();
        reading.nightMode = isNightMode();

        // Handle very low light levels or invalid calculations
        if (reading.lux < 0 || isnan(reading.lux))
        {
            reading.lux = 0.0; // Report as 0 lux in complete darkness
            Logger::debug(
                tag(), "Complete darkness - Lux: 0.00, Visible: %u, IR: %u, Full: %u", reading.visible, reading.infrared, reading.full);
        }
        else if (!saturated)
        {
            Logger::debug(tag(), "Lux: %.6f, Visible: %u, IR: %u, Full: %u", reading.lux, reading.visible, reading.infrared, reading.full);
        }

        if (!reading.saturated)
        {
            pushSample(static_cast<uint16_t>(reading.full), reading.infrared, reading.visible);
            updateRollingReading();
        }
        else
        {
            rejectedSamples++;
        }

        autoRange(static_cast<uint16_t>(reading.full), reading.infrared);
    }

    void TSL2591Sensor::resetRollingSamples()
    {
        sampleHead = 0;
        sampleCount = 0;
        rejectedSamples = 0;
        reading.sampleCount = 0;
        reading.rollingVisible = 0.0F;
        reading.correctedVisible = 0.0F;
    }

    void TSL2591Sensor::pushSample(uint16_t full, uint16_t ir, uint16_t visible)
    {
        const size_t maxSamplesForWindow = std::max<size_t>(
            1, std::min<size_t>(MAX_ROLLING_SAMPLES, (static_cast<size_t>(averagingWindowSeconds) * 1000UL) / currentIntegrationMs()));

        if (!samples)
            return;
        samples->full[sampleHead] = full;
        samples->ir[sampleHead] = ir;
        samples->visible[sampleHead] = visible;
        sampleHead = (sampleHead + 1) % maxSamplesForWindow;
        if (sampleCount < maxSamplesForWindow)
        {
            sampleCount++;
        }
    }

    void TSL2591Sensor::updateRollingReading()
    {
        if (sampleCount == 0 || !samples)
        {
            return;
        }

        uint32_t fullTotal = 0;
        uint32_t irTotal = 0;
        uint32_t visibleTotal = 0;
        for (size_t i = 0; i < sampleCount; i++)
        {
            fullTotal += samples->full[i];
            irTotal += samples->ir[i];
            visibleTotal += samples->visible[i];
        }

        const float rollingFull = static_cast<float>(fullTotal) / static_cast<float>(sampleCount);
        const float rollingIr = static_cast<float>(irTotal) / static_cast<float>(sampleCount);
        const float rollingVisible = static_cast<float>(visibleTotal) / static_cast<float>(sampleCount);
        const float correctedVisible = std::max(0.0F, rollingVisible - darkVisibleOffset);
        const float rawLux = correctedVisible / currentCpl();
        const CalibratedLight light = SkyQuality::calibrate(rawLux, calibrationEnabled, sqmOffset);

        reading.rollingVisible = rollingVisible;
        reading.correctedVisible = correctedVisible;
        reading.darkVisibleOffset = darkVisibleOffset;
        reading.sampleCount = static_cast<uint16_t>(std::min<size_t>(sampleCount, UINT16_MAX));
        reading.rawLux = rawLux;
        reading.rawSqm = light.rawSqm;
        reading.calibratedSqm = light.calibratedSqm;
        reading.calibrated = calibrationEnabled;
        reading.averagingWindowSeconds = averagingWindowSeconds;
        // The averaged, calibrated value in every mode (spec 001 FR-003/FR-005):
        // samples reset on each range change, so the average never mixes gains.
        reading.lux = light.lux;

        (void)rollingFull;
        (void)rollingIr;
    }

    void TSL2591Sensor::autoRange(uint16_t full, uint16_t ir)
    {
        if (full >= SATURATION_THRESHOLD || ir >= SATURATION_THRESHOLD)
        {
            consecutiveSaturatedSamples++;
            consecutiveLowSamples = 0;
        }
        else if (full <= LOW_LIGHT_THRESHOLD && ir <= LOW_LIGHT_THRESHOLD)
        {
            consecutiveLowSamples++;
            consecutiveSaturatedSamples = 0;
        }
        else
        {
            consecutiveSaturatedSamples = 0;
            consecutiveLowSamples = 0;
        }

        if (consecutiveSaturatedSamples >= RANGE_CHANGE_SAMPLES)
        {
            if (gainIndex > 0)
            {
                gainIndex--;
                applyRangeSettings();
                resetRollingSamples();
            }
            else if (integrationIndex > 0)
            {
                integrationIndex--;
                applyRangeSettings();
                resetRollingSamples();
            }
            consecutiveSaturatedSamples = 0;
        }
        else if (consecutiveLowSamples >= RANGE_CHANGE_SAMPLES)
        {
            if (integrationIndex < 5)
            {
                integrationIndex++;
                applyRangeSettings();
                resetRollingSamples();
            }
            else if (gainIndex < 3)
            {
                gainIndex++;
                applyRangeSettings();
                resetRollingSamples();
            }
            consecutiveLowSamples = 0;
        }
    }

    void TSL2591Sensor::applyRangeSettings()
    {
        // Called after a successful library begin() (begin) or on a begun sensor (autoRange).
        if (!sensor)
        {
            return;
        }
        sensor->setGain(GAIN_LEVELS[gainIndex]);
        sensor->setTiming(INTEGRATION_LEVELS[integrationIndex]);
        reading.gainIndex = gainIndex;
        reading.gainFactor = currentGainFactor();
        reading.integrationMs = currentIntegrationMs();
        Logger::info(
            tag(), "Range set to gain %s (%.0fx), integration %ums", currentGainName(), currentGainFactor(), currentIntegrationMs());
    }

    float TSL2591Sensor::currentCpl() const
    {
        return (static_cast<float>(currentIntegrationMs()) * currentGainFactor()) / TSL2591_LUX_COEFFICIENT;
    }

    bool TSL2591Sensor::isNightMode() const
    {
        return gainIndex == 3 && integrationIndex == 5;
    }

    const char *TSL2591Sensor::currentGainName() const
    {
        return GAIN_NAMES[gainIndex];
    }

    float TSL2591Sensor::currentGainFactor() const
    {
        return GAIN_FACTORS[gainIndex];
    }

    uint16_t TSL2591Sensor::currentIntegrationMs() const
    {
        return INTEGRATION_MS[integrationIndex];
    }

    TSL2591Diagnostics TSL2591Sensor::getDiagnostics() const
    {
        TSL2591Diagnostics diagnostics{};
        diagnostics.gainName = currentGainName();
        diagnostics.gainFactor = currentGainFactor();
        diagnostics.integrationMs = currentIntegrationMs();
        diagnostics.averagingWindowSeconds = averagingWindowSeconds;
        diagnostics.sampleCount = static_cast<uint16_t>(std::min<size_t>(sampleCount, UINT16_MAX));
        diagnostics.rejectedSamples = rejectedSamples;
        diagnostics.consecutiveSaturatedSamples = consecutiveSaturatedSamples;
        diagnostics.consecutiveLowSamples = consecutiveLowSamples;
        diagnostics.rollingFull = 0.0F;
        diagnostics.rollingIr = 0.0F;
        diagnostics.rollingVisible = reading.rollingVisible;
        diagnostics.correctedVisible = reading.correctedVisible;
        diagnostics.darkVisibleOffset = darkVisibleOffset;
        diagnostics.rawSqm = reading.rawSqm;
        diagnostics.calibratedSqm = reading.calibratedSqm;
        diagnostics.saturated = reading.saturated;
        diagnostics.nightMode = isNightMode();
        diagnostics.calibrated = calibrationEnabled;
        return diagnostics;
    }

    std::string TSL2591Sensor::toJson() const
    {
        StaticJsonDocument<512> doc;

        doc["sensor"] = "TSL2591";
        doc["timestamp"] = reading.timestamp;
        doc["status"] = static_cast<int>(reading.status);
        doc["lux"] = reading.lux;
        doc["rawLux"] = reading.rawLux;
        doc["rawSqm"] = reading.rawSqm;
        doc["calibratedSqm"] = reading.calibratedSqm;
        doc["visible"] = reading.visible;
        doc["infrared"] = reading.infrared;
        doc["full"] = reading.full;
        doc["rollingVisible"] = reading.rollingVisible;
        doc["correctedVisible"] = reading.correctedVisible;
        doc["sampleCount"] = reading.sampleCount;
        doc["gain"] = currentGainName();
        doc["gainFactor"] = currentGainFactor();
        doc["integrationMs"] = currentIntegrationMs();
        doc["saturated"] = reading.saturated;
        doc["nightMode"] = isNightMode();

        std::string output;
        serializeJson(doc, output);
        return output;
    }

} // namespace SQM
