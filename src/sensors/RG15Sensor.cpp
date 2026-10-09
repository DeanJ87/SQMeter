#include "sensors/RG15Sensor.h"
#include "sensors/RG15StatusLines.h"
#include "Logger.h"
#include "RainLogic.h"
#include <ArduinoJson.h>
#include <Preferences.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

namespace SQM
{
    namespace
    {
        class MutexGuard
        {
        public:
            explicit MutexGuard(SemaphoreHandle_t mutex, TickType_t timeoutTicks = pdMS_TO_TICKS(20))
                : mutex(mutex),
                  locked(mutex != nullptr && xSemaphoreTake(mutex, timeoutTicks) == pdTRUE)
            {
            }

            ~MutexGuard()
            {
                if (locked)
                {
                    xSemaphoreGive(mutex);
                }
            }

            bool isLocked() const { return locked; }

        private:
            SemaphoreHandle_t mutex;
            bool locked;
        };

    } // namespace

    namespace
    {
        RainConfig disabledRain()
        {
            RainConfig rain;
            rain.enabled = false;
            rain.rxPin = 18;
            rain.txPin = 19;
            rain.baudRate = 9600;
            rain.debugUart = false;
            rain.mode = "polling";
            rain.resolution = "high";
            rain.units = "metric";
            rain.pollIntervalMs = 5000;
            rain.rainClearDelayMs = 900000;
            rain.dailyResetEnabled = false;
            rain.dailyResetHour = 0;
            rain.dailyResetMinute = 0;
            return rain;
        }
    } // namespace

    RG15Sensor::RG15Sensor()
        : RG15Sensor(disabledRain())
    {
    }

    RG15Sensor::RG15Sensor(const RainConfig &config)
        : enabledConfig(config.enabled),
          serial(std::make_unique<HardwareSerial>(UART_NUM)),
          stateMutex(xSemaphoreCreateMutex())
    {
        applySettings(config);
        diagnostics.enabled = config.enabled;
        diagnostics.configured = config.enabled;
        diagnostics.responseTimeoutMs = RESPONSE_TIMEOUT_MS;
        diagnostics.staleTimeoutMs = effectiveStaleTimeoutMs();
        diagnostics.uartPort = UART_NUM;
    }

    void RG15Sensor::applySettings(const RainConfig &config)
    {
        rxPin = config.rxPin;
        txPin = config.txPin;
        baudRate = config.baudRate;
        mode = config.mode;
        resolution = config.resolution;
        units = config.units;
        debugUart = config.debugUart;
        pollIntervalMs = config.pollIntervalMs;
        rainClearDelayMs = config.rainClearDelayMs;
        dailyResetEnabled = config.dailyResetEnabled;
        dailyResetHour = config.dailyResetHour;
        dailyResetMinute = config.dailyResetMinute;

        diagnostics.rxPin = rxPin;
        diagnostics.txPin = txPin;
        diagnostics.baudRate = baudRate;
        diagnostics.mode = mode;
        diagnostics.resolution = resolution;
        diagnostics.units = units;
        diagnostics.debugUart = debugUart;
        diagnostics.pollIntervalMs = pollIntervalMs;
        diagnostics.rainClearDelayMs = rainClearDelayMs;
        diagnostics.dailyResetEnabled = dailyResetEnabled;
        diagnostics.dailyResetHour = dailyResetHour;
        diagnostics.dailyResetMinute = dailyResetMinute;
    }

    void RG15Sensor::resetSessionState()
    {
        reading = RG15Reading{};
        diagnostics = RG15Diagnostics{};
        diagnostics.rxPin = rxPin;
        diagnostics.txPin = txPin;
        diagnostics.baudRate = baudRate;
        diagnostics.mode = mode;
        diagnostics.resolution = resolution;
        diagnostics.units = units;
        diagnostics.debugUart = debugUart;
        diagnostics.enabled = enabledConfig;
        diagnostics.configured = enabledConfig;
        diagnostics.pollIntervalMs = pollIntervalMs;
        diagnostics.rainClearDelayMs = rainClearDelayMs;
        diagnostics.dailyResetEnabled = dailyResetEnabled;
        diagnostics.dailyResetHour = dailyResetHour;
        diagnostics.dailyResetMinute = dailyResetMinute;
        diagnostics.responseTimeoutMs = RESPONSE_TIMEOUT_MS;
        diagnostics.staleTimeoutMs = effectiveStaleTimeoutMs();
        diagnostics.uartPort = UART_NUM;
        diagnostics.state = enabledConfig ? RG15State::Configured : RG15State::Disabled;
    }

