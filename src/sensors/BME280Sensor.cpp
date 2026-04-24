#include "sensors/BME280Sensor.h"
#include "Logger.h"
#include <ArduinoJson.h>

namespace SQM
{

    BME280Sensor::BME280Sensor()
        : sensor(std::make_unique<Adafruit_BME280>())
    {
    }

    bool BME280Sensor::begin()
    {
        Logger::info(TAG, "Initializing BME280 sensor");

        if (!sensor->begin(I2C_ADDRESS))
        {
            Logger::error(TAG, "Failed to initialize BME280");
            initialized = false;
            return false;
        }

        // Configure sensor for weather monitoring
        sensor->setSampling(
            Adafruit_BME280::MODE_NORMAL,
            Adafruit_BME280::SAMPLING_X16,  // Temperature oversampling
            Adafruit_BME280::SAMPLING_X16,  // Pressure oversampling
            Adafruit_BME280::SAMPLING_X16,  // Humidity oversampling
            Adafruit_BME280::FILTER_X16,    // Filter coefficient
            Adafruit_BME280::STANDBY_MS_500 // Standby time
        );

        initialized = true;
        Logger::info(TAG, "BME280 initialized successfully");
        return true;
    }

    void BME280Sensor::update()
    {
        if (!initialized)
        {
            reading.status = SensorStatus::NOT_INITIALIZED;
            return;
        }

        if (readSensor() && validateReading())
        {
            reading.status = SensorStatus::OK;
            lastUpdateTime = millis();
        }
        else
        {
            reading.status = SensorStatus::READ_ERROR;
        }
    }

    bool BME280Sensor::readSensor()
    {
        reading.temperature = sensor->readTemperature();
        reading.humidity = sensor->readHumidity();
        reading.pressure = sensor->readPressure() / 100.0F; // Convert Pa to hPa
        reading.timestamp = millis();

        // Calculate dewpoint using Magnus formula
        reading.dewpoint = calculateDewpoint(reading.temperature, reading.humidity);

        Logger::debug(TAG, "Temp: %.2f°C, Humidity: %.2f%%, Pressure: %.2f hPa",
                      reading.temperature, reading.humidity, reading.pressure);

        return true;
    }

    bool BME280Sensor::validateReading() const
    {
        // Validate temperature (-40 to 85°C for BME280)
        if (reading.temperature < -40.0f || reading.temperature > 85.0f)
        {
            Logger::warn(TAG, "Temperature out of range: %.2f", reading.temperature);
            return false;
        }

        // Validate humidity (0-100%)
        if (reading.humidity < 0.0f || reading.humidity > 100.0f)
        {
            Logger::warn(TAG, "Humidity out of range: %.2f", reading.humidity);
            return false;
        }

        // Validate pressure (300-1100 hPa is reasonable range)
        if (reading.pressure < 300.0f || reading.pressure > 1100.0f)
        {
            Logger::warn(TAG, "Pressure out of range: %.2f", reading.pressure);
            return false;
        }

        return true;
    }

    std::string BME280Sensor::toJson() const
    {
        StaticJsonDocument<256> doc;

        doc["sensor"] = "BME280";
        doc["timestamp"] = reading.timestamp;
        doc["status"] = static_cast<int>(reading.status);
        doc["temperature"] = reading.temperature;
        doc["humidity"] = reading.humidity;
        doc["pressure"] = reading.pressure;
        doc["dewpoint"] = reading.dewpoint;

        std::string output;
        serializeJson(doc, output);
        return output;
    }

    float BME280Sensor::calculateDewpoint(float temperature, float humidity) const
    {
        // Magnus formula for dewpoint calculation
        // Valid for temperatures from -40°C to 50°C
        const float a = 17.27f;
        const float b = 237.7f;

        // Handle edge cases
        if (humidity <= 0.0f || humidity > 100.0f)
            return 0.0f;

        float alpha = ((a * temperature) / (b + temperature)) + logf(humidity / 100.0f);
        float dewpoint = (b * alpha) / (a - alpha);

        // Sanity check result
        if (isnan(dewpoint) || isinf(dewpoint))
            return 0.0f;

        return dewpoint;
    }

} // namespace SQM
