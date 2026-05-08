#pragma once

#include "sensors/SensorBase.h"
#include "Config.h"
#include <Adafruit_TSL2591.h>
#include <array>
#include <memory>

namespace SQM
{

    struct TSL2591Reading : public SensorReading
    {
        float lux;         // Illuminance in lux
        float rawLux;      // Lux before SQM calibration offset
        float rawSqm;      // SQM before SQM calibration offset
        float calibratedSqm;
        float rollingVisible;
        float correctedVisible;
        float darkVisibleOffset;
        uint16_t visible;  // Visible light (channel 0 - infrared)
        uint16_t infrared; // Infrared light (channel 1)
        uint32_t full;     // Full spectrum (channel 0)
        uint16_t integrationMs;
        uint16_t averagingWindowSeconds;
        uint16_t sampleCount;
        uint8_t gainIndex;
        float gainFactor;
        bool calibrated;
        bool saturated;
        bool nightMode;

        TSL2591Reading()
            : lux(0.0f), rawLux(0.0f), rawSqm(0.0f), calibratedSqm(0.0f), rollingVisible(0.0f),
              correctedVisible(0.0f), darkVisibleOffset(0.0f), visible(0), infrared(0), full(0),
              integrationMs(600), averagingWindowSeconds(90), sampleCount(0), gainIndex(3),
              gainFactor(9876.0f), calibrated(false), saturated(false), nightMode(false)
        {
            timestamp = 0;
            status = SensorStatus::NOT_INITIALIZED;
        }
    };

    struct TSL2591Diagnostics
    {
        const char *gainName;
        float gainFactor;
        uint16_t integrationMs;
        uint16_t averagingWindowSeconds;
        uint16_t sampleCount;
        uint16_t rejectedSamples;
        uint16_t consecutiveSaturatedSamples;
        uint16_t consecutiveLowSamples;
        float rollingFull;
        float rollingIr;
        float rollingVisible;
        float correctedVisible;
        float darkVisibleOffset;
        float rawSqm;
        float calibratedSqm;
        bool saturated;
        bool nightMode;
        bool calibrated;
    };

    class TSL2591Sensor : public SensorBase
    {
    public:
        TSL2591Sensor();
        ~TSL2591Sensor() override = default;

        bool begin() override;
        void update() override;
        std::string getName() const override { return "TSL2591"; }
        std::string toJson() const override;

        const TSL2591Reading &getReading() const { return reading; }
        TSL2591Diagnostics getDiagnostics() const;

        // Configuration
        void setGain(tsl2591Gain_t gain);
        void setIntegrationTime(tsl2591IntegrationTime_t time);
        void configureSkyMeasurement(const SkyAveragingConfig &averaging, const SkyCalibrationConfig &calibration);

    private:
        static constexpr const char *TAG = "TSL2591";
        static constexpr size_t MAX_ROLLING_SAMPLES = 512;

        std::unique_ptr<Adafruit_TSL2591> sensor;
        TSL2591Reading reading;
        std::array<uint16_t, MAX_ROLLING_SAMPLES> fullSamples{};
        std::array<uint16_t, MAX_ROLLING_SAMPLES> irSamples{};
        std::array<uint16_t, MAX_ROLLING_SAMPLES> visibleSamples{};
        size_t sampleHead = 0;
        size_t sampleCount = 0;
        uint16_t averagingWindowSeconds = 90;
        float sqmOffset = 0.0F;
        float darkVisibleOffset = 0.0F;
        bool calibrationEnabled = false;
        uint8_t gainIndex = 3;
        uint8_t integrationIndex = 5;
        uint16_t consecutiveSaturatedSamples = 0;
        uint16_t consecutiveLowSamples = 0;
        uint16_t rejectedSamples = 0;

        bool readSensor();
        void resetRollingSamples();
        void pushSample(uint16_t full, uint16_t ir, uint16_t visible);
        void updateRollingReading();
        void autoRange(uint16_t full, uint16_t ir);
        void applyRangeSettings();
        float currentCpl() const;
        bool isNightMode() const;
        const char *currentGainName() const;
        float currentGainFactor() const;
        uint16_t currentIntegrationMs() const;
    };

} // namespace SQM
