#pragma once

#include "SensorBase.h"
#include "WindAggregator.h"
#include <cstdint>

namespace SQM
{

    struct WindReading : public SensorReading
    {
        float speedMs = 0.0f;      // 2-minute mean
        float gustMs = 0.0f;       // max 3 s mean over 10 minutes
        float instantMs = 0.0f;    // last 1 s
        float directionDeg = 0.0f; // 0 = North, clockwise; valid only when directionValid
        bool directionValid = false;
        bool vaneFault = false;    // vane enabled but its reading matches no position
        uint32_t samples = 0;      // seconds of history (up to 600)

        WindReading()
        {
            timestamp = 0;
            status = SensorStatus::NOT_INITIALIZED;
        }
    };

    struct WindSensorSettings
    {
        bool enabled = false;
        uint8_t speedPin = 27;
        bool directionEnabled = false;
        uint8_t directionPin = 35;
        float kmhPerHz = Wind::MISOL_KMH_PER_HZ;
        float directionOffsetDeg = 0.0f;
        float vanePullupOhms = 10000.0f;
    };

    // Reed-switch cup anemometer on a GPIO interrupt (with software
    // debounce - reed contacts bounce for longer than the PCNT glitch filter
    // can reject) plus an optional resistor-ladder wind vane on an ADC1 pin
    // (ADC2 is unusable while WiFi is on). Sampled once a second.
    class WindSensor : public SensorBase
    {
    public:
        ~WindSensor() override;

        void configure(const WindSensorSettings &settings);
        bool begin() override;
        void update() override; // call every loop iteration; samples once per second
        std::string getName() const override { return "Wind"; }
        std::string toJson() const override;

        const WindReading &getReading() const { return reading; }
        bool isEnabled() const { return settings.enabled; }

    private:
        static constexpr const char *TAG = "Wind";
        static constexpr uint32_t SAMPLE_INTERVAL_MS = 1000;
        static constexpr uint8_t VANE_FAULT_SECONDS = 10;
        static constexpr float ADC_REFERENCE_MV = 3300.0f;

        void attach();
        void detach();

        WindSensorSettings settings;
        Wind::WindAggregator aggregator;
        WindReading reading;
        bool attached = false;
        uint32_t lastSampleAt = 0;
        uint8_t vaneMisses = 0;
    };

} // namespace SQM
