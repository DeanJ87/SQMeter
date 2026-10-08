#pragma once

#include "sensors/SensorBase.h"
#include <Adafruit_BME280.h>
#include <memory>

namespace SQM
{



    class BME280Sensor : public SensorBase
    {
    public:
        BME280Sensor();
        ~BME280Sensor() override = default;

        bool begin() override;
        void update() override;
        std::string getName() const override { return "BME280"; }
        std::string toJson() const override;

        const BME280Reading &getReading() const { return reading; }

    private:
        static constexpr const char *TAG = "BME280";
        static constexpr uint8_t I2C_ADDRESS = 0x76; // Default I2C address

        std::unique_ptr<Adafruit_BME280> sensor;
        BME280Reading reading;

        bool readSensor();
        bool validateReading() const;
        float calculateDewpoint(float temperature, float humidity) const;
    };

} // namespace SQM
