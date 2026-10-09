#pragma once

#include "sensors/SensorBase.h"
#include "Config.h"
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
        // A disabled sensor with the default settings (rain switched off).
        RG15Sensor();
        explicit RG15Sensor(const RainConfig &config);
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

        // New settings from Settings -> Sensors; the sensor restarts with them.
        void reconfigure(const RainConfig &config);

        void stop();

    private:
        static constexpr const char *TAG = "RG15";
        static constexpr uint32_t UART_NUM = 1;
        static constexpr uint32_t RESPONSE_TIMEOUT_MS = 1500;
        static constexpr uint32_t STALE_TIMEOUT_MS = 30000;
        static constexpr size_t LINE_BUFFER_SIZE = 128;
        static constexpr uint32_t ACK_QUIET_PERIOD_MS = 20;

        // Copies the settings into the sensor and its diagnostics.
        void applySettings(const RainConfig &config);

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
        int32_t loadLastResetDay();
        void saveLastResetDay(int32_t day);
        void forgetLastResetDay();
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
