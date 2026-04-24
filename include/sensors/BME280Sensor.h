#pragma once

#include "sensors/SensorBase.h"
#include <Adafruit_BME280.h>
#include <memory>

namespace SQM
{

    struct BME280Reading : public SensorReading
    {
        float temperature; // Temperature in Celsius
        float humidity;    // Relative humidity in %
        float pressure;    // Atmospheric pressure in hPa
        float dewpoint;    // Calculated dewpoint in Celsius

        BME280Reading() : temperature(0.0f), humidity(0.0f), pressure(0.0f), dewpoint(0.0f)
        {
            timestamp = 0;
            status = SensorStatus::NOT_INITIALIZED;
        }

        // Validate all readings are within reasonable bounds
        bool isValid() const
        {
            return !isnan(temperature) && !isnan(humidity) && !isnan(pressure) && !isnan(dewpoint) &&
                   temperature >= -40.0f && temperature <= 85.0f && // BME280 valid range
                   humidity >= 0.0f && humidity <= 100.0f &&
                   pressure >= 300.0f && pressure <= 1100.0f;
        }
    };

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