    void RG15Sensor::updateDiagnosticsState(RG15State state)
    {
        diagnostics.state = state;
    }

    void RG15Sensor::markCommunicationOk()
    {
        diagnostics.online = true;
        reading.online = true;
        reading.stale = false;
    }

    uint32_t RG15Sensor::effectiveStaleTimeoutMs() const
    {
        return STALE_TIMEOUT_MS;
    }

    bool RG15Sensor::start(bool probeImmediately)
    {
        MutexGuard guard(stateMutex);
        resetSessionState();

        if (!enabledConfig)
        {
            initialized = false;
            diagnostics.uartOpened = false;
            diagnostics.online = false;
            diagnostics.stale = false;
            updateDiagnosticsState(RG15State::Disabled);
            Logger::info(TAG, "RG-15 disabled in configuration");
            return true;
        }

        Logger::info(
            TAG,
            "UART begin rx=%u tx=%u baud=%u port=%u mode=%s res=%s units=%s",
            rxPin,
            txPin,
            baudRate,
            UART_NUM,
            mode.c_str(),
            resolution.c_str(),
            units.c_str());

        serial->begin(baudRate, SERIAL_8N1, rxPin, txPin);
        initialized = true;
        diagnostics.uartOpened = true;
        diagnostics.configured = true;
        updateDiagnosticsState(RG15State::UartOpened);

        if (debugUart)
        {
            Logger::info(TAG, "UART configured: rx=%u tx=%u baud=%u port=%u debug=%d", rxPin, txPin, baudRate, UART_NUM, debugUart ? 1 : 0);
        }

        applyConfig();

        if (probeImmediately)
        {
            // Prove communication immediately when possible.
            pollReading();
        }

        if (probeImmediately && !diagnostics.online)
        {
            Logger::warn(TAG, "sensor remains offline: no UART response received");
        }
        else if (probeImmediately && diagnostics.successfulReads == 0)
        {
            Logger::warn(TAG, "sensor responded, but no valid rain reading has been parsed yet");
        }

        return true;
    }

    bool RG15Sensor::begin()
    {
        return start(true);
    }

    void RG15Sensor::applyConfig()
    {
        if (!initialized)
        {
            return;
        }

        updateDiagnosticsState(RG15State::Configuring);
        diagnostics.lastAck.reset();
        diagnostics.lastRawResponse.reset();
        diagnostics.lastError.reset();

        drainBuffer();

        if (debugUart)
        {
            Logger::info(TAG, "query baud: TX \"B\"");
        }
        queryLineCommand('B', "Baud");

        if (debugUart)
        {
            Logger::info(TAG, "forcing polling mode: TX \"P\"");
        }
        sendCommand('P', "p");

        if (units == "metric")
        {
            if (debugUart)
            {
                Logger::info(TAG, "forcing metric units: TX \"M\"");
            }
            sendCommand('M', "m");
        }
        else if (units == "imperial")
        {
            if (debugUart)
            {
                Logger::info(TAG, "forcing imperial units: TX \"I\"");
            }
            sendCommand('I', "i");
        }

        if (resolution == "high")
        {
            if (debugUart)
            {
                Logger::info(TAG, "forcing high resolution: TX \"H\"");
            }
            sendCommand('H', "h");
        }
        else if (resolution == "low")
        {
            if (debugUart)
            {
                Logger::info(TAG, "forcing low resolution: TX \"L\"");
            }
            sendCommand('L', "l");
        }

        updateDiagnosticsState(RG15State::Configured);
    }

