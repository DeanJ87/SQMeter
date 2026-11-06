#include "sensors/GPSSensor.h"
#include "GpsTime.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <ctime>

namespace SQM
{

    GPSSensor::GPSSensor(uint8_t rxPin, uint8_t txPin, uint32_t baudRate)
        : rxPin(rxPin),
          txPin(txPin),
          baudRate(baudRate),
          serial(std::make_unique<HardwareSerial>(UART_NUM)),
          gps(std::make_unique<TinyGPSPlus>())
    {
    }

    bool GPSSensor::begin()
    {
        Logger::info(TAG, "Initializing GPS module (RX: %d, TX: %d, Baud: %d)", rxPin, txPin, baudRate);

        // Initialize hardware serial for GPS (the buffer size must be set before begin)
        serial->setRxBufferSize(RX_BUFFER_BYTES);
        serial->begin(baudRate, SERIAL_8N1, rxPin, txPin);

        if (!serial)
        {
            Logger::error(TAG, "Failed to initialize GPS serial");
            initialized = false;
            return false;
        }

        initialized = true;
        Logger::info(TAG, "GPS module initialized - waiting for fix...");
        return true;
    }

    void GPSSensor::update()
    {
        if (!initialized)
        {
            reading.status = SensorStatus::NotInitialized;
            return;
        }

        poll();
        updateReading();
    }

    void GPSSensor::poll()
    {
        if (!initialized)
            return;
        while (serial->available() > 0)
            gps->encode(static_cast<char>(serial->read()));
    }

    void GPSSensor::updateReading()
    {
        reading.timestamp = millis();

        // Check if we have a valid location fix
        if (gps->location.isValid() && gps->location.age() < GPS_TIMEOUT_MS)
        {
            reading.hasFix = true;
            reading.latitude = gps->location.lat();
            reading.longitude = gps->location.lng();
            reading.age = gps->location.age();
            reading.status = SensorStatus::Ok;
            lastUpdateTime = millis();

            Logger::debug(TAG, "GPS Fix: %.6f, %.6f", reading.latitude, reading.longitude);
        }
        else
        {
            reading.hasFix = false;
            reading.status = SensorStatus::ReadError;

            // Log periodically while waiting for fix
            static unsigned long lastLog = 0;
            if (millis() - lastLog > 30000) // Every 30 seconds
            {
                Logger::info(
                    TAG, "Waiting for GPS fix... (%d satellites visible)", gps->satellites.isValid() ? gps->satellites.value() : 0);
                lastLog = millis();
            }
        }

        // Get satellite count (always available even without fix)
        if (gps->satellites.isValid())
        {
            reading.satellites = gps->satellites.value();
        }
        else
        {
            reading.satellites = 0;
        }

        // Get altitude if available
        if (gps->altitude.isValid())
        {
            reading.altitude = gps->altitude.meters();
        }
        else
        {
            reading.altitude = 0.0;
        }

        // Get HDOP (horizontal dilution of precision)
        if (gps->hdop.isValid())
        {
            reading.hdop = gps->hdop.value();
        }
        else
        {
            reading.hdop = 0;
        }
    }

    std::string GPSSensor::toJson() const
    {
        StaticJsonDocument<512> doc;

        doc["sensor"] = "GPS";
        doc["timestamp"] = reading.timestamp;
        doc["status"] = static_cast<int>(reading.status);
        doc["hasFix"] = reading.hasFix;
        doc["satellites"] = reading.satellites;
        doc["latitude"] = reading.latitude;
        doc["longitude"] = reading.longitude;
        doc["altitude"] = reading.altitude;
        doc["hdop"] = reading.hdop / 100.0; // Convert back to actual HDOP value
        doc["age"] = reading.age;

        std::string output;
        serializeJson(doc, output);
        return output;
    }

    bool GPSSensor::hasValidTime() const
    {
        return initialized && gps->time.isValid() && gps->time.age() < 2000; // Fresh within 2 seconds
    }

    bool GPSSensor::hasValidDate() const
    {
        return initialized && gps->date.isValid() && gps->date.age() < 2000; // Fresh within 2 seconds
    }

    bool GPSSensor::getDateTime(struct tm *timeinfo) const
    {
        if (!hasValidTime() || !hasValidDate() || !timeinfo)
        {
            return false;
        }

        // GPS provides UTC time; older receivers report it on a date 1024 weeks early.
        const time_t epoch = static_cast<time_t>(GpsTime::undoRollover(GpsTime::utcEpoch(
            {gps->date.year(), gps->date.month(), gps->date.day(), gps->time.hour(), gps->time.minute(), gps->time.second()})));
        gmtime_r(&epoch, timeinfo);

        return true;
    }

    uint32_t GPSSensor::getTimeSinceLastUpdate() const
    {
        if (!gps->time.isValid())
        {
            return 0xFFFFFFFF; // Max value indicates never updated
        }
        return gps->time.age();
    }

} // namespace SQM
