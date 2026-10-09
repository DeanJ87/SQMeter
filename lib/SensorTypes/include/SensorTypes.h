#pragma once

// Plain sensor reading structs, shared by the sensor drivers (firmware) and
// lib/DeviceCore (firmware, native tests and the browser demo). No hardware
// headers here.

#include <cmath>
#include <cstdint>
#include <optional>
#include <string>

namespace SQM
{
    enum class SensorStatus
    {
        OK,
        NOT_INITIALIZED,
        READ_ERROR,
        TIMEOUT,
        INVALID_DATA
    };

    struct SensorReading
    {
        uint32_t timestamp = 0; // millis() of the reading; 0 = never read
        SensorStatus status = SensorStatus::NOT_INITIALIZED;

        bool isValid() const { return status == SensorStatus::OK; }
    };

    struct TSL2591Reading : public SensorReading
    {
        float lux;    // Illuminance in lux
        float rawLux; // Lux before SQM calibration offset
        float rawSqm; // SQM before SQM calibration offset
        float calibratedSqm;
        float rollingVisible;
        float correctedVisible;
        float darkVisibleOffset;
        uint16_t visible;  // Visible light (channel 0 - infrared)
        uint16_t infrared; // Infrared light (channel 1)
        uint32_t full;     // Full spectrum (channel 0)
        uint16_t integrationMs;
        uint16_t averagingWindowSeconds;
        uint16_t sampleCount;
        uint8_t gainIndex;
        float gainFactor;
        bool calibrated;
        bool saturated;
        bool nightMode;

        TSL2591Reading()
            : lux(0.0f),
              rawLux(0.0f),
              rawSqm(0.0f),
              calibratedSqm(0.0f),
              rollingVisible(0.0f),
              correctedVisible(0.0f),
              darkVisibleOffset(0.0f),
              visible(0),
              infrared(0),
              full(0),
              integrationMs(600),
              averagingWindowSeconds(90),
              sampleCount(0),
              gainIndex(3),
              gainFactor(9876.0f),
              calibrated(false),
              saturated(false),
              nightMode(false)
        {
            timestamp = 0;
            status = SensorStatus::NOT_INITIALIZED;
        }
    };

    struct TSL2591Diagnostics
    {
        const char *gainName;
        float gainFactor;
        uint16_t integrationMs;
        uint16_t averagingWindowSeconds;
        uint16_t sampleCount;
        uint16_t rejectedSamples;
        uint16_t consecutiveSaturatedSamples;
        uint16_t consecutiveLowSamples;
        float rollingFull;
        float rollingIr;
        float rollingVisible;
        float correctedVisible;
        float darkVisibleOffset;
        float rawSqm;
        float calibratedSqm;
        bool saturated;
        bool nightMode;
        bool calibrated;
    };

    struct BME280Reading : public SensorReading
    {
        float temperature; // Temperature in Celsius
        float humidity;    // Relative humidity in %
        float pressure;    // Atmospheric pressure in hPa
        float dewpoint;    // Calculated dewpoint in Celsius

        BME280Reading()
            : temperature(0.0f),
              humidity(0.0f),
              pressure(0.0f),
              dewpoint(0.0f)
        {
            timestamp = 0;
            status = SensorStatus::NOT_INITIALIZED;
        }

        // Validate all readings are within reasonable bounds
        bool isValid() const
        {
            return !std::isnan(temperature) && !std::isnan(humidity) && !std::isnan(pressure) && !std::isnan(dewpoint) &&
                   temperature >= -40.0f && temperature <= 85.0f && // BME280 valid range
                   humidity >= 0.0f && humidity <= 100.0f && pressure >= 300.0f && pressure <= 1100.0f;
        }
    };

    struct MLX90614Reading : public SensorReading
    {
        float objectTemp = 0.0f;  // Object temperature in Celsius
        float ambientTemp = 0.0f; // Ambient temperature in Celsius
    };

    struct GPSReading : public SensorReading
    {
        bool hasFix;         // GPS lock acquired
        uint32_t satellites; // Number of satellites in view
        double latitude;     // Latitude in degrees
        double longitude;    // Longitude in degrees
        double altitude;     // Altitude in meters above sea level
        uint32_t hdop;       // Horizontal Dilution of Precision (x100)
        uint32_t age;        // Age of fix data in milliseconds

