#pragma once

#include "sensors/SensorBase.h"
#include <Adafruit_TSL2591.h>
#include <memory>

namespace SQM
{

    struct TSL2591Reading : public SensorReading
    {
        float lux;         // Illuminance in lux
        uint16_t visible;  // Visible light (channel 0 - infrared)
        uint16_t infrared; // Infrared light (channel 1)
        uint32_t full;     // Full spectrum (channel 0)

        TSL2591Reading() : lux(0.0f), visible(0), infrared(0), full(0)
        {
            timestamp = 0;
            status = SensorStatus::NOT_INITIALIZED;
        }
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

        // Configuration
        void setGain(tsl2591Gain_t gain);
        void setIntegrationTime(tsl2591IntegrationTime_t time);

    private:
        static constexpr const char *TAG = "TSL2591";

        std::unique_ptr<Adafruit_TSL2591> sensor;
        TSL2591Reading reading;

        bool readSensor();
    };

} // namespace SQM