    void RG15Sensor::update()
    {
        MutexGuard guard(stateMutex);

        if (!initialized)
        {
            reading.status = enabledConfig ? SensorStatus::NotInitialized : SensorStatus::NotInitialized;
            reading.online = false;
            reading.stale = false;
            return;
        }

        const uint32_t now = millis();
        maybeRunScheduledTotalReset(now);
        if (reading.rainLatched)
        {
            Rain::Latch latch = currentLatch();
            Rain::expire(latch, now, rainClearDelayMs);
            applyLatch(latch);
        }
        bool gotCommunication = false;
        if (diagnostics.lastPollMs == 0 || now - diagnostics.lastPollMs >= pollIntervalMs)
        {
            diagnostics.lastPollMs = now;
            gotCommunication = pollReading();
        }

        if (!gotCommunication)
        {
            const uint32_t age = reading.timestamp == 0 ? 0 : now - reading.timestamp;
            reading.ageMs = age;

            if (reading.timestamp == 0)
            {
                reading.online = diagnostics.online;
                reading.stale = enabledConfig;
                reading.status = SensorStatus::Timeout;
                updateDiagnosticsState(RG15State::Timeout);
                return;
            }

            const uint32_t lastProofMs = diagnostics.lastResponseMs != 0 ? diagnostics.lastResponseMs : diagnostics.lastSuccessfulReadMs;
            const uint32_t proofAge = lastProofMs == 0 ? 0 : now - lastProofMs;
            const uint32_t staleTimeoutMs = effectiveStaleTimeoutMs();
            if (lastProofMs == 0 || proofAge > staleTimeoutMs)
            {
                if (reading.status == SensorStatus::Ok)
                {
                    Logger::warn(TAG, "No data for %u ms, marking stale", staleTimeoutMs);
                }
                reading.status = SensorStatus::Timeout;
                reading.online = diagnostics.online;
                reading.stale = true;
                updateDiagnosticsState(RG15State::Stale);
                diagnostics.stale = true;
            }
            else if (reading.status != SensorStatus::Ok)
            {
                reading.online = diagnostics.online;
                reading.stale = false;
            }
        }
    }

    bool RG15Sensor::pollReading()
    {
        if (!initialized || !serial)
        {
            diagnostics.lastError = "uart_not_opened";
            updateDiagnosticsState(RG15State::Disabled);
            return false;
        }

        if (debugUart)
        {
            Logger::info(TAG, "poll TX \"R\"");
        }

        if (!sendCommand('R'))
        {
            return false;
        }

        std::string line;
        const uint32_t startedAt = millis();
        bool gotAnyResponse = false;

        while (millis() - startedAt < RESPONSE_TIMEOUT_MS)
        {
            if (!readLine(line))
            {
                continue;
            }

            diagnostics.lastRawResponse = line;
            diagnostics.lastResponseMs = millis();
            markCommunicationOk();
            gotAnyResponse = true;

            if (debugUart)
            {
                Logger::info(TAG, "RX raw: \"%s\"", line.c_str());
            }

            if (handleControlLine(line))
            {
                continue;
            }

            return handleRainLine(line);
        }

        if (gotAnyResponse)
        {
            diagnostics.lastError.reset();
            reading.online = diagnostics.online;
            reading.stale = false;
            return false;
        }

        diagnostics.timeouts++;
        diagnostics.lastError = "timeout_waiting_for_response";
        updateDiagnosticsState(RG15State::Timeout);
        reading.online = diagnostics.online;
        reading.stale = true;
        reading.status = SensorStatus::Timeout;
        if (debugUart)
        {
            Logger::info(TAG, "timeout waiting for response after %u ms", RESPONSE_TIMEOUT_MS);
        }
        return false;
    }

