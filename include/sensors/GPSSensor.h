#pragma once

#include "sensors/SensorBase.h"
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <memory>

namespace SQM
{

    class GPSSensor : public SensorBase
    {
    public:
        GPSSensor(uint8_t rxPin = 16, uint8_t txPin = 17, uint32_t baudRate = 9600);
        ~GPSSensor() override = default;

        bool begin() override;
        void update() override;
        // Feed the parser everything the UART has buffered; call every loop pass so
        // the RX buffer never overflows between sensor reads.
        void poll();
        std::string getName() const override { return "GPS"; }
        std::string toJson() const override;

        const GPSReading &getReading() const { return reading; }

        // Time-related methods for time synchronization
        bool hasValidTime() const;
        bool hasValidDate() const;
        bool getDateTime(struct tm *timeinfo) const;
        uint32_t getTimeSinceLastUpdate() const;

    private:
        static constexpr const char *TAG = "GPS";
        static constexpr uint32_t UART_NUM = 2;          // Use UART2
        static constexpr uint32_t GPS_TIMEOUT_MS = 5000; // Consider no fix if no update for 5s
        static constexpr size_t RX_BUFFER_BYTES = 1024;  // ~1 s of NMEA at 9600 baud

        uint8_t rxPin;
        uint8_t txPin;
        uint32_t baudRate;

        std::unique_ptr<HardwareSerial> serial;
        std::unique_ptr<TinyGPSPlus> gps;
        GPSReading reading;

        void updateReading();
    };

} // namespace SQM
