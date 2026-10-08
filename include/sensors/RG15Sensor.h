#pragma once

#include "sensors/SensorBase.h"
#include "RainLogic.h"
#include <HardwareSerial.h>
#include <memory>
#include <optional>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace SQM
{

    class RG15Sensor : public SensorBase
    {
    public:
        RG15Sensor(
            uint8_t rxPin = 18,
            uint8_t txPin = 19,
            uint32_t baudRate = 9600,
            const std::string &mode = "polling",
            const std::string &resolution = "high",
            const std::string &units = "metric",
            bool enabled = false,
            bool debugUart = false,
            uint32_t pollIntervalMs = 5000,
            uint32_t rainClearDelayMs = 900000,
            bool dailyResetEnabled = false,
            uint8_t dailyResetHour = 0,
            uint8_t dailyResetMinute = 0);
        ~RG15Sensor() override = default;

        bool begin() override;
        void update() override;
        std::string getName() const override { return "RG15"; }
        std::string toJson() const override;

        const RG15Reading &getReading() const { return reading; }
        RG15Reading copyReading() const;
        RG15Diagnostics getDiagnostics() const;
        bool isOnline() const { return reading.online; }
        bool testCommunication();
        bool resetTotalAccumulation();
        bool rebootSensor();

        void reconfigure(
            uint8_t newRxPin,
            uint8_t newTxPin,
            uint32_t newBaudRate,
            const std::string &newMode,
            const std::string &newResolution,
            const std::string &newUnits,
            bool newDebugUart,
            uint32_t newPollIntervalMs,
            uint32_t newRainClearDelayMs,
            bool newDailyResetEnabled,
            uint8_t newDailyResetHour,
            uint8_t newDailyResetMinute);

        void stop();

    private:
        static constexpr const char *TAG = "RG15";
        static constexpr uint32_t UART_NUM = 1;
        static constexpr uint32_t RESPONSE_TIMEOUT_MS = 1500;
        static constexpr uint32_t STALE_TIMEOUT_MS = 30000;
        static constexpr size_t LINE_BUFFER_SIZE = 128;
        static constexpr uint32_t ACK_QUIET_PERIOD_MS = 20;

        bool enabledConfig;
        bool debugUart;
        uint8_t rxPin;
        uint8_t txPin;
        uint32_t baudRate;
        std::string mode;
        std::string resolution;
        std::string units;
        uint32_t pollIntervalMs;
        uint32_t rainClearDelayMs;
        bool dailyResetEnabled;
        uint8_t dailyResetHour;
        uint8_t dailyResetMinute;

        std::unique_ptr<HardwareSerial> serial;
        RG15Reading reading;
        RG15Diagnostics diagnostics;
        SemaphoreHandle_t stateMutex;

        void resetSessionState();
        void updateDiagnosticsState(RG15State state);
        void markCommunicationOk();
        uint32_t effectiveStaleTimeoutMs() const;
        void updateRainLatch(uint32_t now);
        Rain::Latch currentLatch() const;
        void applyLatch(const Rain::Latch &latch);
        void maybeRunScheduledTotalReset(uint32_t now);
        static const char *stateToString(RG15State state);
        bool start(bool probeImmediately);
        void applyConfig();
        bool sendCommand(char cmd, const char *expectedAck = nullptr);
        bool queryLineCommand(char cmd, const char *expectedPrefix = nullptr);
        bool pollReading();
        bool drainBuffer();
        bool readLine(std::string &line);
        bool readAck(char expectedAck, std::string &ack);
        bool handleControlLine(const std::string &line);
        bool handleRainLine(const std::string &line);
        bool parseLine(const std::string &line);
    };

} // namespace SQM
