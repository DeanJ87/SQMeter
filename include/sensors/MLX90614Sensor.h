#pragma once

#include "SensorBase.h"
#include <Adafruit_MLX90614.h>
#include <memory>

namespace SQM
{

    struct MLX90614Reading : public SensorReading
    {
        float objectTemp = 0.0f;  // Object temperature in Celsius
        float ambientTemp = 0.0f; // Ambient temperature in Celsius
    };

    class MLX90614Sensor : public SensorBase
    {
    public:
        MLX90614Sensor();
        ~MLX90614Sensor() override = default;

        bool begin() override;
        void update() override;
        std::string getName() const override { return "MLX90614"; }
        std::string toJson() const override;

        const MLX90614Reading &getReading() const { return reading; }

    private:
        static constexpr const char *TAG = "MLX90614";

        std::unique_ptr<Adafruit_MLX90614> sensor;
        MLX90614Reading reading;

        bool readSensor();
    };

} // namespace SQM
