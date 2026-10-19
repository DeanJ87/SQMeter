#pragma once

#include "sensors/SensorBase.h"
#include "Config.h"
#include "calculations/SkyReadCycle.h"
#include <Adafruit_TSL2591.h>
#include <Wire.h>
#include <array>
#include <memory>

namespace SQM
{

    // One TSL2591 sky sensor on its own I2C bus (spec 030): the SQM on Wire,
    // the SQM-L on Wire1. Reads never block: tick() starts an integration,
    // returns, and collects the counts once the integration is over
    // (SkyReadCycle decides when), so both sensors integrate together. A
    // failed I2C transaction is a read error (an unplugged sensor never reads
    // as darkness); after repeated failures the sensor is re-begun with a
    // back-off, so a replugged sensor recovers without a restart.
    class TSL2591Sensor : public SensorBase
    {
    public:
        static constexpr uint32_t SAMPLE_INTERVAL_MS = 650;

        TSL2591Sensor(TwoWire &bus, SkyRole role);
        ~TSL2591Sensor() override = default;

        // Finds and configures the sensor. isInitialized() then stays true:
        // it has answered since boot.
        bool begin() override;
        void update() override { tick(millis()); }
        // Starts, collects or re-begins as the read cycle says; never waits.
        void tick(uint32_t nowMs);
        SkyRole role() const { return skyRole; }
        std::string getName() const override { return "TSL2591"; }
        std::string toJson() const override;

        const TSL2591Reading &getReading() const { return reading; }
        TSL2591Diagnostics getDiagnostics() const;

        // Configuration
        void setGain(tsl2591Gain_t gain);
        void setIntegrationTime(tsl2591IntegrationTime_t time);
        void configureSkyMeasurement(const SkyAveragingConfig &averaging, const SkyCalibrationConfig &calibration);

    private:
        static constexpr size_t MAX_ROLLING_SAMPLES = 512;

        // The rolling window (3 KB), allocated in the first successful
        // begin() so a sensor that is off or absent costs no heap.
        struct SampleBuffers
        {
            std::array<uint16_t, MAX_ROLLING_SAMPLES> full{};
            std::array<uint16_t, MAX_ROLLING_SAMPLES> ir{};
            std::array<uint16_t, MAX_ROLLING_SAMPLES> visible{};
        };

        TwoWire &bus;
        SkyRole skyRole;
        SkyReadCycle cycle{SAMPLE_INTERVAL_MS};
        bool beginFailureLogged = false;
        std::unique_ptr<Adafruit_TSL2591> sensor;
        TSL2591Reading reading;
        std::unique_ptr<SampleBuffers> samples;
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

        const char *tag() const { return skyRole == SkyRole::Sqml ? "TSL2591 SQM-L" : "TSL2591 SQM"; }
        bool writeEnable(uint8_t value);
        bool readChannels(uint16_t &full, uint16_t &ir);
        void startIntegration(uint32_t nowMs);
        void collect(uint32_t nowMs);
        void rebegin(uint32_t nowMs);
        void processCounts(uint16_t full, uint16_t ir);
        float luxFromCounts(bool saturated, float cpl) const;
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
