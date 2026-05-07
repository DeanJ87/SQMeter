#include "sensors/TSL2591Sensor.h"
#include "calculations/SkyQuality.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <algorithm>

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
    }

    TSL2591Sensor::TSL2591Sensor()
        : sensor(std::make_unique<Adafruit_TSL2591>(2591))
    {
    }

    bool TSL2591Sensor::begin()
    {
        Logger::info(TAG, "Initializing TSL2591 sensor");

        if (!sensor->begin())
        {
            Logger::error(TAG, "Failed to initialize TSL2591");
            initialized = false;
            return false;
        }

        // MAX gain (9876x) and longest integration time for dark sky monitoring
        // This gives maximum sensitivity for detecting very low light levels
        gainIndex = 3;
        integrationIndex = 5;
        applyRangeSettings();

        initialized = true;
        Logger::info(TAG, "TSL2591 initialized successfully (MAX gain 9876x, 600ms integration)");
        return true;
    }

    void TSL2591Sensor::update()
    {
        if (!initialized)
        {
            reading.status = SensorStatus::NOT_INITIALIZED;
            return;
        }

        if (readSensor())
        {
            reading.status = SensorStatus::OK;
            lastUpdateTime = millis();
        }
        else
        {
            reading.status = SensorStatus::READ_ERROR;
        }
    }

    void TSL2591Sensor::setGain(tsl2591Gain_t gain)
    {
        if (sensor)
        {
            sensor->setGain(gain);
        }
    }

    void TSL2591Sensor::setIntegrationTime(tsl2591IntegrationTime_t time)
    {
        if (sensor)
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

    bool TSL2591Sensor::readSensor()
    {
        uint32_t lum = sensor->getFullLuminosity();
        reading.infrared = lum >> 16;
        reading.full = lum & 0xFFFF;
        reading.visible = reading.full >= reading.infrared ? reading.full - reading.infrared : 0;

        const float cpl = currentCpl();

        // Check for saturation first - normal for outdoor sensors during daytime
        bool saturated = (reading.full >= 0xFFFF || reading.infrared >= 0xFFFF);
        reading.saturated = saturated || reading.full >= SATURATION_THRESHOLD || reading.infrared >= SATURATION_THRESHOLD;

        // Calculate lux using visible channel (CH0 - CH1)
        float luxCalc = 0.0F;

        if (saturated)
        {
            // Sensor saturated - clamp to bright daylight value (~100,000 lux)
            luxCalc = 100000.0F;
            Logger::debug(TAG, "Sensor saturated (bright daylight) - Full: %u, IR: %u, Lux clamped to %.0f",
                          reading.full, reading.infrared, luxCalc);
        }
        else if (reading.full == 0)
        {
            luxCalc = 0.0F;
        }
        else if (reading.infrared > reading.full)
        {
            // IR overflow - use fallback with full spectrum
            luxCalc = (float)reading.full / cpl;
        }
        else
        {
            // Standard calculation: (Visible channel) / CPL
            luxCalc = ((float)reading.full - (float)reading.infrared) / cpl;

            // Allow reporting of EXTREMELY low values - no artificial floor
            // Dark skies can be as low as 0.00001 lux or even lower
            if (luxCalc < 0.0F)
            {
                luxCalc = 0.0F; // Only prevent negative values
            }
        }

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
            Logger::debug(TAG, "Complete darkness - Lux: 0.00, Visible: %u, IR: %u, Full: %u",
                          reading.visible, reading.infrared, reading.full);
        }
        else if (!saturated)
        {
            Logger::debug(TAG, "Lux: %.6f, Visible: %u, IR: %u, Full: %u",
                          reading.lux, reading.visible, reading.infrared, reading.full);
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

        return true;
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
        const size_t maxSamplesForWindow = std::max<size_t>(1, std::min<size_t>(
                                                                   MAX_ROLLING_SAMPLES,
                                                                   (static_cast<size_t>(averagingWindowSeconds) * 1000UL) / currentIntegrationMs()));

        fullSamples[sampleHead] = full;
        irSamples[sampleHead] = ir;
        visibleSamples[sampleHead] = visible;
        sampleHead = (sampleHead + 1) % maxSamplesForWindow;
        if (sampleCount < maxSamplesForWindow)
        {
            sampleCount++;
        }
    }

    void TSL2591Sensor::updateRollingReading()
    {
        if (sampleCount == 0)
        {
            return;
        }

        uint32_t fullTotal = 0;
        uint32_t irTotal = 0;
        uint32_t visibleTotal = 0;
        for (size_t i = 0; i < sampleCount; i++)
        {
            fullTotal += fullSamples[i];
            irTotal += irSamples[i];
            visibleTotal += visibleSamples[i];
        }

        const float rollingFull = static_cast<float>(fullTotal) / static_cast<float>(sampleCount);
        const float rollingIr = static_cast<float>(irTotal) / static_cast<float>(sampleCount);
        const float rollingVisible = static_cast<float>(visibleTotal) / static_cast<float>(sampleCount);
        const float correctedVisible = std::max(0.0F, rollingVisible - darkVisibleOffset);
        const float rawLux = correctedVisible / currentCpl();
        const float rawSqm = SkyQuality::luxToSQM(rawLux);
        const float calibratedSqm = calibrationEnabled ? rawSqm + sqmOffset : rawSqm;
        const float calibratedLux = powf(10.0F, (12.6F - calibratedSqm) / 2.5F);

        reading.rollingVisible = rollingVisible;
        reading.correctedVisible = correctedVisible;
        reading.darkVisibleOffset = darkVisibleOffset;
        reading.sampleCount = static_cast<uint16_t>(std::min<size_t>(sampleCount, UINT16_MAX));
        reading.rawLux = rawLux;
        reading.rawSqm = rawSqm;
        reading.calibratedSqm = calibratedSqm;
        reading.calibrated = calibrationEnabled;
        reading.averagingWindowSeconds = averagingWindowSeconds;

        if (isNightMode())
        {
            reading.lux = calibratedLux;
        }

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
        if (!sensor)
        {
            return;
        }
        sensor->setGain(GAIN_LEVELS[gainIndex]);
        sensor->setTiming(INTEGRATION_LEVELS[integrationIndex]);
        reading.gainIndex = gainIndex;
        reading.gainFactor = currentGainFactor();
        reading.integrationMs = currentIntegrationMs();
        Logger::info(TAG, "Range set to gain %s (%.0fx), integration %ums",
                     currentGainName(), currentGainFactor(), currentIntegrationMs());
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