    bool RG15Sensor::handleRainLine(const std::string &line)
    {
        updateDiagnosticsState(RG15State::ReadingReceived);

        if (!parseLine(line))
        {
            diagnostics.parseErrors++;
            diagnostics.lastError = "parse_failed_expected_fields";
            updateDiagnosticsState(RG15State::ParseError);
            reading.online = diagnostics.online;
            reading.stale = true;
            reading.status = SensorStatus::InvalidData;
            if (debugUart)
            {
                Logger::info(TAG, "parse failed: expected Acc/EventAcc/TotalAcc/RInt fields");
            }
            return false;
        }

        diagnostics.lastError.reset();
        diagnostics.successfulReads++;
        diagnostics.lastSuccessfulReadMs = reading.timestamp;
        updateRainLatch(reading.timestamp);
        reading.online = true;
        reading.stale = false;
        reading.ageMs = 0;
        updateDiagnosticsState(RG15State::Online);

        if (debugUart)
        {
            Logger::info(
                TAG,
                "parsed acc=%.2f event=%.2f total=%.2f intensity=%.2f unit=%s",
                reading.acc,
                reading.eventAcc,
                reading.totalAcc,
                reading.rInt,
                units == "imperial" ? "in" : "mm");
            Logger::info(TAG, "online=true age=%ums", 0u);
        }

        return true;
    }

    bool RG15Sensor::parseLine(const std::string &line)
    {
        Rain::Line parsed;
        const Rain::ParseResult result = Rain::parseLine(line, parsed);
        if (result != Rain::ParseResult::Ok)
        {
            if (result == Rain::ParseResult::OutOfRange)
                Logger::warn(TAG, "Out-of-range values in line: '%s'", line.c_str());
            else if (debugUart && result == Rain::ParseResult::TooShort)
                Logger::info(TAG, "line too short to parse: \"%s\"", line.c_str());
            return false;
        }

        reading.acc = parsed.acc;
        reading.eventAcc = parsed.eventAcc;
        reading.totalAcc = parsed.totalAcc;
        reading.rInt = parsed.rInt;
        reading.isRaining = parsed.rInt > 0.0f;
        reading.imperial = parsed.imperial;
        reading.lensBad = parsed.lensBad;
        reading.emSat = parsed.emSat;

        reading.timestamp = millis();
        reading.ageMs = 0;
        reading.status = SensorStatus::Ok;
        reading.online = true;
        reading.stale = false;
        lastUpdateTime = reading.timestamp;
        diagnostics.lastSuccessfulReadMs = reading.timestamp;

        return true;
    }

    void RG15Sensor::updateRainLatch(uint32_t now)
    {
        Rain::Latch latch = currentLatch();
        Rain::observe(latch, reading.rInt, reading.acc, now, rainClearDelayMs);
        applyLatch(latch);
    }

    Rain::Latch RG15Sensor::currentLatch() const
    {
        Rain::Latch latch;
        latch.latched = reading.rainLatched;
        latch.eventAccumulation = reading.localEventAcc;
        latch.lastRainMs = diagnostics.lastRainDetectedMs;
        return latch;
    }

    void RG15Sensor::applyLatch(const Rain::Latch &latch)
    {
        reading.rainLatched = latch.latched;
        reading.lastRainMs = latch.lastRainMs;
        reading.localEventAcc = latch.eventAccumulation;
        diagnostics.lastRainDetectedMs = latch.lastRainMs;
    }

    void RG15Sensor::stop()
    {
        MutexGuard guard(stateMutex);
        if (!guard.isLocked())
        {
            return;
        }

        if (initialized && serial)
        {
            serial->end();
        }

        initialized = false;
        enabledConfig = false;
        resetSessionState();
        diagnostics.enabled = false;
        diagnostics.configured = false;
        diagnostics.uartOpened = false;
        diagnostics.online = false;
        diagnostics.stale = false;
        updateDiagnosticsState(RG15State::Disabled);
        Logger::info(TAG, "RG-15 stopped");
    }

    void RG15Sensor::reconfigure(const RainConfig &config)
    {
        stop();

        // A schedule that's switched on or moved starts fresh: the current
        // day is adopted, so the change doesn't wipe today's total at once.
        if ((config.dailyResetEnabled && !dailyResetEnabled) || config.dailyResetHour != dailyResetHour ||
            config.dailyResetMinute != dailyResetMinute)
        {
            forgetLastResetDay();
        }

        applySettings(config);
        enabledConfig = true;
        start(false);
    }

} // namespace SQM
