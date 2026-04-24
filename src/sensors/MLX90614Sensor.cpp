#include "sensors/MLX90614Sensor.h"
#include "Logger.h"
#include <ArduinoJson.h>

namespace SQM
{

    MLX90614Sensor::MLX90614Sensor()
        : sensor(std::make_unique<Adafruit_MLX90614>())
    {
    }

    bool MLX90614Sensor::begin()
    {
        Logger::info(TAG, "Initializing MLX90614 sensor");

        if (!sensor->begin())
        {
            Logger::error(TAG, "Failed to initialize MLX90614");
            initialized = false;
            return false;
        }

        initialized = true;
        Logger::info(TAG, "MLX90614 initialized successfully");
        return true;
    }

    void MLX90614Sensor::update()
    {
        if (!initialized)
        {
            reading.status = SensorStatus::NOT_INITIALIZED;
            return;
        }

        if (readSensor())
        {
            reading.status = SensorStatus::OK;
            lastUpdateTime = millis();
        }
        else
        {
            reading.status = SensorStatus::READ_ERROR;
        }
    }

    bool MLX90614Sensor::readSensor()
    {
        reading.objectTemp = sensor->readObjectTempC();
        reading.ambientTemp = sensor->readAmbientTempC();
        reading.timestamp = millis();

        // Validate readings (MLX90614 valid range is -70 to +380°C)
        if (reading.objectTemp < -70 || reading.objectTemp > 380 ||
            reading.ambientTemp < -70 || reading.ambientTemp > 380)
        {
            Logger::warn(TAG, "Invalid temperature reading - Object: %.2f°C, Ambient: %.2f°C",
                         reading.objectTemp, reading.ambientTemp);
            return false;
        }

        Logger::debug(TAG, "Object: %.2f°C, Ambient: %.2f°C",
                      reading.objectTemp, reading.ambientTemp);

        return true;
    }

    std::string MLX90614Sensor::toJson() const
    {
        StaticJsonDocument<200> doc;

        doc["sensor"] = "MLX90614";
        doc["timestamp"] = reading.timestamp;
        doc["status"] = static_cast<int>(reading.status);
        doc["objectTemp"] = reading.objectTemp;
        doc["ambientTemp"] = reading.ambientTemp;

        std::string output;
        serializeJson(doc, output);
        return output;
    }

} // namespace SQM
