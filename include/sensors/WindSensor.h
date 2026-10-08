#pragma once

#include "SensorBase.h"
#include "WindAggregator.h"
#include <cstdint>
#include <memory>

namespace SQM
{



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
        // Only allocated while the anemometer is enabled (2.4 KB of history).
        std::unique_ptr<Wind::WindAggregator> aggregator;
        WindReading reading;
        bool attached = false;
        uint32_t lastSampleAt = 0;
        uint8_t vaneMisses = 0;
    };

} // namespace SQM
