#pragma once

#include "sensors/SensorBase.h"
#include <HardwareSerial.h>
#include <memory>
#include <optional>
#include <string>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace SQM
{
    enum class RG15State : uint8_t
    {
        RG15_DISABLED = 0,
        RG15_CONFIGURED,
        RG15_UART_OPENED,
        RG15_CONFIGURING,
        RG15_COMMAND_SENT,
        RG15_AWAITING_RESPONSE,
        RG15_ACKNOWLEDGED,
        RG15_READING_RECEIVED,
        RG15_PARSE_ERROR,
        RG15_TIMEOUT,
        RG15_STALE,
        RG15_ONLINE
    };

    struct RG15Reading : public SensorReading
    {
        bool isRaining;
        bool online;
        bool stale;
        float acc;      // Accumulation since last poll (mm or in)
        float eventAcc; // Event accumulation (mm or in)
        float totalAcc; // Total accumulation since power-on (mm or in)
        float rInt;     // Rain intensity (mm/h or in/h)
        bool lensBad;   // Hardware / lens fault
        bool emSat;     // Emitter saturation
        uint32_t ageMs;

        RG15Reading() : isRaining(false), online(false), stale(true), acc(0.0f),
                        eventAcc(0.0f), totalAcc(0.0f), rInt(0.0f), lensBad(false),
                        emSat(false), ageMs(0)
        {
            timestamp = 0;
            status = SensorStatus::NOT_INITIALIZED;
        }
    };

    struct RG15Diagnostics
    {
        bool enabled;
        bool configured;
        bool uartOpened;
        bool online;
        bool stale;
        bool debugUart;
        RG15State state;
        uint8_t rxPin;
        uint8_t txPin;
        uint32_t baudRate;
        uint32_t uartPort;
        std::string mode;
        std::string resolution;
        std::string units;
        std::optional<std::string> lastCommand;
        uint32_t lastCommandMs;
        size_t lastBytesWritten;
        std::optional<std::string> expectedAck;
        std::optional<std::string> lastAck;
        uint32_t lastAckMs;
        std::optional<std::string> lastRawResponse;
        uint32_t lastResponseMs;
        std::optional<std::string> lastError;
        std::optional<std::string> lastStatusLine;
        std::optional<std::string> softwareVersion;
        std::optional<std::string> softwareBuildDate;
        std::optional<std::string> resetReason;
        std::optional<float> powerOnDays;
        std::optional<int> emitter1;
        std::optional<int> emitter2;
        std::optional<int> emitterTotal;
        uint32_t lastHealthCheckMs;
        uint32_t lastSuccessfulReadMs;
        uint32_t timeouts;
        uint32_t parseErrors;
        uint32_t successfulReads;
        uint32_t responseTimeoutMs;
        uint32_t staleTimeoutMs;

        RG15Diagnostics()
            : enabled(false), configured(false), uartOpened(false), online(false), stale(false),
              debugUart(false), state(RG15State::RG15_DISABLED), rxPin(0), txPin(0), baudRate(9600),
              uartPort(1), mode("polling"), resolution("high"), units("metric"), lastCommand(std::nullopt),
              lastCommandMs(0), lastBytesWritten(0), expectedAck(std::nullopt), lastAck(std::nullopt),
              lastAckMs(0), lastRawResponse(std::nullopt), lastResponseMs(0), lastError(std::nullopt),
              lastStatusLine(std::nullopt), softwareVersion(std::nullopt), softwareBuildDate(std::nullopt),
              resetReason(std::nullopt), powerOnDays(std::nullopt), emitter1(std::nullopt),
              emitter2(std::nullopt), emitterTotal(std::nullopt), lastHealthCheckMs(0),
              lastSuccessfulReadMs(0), timeouts(0), parseErrors(0), successfulReads(0),
              responseTimeoutMs(500), staleTimeoutMs(30000)
        {
        }
    };

    class RG15Sensor : public SensorBase
    {
    public:
        RG15Sensor(uint8_t rxPin = 18, uint8_t txPin = 19, uint32_t baudRate = 9600,
                   const std::string &mode = "polling",
                   const std::string &resolution = "high",
                   const std::string &units = "metric",
                   bool enabled = false,
                   bool debugUart = false);
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

        void reconfigure(uint8_t newRxPin, uint8_t newTxPin, uint32_t newBaudRate,
                         const std::string &newMode, const std::string &newResolution,
                         const std::string &newUnits, bool newDebugUart);

        void stop();

    private:
        static constexpr const char *TAG = "RG15";
        static constexpr uint32_t UART_NUM = 1;
        static constexpr uint32_t RESPONSE_TIMEOUT_MS = 1500;
        static constexpr uint32_t STALE_TIMEOUT_MS = 30000;
        static constexpr uint32_t CONTINUOUS_HEALTH_CHECK_MS = 5UL * 60UL * 1000UL;
        static constexpr uint32_t CONTINUOUS_STALE_TIMEOUT_MS = (2UL * CONTINUOUS_HEALTH_CHECK_MS) + STALE_TIMEOUT_MS;
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

        std::unique_ptr<HardwareSerial> serial;
        RG15Reading reading;
        RG15Diagnostics diagnostics;
        SemaphoreHandle_t stateMutex;

        void resetSessionState();
        void updateDiagnosticsState(RG15State state);
        void markCommunicationOk();
        uint32_t effectiveStaleTimeoutMs() const;
        static const char *stateToString(RG15State state);
        bool start(bool probeImmediately);
        void applyConfig();
        bool sendCommand(char cmd, const char *expectedAck = nullptr);
        bool queryLineCommand(char cmd, const char *expectedPrefix = nullptr);
        bool queryHealth();
        bool pollReading();
        bool drainBuffer();
        bool readLine(std::string &line);
        bool readAck(char expectedAck, std::string &ack);
        bool handleControlLine(const std::string &line);
        bool handleRainLine(const std::string &line);
        bool parseLine(const std::string &line);
    };

} // namespace SQM