        GPSReading()
            : hasFix(false),
              satellites(0),
              latitude(0.0),
              longitude(0.0),
              altitude(0.0),
              hdop(0),
              age(0)
        {
            timestamp = 0;
            status = SensorStatus::NOT_INITIALIZED;
        }
    };

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
        bool rainLatched;
        uint32_t lastRainMs; // millis() of the last reading that saw rain, 0 = never (the latch's)
        bool online;
        bool stale;
        float acc;           // Accumulation since last poll (mm or in)
        float eventAcc;      // Event accumulation (mm or in)
        float localEventAcc; // SQMeter event accumulation using rainClearDelayMs
        float totalAcc;      // Total accumulation since power-on (mm or in)
        float rInt;          // Rain intensity (mm/h or in/h)
        bool lensBad;        // Hardware / lens fault
        bool emSat;          // Emitter saturation
        bool imperial;       // Last response reported inches ("iph") rather than mm ("mmph")
        uint32_t ageMs;

        RG15Reading()
            : isRaining(false),
              rainLatched(false),
              lastRainMs(0),
              online(false),
              stale(true),
              acc(0.0f),
              eventAcc(0.0f),
              localEventAcc(0.0f),
              totalAcc(0.0f),
              rInt(0.0f),
              lensBad(false),
              emSat(false),
              imperial(false),
              ageMs(0)
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
        uint32_t pollIntervalMs;
        uint32_t rainClearDelayMs;
        bool dailyResetEnabled;
        uint8_t dailyResetHour;
        uint8_t dailyResetMinute;
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
        uint32_t lastPollMs;
        uint32_t lastRainDetectedMs;
        uint32_t lastTotalResetMs;
        uint32_t lastRebootCommandMs;
        int32_t lastDailyResetDay; // Rain::resetDay of the last daily reset, Rain::NO_RESET_DAY if none
        uint32_t lastSuccessfulReadMs;
        uint32_t timeouts;
        uint32_t parseErrors;
        uint32_t successfulReads;
        uint32_t responseTimeoutMs;
        uint32_t staleTimeoutMs;

        RG15Diagnostics()
            : enabled(false),
              configured(false),
              uartOpened(false),
              online(false),
              stale(false),
              debugUart(false),
              state(RG15State::RG15_DISABLED),
              rxPin(0),
              txPin(0),
              baudRate(9600),
              uartPort(1),
              mode("polling"),
              resolution("high"),
              units("metric"),
              pollIntervalMs(5000),
              rainClearDelayMs(900000),
              dailyResetEnabled(false),
              dailyResetHour(0),
              dailyResetMinute(0),
              lastCommand(std::nullopt),
              lastCommandMs(0),
              lastBytesWritten(0),
              expectedAck(std::nullopt),
              lastAck(std::nullopt),
              lastAckMs(0),
              lastRawResponse(std::nullopt),
              lastResponseMs(0),
              lastError(std::nullopt),
              lastStatusLine(std::nullopt),
              softwareVersion(std::nullopt),
              softwareBuildDate(std::nullopt),
              resetReason(std::nullopt),
              powerOnDays(std::nullopt),
              emitter1(std::nullopt),
              emitter2(std::nullopt),
              emitterTotal(std::nullopt),
              lastHealthCheckMs(0),
              lastPollMs(0),
              lastRainDetectedMs(0),
              lastTotalResetMs(0),
              lastRebootCommandMs(0),
              lastDailyResetDay(-1),
              lastSuccessfulReadMs(0),
              timeouts(0),
              parseErrors(0),
              successfulReads(0),
              responseTimeoutMs(500),
              staleTimeoutMs(30000)
        {
        }
    };

    struct WindReading : public SensorReading
    {
        float speedMs = 0.0f;      // 2-minute mean
        float gustMs = 0.0f;       // max 3 s mean over 10 minutes
        float instantMs = 0.0f;    // last 1 s
        float directionDeg = 0.0f; // 0 = North, clockwise; valid only when directionValid
        bool directionValid = false;
        bool vaneFault = false; // vane enabled but its reading matches no position
        uint32_t samples = 0;   // seconds of history (up to 600)

        WindReading()
        {
            timestamp = 0;
            status = SensorStatus::NOT_INITIALIZED;
        }
    };

} // namespace SQM
