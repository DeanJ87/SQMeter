#pragma once

#include "sensors/SensorBase.h"
#include <HardwareSerial.h>
#include <memory>
#include <string>

namespace SQM
{

    struct RG15Reading : public SensorReading
    {
        bool isRaining;
        float acc;      // Accumulation since last poll (mm or in)
        float eventAcc; // Event accumulation (mm or in)
        float totalAcc; // Total accumulation since power-on (mm or in)
        float rInt;     // Rain intensity (mm/h or in/h)
        bool lensBad;   // Hardware / lens fault
        bool emSat;     // Emitter saturation

        RG15Reading() : isRaining(false), acc(0.0f), eventAcc(0.0f),
                        totalAcc(0.0f), rInt(0.0f), lensBad(false), emSat(false)
        {
            timestamp = 0;
            status = SensorStatus::NOT_INITIALIZED;
        }
    };

    class RG15Sensor : public SensorBase
    {
    public:
        RG15Sensor(uint8_t rxPin = 18, uint8_t txPin = 19, uint32_t baudRate = 9600,
                   const std::string &mode = "polling",
                   const std::string &resolution = "high",
                   const std::string &units = "metric");
        ~RG15Sensor() override = default;

        bool begin() override;
        void update() override;
        std::string getName() const override { return "RG15"; }
        std::string toJson() const override;

        const RG15Reading &getReading() const { return reading; }

        void reconfigure(uint8_t newRxPin, uint8_t newTxPin, uint32_t newBaudRate,
                         const std::string &newMode, const std::string &newResolution,
                         const std::string &newUnits);

        void stop();

    private:
        static constexpr const char *TAG = "RG15";
        static constexpr uint32_t UART_NUM = 1;
        static constexpr uint32_t RESPONSE_TIMEOUT_MS = 500;
        static constexpr uint32_t STALE_TIMEOUT_MS = 30000;
        static constexpr size_t LINE_BUFFER_SIZE = 128;

        uint8_t rxPin;
        uint8_t txPin;
        uint32_t baudRate;
        std::string mode;
        std::string resolution;
        std::string units;

        std::unique_ptr<HardwareSerial> serial;
        RG15Reading reading;

        void applyConfig();
        void sendCommand(char cmd);
        bool pollReading();
        bool drainBuffer();
        bool readLine(std::string &line);
        bool parseLine(const std::string &line);
    };

} // namespace SQM
