#pragma once

#include "SensorBase.h"
#include "As5600.h"
#include "WindAggregator.h"
#include <cstdint>
#include <memory>

class TwoWire;

namespace SQM
{

    struct WindSensorSettings
    {
        bool enabled = false;
        uint8_t speedPin = 27;
        bool directionEnabled = false;
        float kmhPerHz = Wind::MISOL_KMH_PER_HZ;
        float directionOffsetDeg = 0.0f;
    };

    // Reed-switch cup anemometer on a GPIO interrupt (with software
    // debounce - reed contacts bounce for longer than the PCNT glitch filter
    // can reject) plus an optional AS5600 wind vane on the main I2C bus.
    // Sampled once a second.
    class WindSensor : public SensorBase
    {
    public:
        explicit WindSensor(TwoWire &wire)
            : wire(wire)
        {
        }
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

        void attach();
        void detach();
        // That second's vane direction, or -1 without one.
        float readVane();

        TwoWire &wire;

        WindSensorSettings settings;
        // Only allocated while the anemometer is enabled (2.4 KB of history).
        std::unique_ptr<Wind::WindAggregator> aggregator;
        WindReading reading;
        bool attached = false;
        uint32_t lastSampleAt = 0;
        uint8_t vaneMisses = 0;
    };

} // namespace SQM
