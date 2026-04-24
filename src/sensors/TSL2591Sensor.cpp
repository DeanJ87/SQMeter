#include "sensors/TSL2591Sensor.h"
#include "Logger.h"
#include <ArduinoJson.h>

namespace SQM
{

    TSL2591Sensor::TSL2591Sensor()
        : sensor(std::make_unique<Adafruit_TSL2591>(2591))
    {
    }

    bool TSL2591Sensor::begin()
    {
        Logger::info(TAG, "Initializing TSL2591 sensor");

        if (!sensor->begin())
        {
            Logger::error(TAG, "Failed to initialize TSL2591");
            initialized = false;
            return false;
        }

        // MAX gain (9876x) and longest integration time for dark sky monitoring
        // This gives maximum sensitivity for detecting very low light levels
        sensor->setGain(TSL2591_GAIN_MAX);                // 9876x gain - highest sensitivity
        sensor->setTiming(TSL2591_INTEGRATIONTIME_600MS); // Longest integration time

        initialized = true;
        Logger::info(TAG, "TSL2591 initialized successfully (MAX gain 9876x, 600ms integration)");
        return true;
    }

    void TSL2591Sensor::update()
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

    void TSL2591Sensor::setGain(tsl2591Gain_t gain)
    {
        if (sensor)
        {
            sensor->setGain(gain);
        }
    }

    void TSL2591Sensor::setIntegrationTime(tsl2591IntegrationTime_t time)
    {
        if (sensor)
        {
            sensor->setTiming(time);
        }
    }

    bool TSL2591Sensor::readSensor()
    {
        uint32_t lum = sensor->getFullLuminosity();
        reading.infrared = lum >> 16;
        reading.full = lum & 0xFFFF;
        reading.visible = reading.full - reading.infrared;

        // Custom lux calculation optimized for dark sky monitoring
        // Based on TSL2591 datasheet and dark sky community formulas
        // Using coefficient of 408.0 (common in SQM applications)
        const float LUX_COEFF = 408.0F;

        // Calculate counts per lux (CPL) based on fixed MAX gain and 600ms integration
        // With GAIN_MAX (9876x) and 600ms integration time
        float cpl = (100.0F * 600.0F / 100.0F) * 9876.0F / LUX_COEFF;

        // Check for saturation first - normal for outdoor sensors during daytime
        bool saturated = (reading.full >= 0xFFFF || reading.infrared >= 0xFFFF);

        // Calculate lux using visible channel (CH0 - CH1)
        float luxCalc = 0.0F;

        if (saturated)
        {
            // Sensor saturated - clamp to bright daylight value (~100,000 lux)
            luxCalc = 100000.0F;
            Logger::debug(TAG, "Sensor saturated (bright daylight) - Full: %u, IR: %u, Lux clamped to %.0f",
                          reading.full, reading.infrared, luxCalc);
        }
        else if (reading.full == 0)
        {
            luxCalc = 0.0F;
        }
        else if (reading.infrared > reading.full)
        {
            // IR overflow - use fallback with full spectrum
            luxCalc = (float)reading.full / cpl;
        }
        else
        {
            // Standard calculation: (Visible channel) / CPL
            luxCalc = ((float)reading.full - (float)reading.infrared) / cpl;

            // Allow reporting of EXTREMELY low values - no artificial floor
            // Dark skies can be as low as 0.00001 lux or even lower
            if (luxCalc < 0.0F)
            {
                luxCalc = 0.0F; // Only prevent negative values
            }
        }

        reading.lux = luxCalc;
        reading.timestamp = millis();

        // Handle very low light levels or invalid calculations
        if (reading.lux < 0 || isnan(reading.lux))
        {
            reading.lux = 0.0; // Report as 0 lux in complete darkness
            Logger::debug(TAG, "Complete darkness - Lux: 0.00, Visible: %u, IR: %u, Full: %u",
                          reading.visible, reading.infrared, reading.full);
        }
        else if (!saturated)
        {
            Logger::debug(TAG, "Lux: %.6f, Visible: %u, IR: %u, Full: %u",
                          reading.lux, reading.visible, reading.infrared, reading.full);
        }

        return true;
    }

    std::string TSL2591Sensor::toJson() const
    {
        StaticJsonDocument<200> doc;

        doc["sensor"] = "TSL2591";
        doc["timestamp"] = reading.timestamp;
        doc["status"] = static_cast<int>(reading.status);
        doc["lux"] = reading.lux;
        doc["visible"] = reading.visible;
        doc["infrared"] = reading.infrared;
        doc["full"] = reading.full;

        std::string output;
        serializeJson(doc, output);
        return output;
    }

} // namespace SQM
