#include "sensors/RG15Sensor.h"
#include "Logger.h"
#include "RainLogic.h"
#include <ArduinoJson.h>
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

        bool extractIntField(const std::string &line, const char *label, int &value)
        {
            size_t labelPos = line.find(label);
            const size_t labelLength = std::strlen(label);

            while (labelPos != std::string::npos)
            {
                const bool startsField =
                    labelPos == 0 || line[labelPos - 1] == ',' || std::isspace(static_cast<unsigned char>(line[labelPos - 1]));
                const size_t valuePos = labelPos + labelLength;
                const bool hasValueSeparator = valuePos < line.length() && std::isspace(static_cast<unsigned char>(line[valuePos]));

                if (startsField && hasValueSeparator)
                {
                    const char *cursor = line.c_str() + valuePos;
                    while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)))
                    {
                        cursor++;
                    }

                    char *end = nullptr;
                    const long parsed = std::strtol(cursor, &end, 10);
                    if (end == cursor)
                    {
                        return false;
                    }

                    value = static_cast<int>(parsed);
                    return true;
                }

                labelPos = line.find(label, labelPos + 1);
            }

            return false;
        }

    } // namespace

    const char *RG15Sensor::stateToString(RG15State state)
    {
        switch (state)
        {
        case RG15State::RG15_DISABLED:
            return "disabled";
        case RG15State::RG15_CONFIGURED:
            return "configured";
        case RG15State::RG15_UART_OPENED:
            return "uart_opened";
        case RG15State::RG15_CONFIGURING:
            return "configuring";
        case RG15State::RG15_COMMAND_SENT:
            return "command_sent";
        case RG15State::RG15_AWAITING_RESPONSE:
            return "awaiting_response";
        case RG15State::RG15_ACKNOWLEDGED:
            return "acknowledged";
        case RG15State::RG15_READING_RECEIVED:
            return "reading_received";
        case RG15State::RG15_PARSE_ERROR:
            return "parse_error";
        case RG15State::RG15_TIMEOUT:
            return "timeout";
        case RG15State::RG15_STALE:
            return "stale";
        case RG15State::RG15_ONLINE:
            return "online";
        default:
            return "unknown";
        }
    }

    RG15Sensor::RG15Sensor(
        uint8_t rxPin,
        uint8_t txPin,
        uint32_t baudRate,
        const std::string &mode,
        const std::string &resolution,
        const std::string &units,
        bool enabled,
        bool debugUart,
        uint32_t pollIntervalMs,
        uint32_t rainClearDelayMs,
        bool dailyResetEnabled,
        uint8_t dailyResetHour,
        uint8_t dailyResetMinute)
        : enabledConfig(enabled),
          debugUart(debugUart),
          rxPin(rxPin),
          txPin(txPin),
          baudRate(baudRate),
          mode(mode),
          resolution(resolution),
          units(units),
          pollIntervalMs(pollIntervalMs),
          rainClearDelayMs(rainClearDelayMs),
          dailyResetEnabled(dailyResetEnabled),
          dailyResetHour(dailyResetHour),
          dailyResetMinute(dailyResetMinute),
          serial(std::make_unique<HardwareSerial>(UART_NUM)),
          stateMutex(xSemaphoreCreateMutex())
    {
        diagnostics.rxPin = rxPin;
        diagnostics.txPin = txPin;
        diagnostics.baudRate = baudRate;
        diagnostics.mode = mode;
        diagnostics.resolution = resolution;
        diagnostics.units = units;
        diagnostics.debugUart = debugUart;
        diagnostics.enabled = enabled;
        diagnostics.configured = enabled;
        diagnostics.pollIntervalMs = pollIntervalMs;
        diagnostics.rainClearDelayMs = rainClearDelayMs;
        diagnostics.dailyResetEnabled = dailyResetEnabled;
        diagnostics.dailyResetHour = dailyResetHour;
        diagnostics.dailyResetMinute = dailyResetMinute;
        diagnostics.responseTimeoutMs = RESPONSE_TIMEOUT_MS;
        diagnostics.staleTimeoutMs = effectiveStaleTimeoutMs();
        diagnostics.uartPort = UART_NUM;
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
        diagnostics.state = enabledConfig ? RG15State::RG15_CONFIGURED : RG15State::RG15_DISABLED;
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
            updateDiagnosticsState(RG15State::RG15_DISABLED);
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
        updateDiagnosticsState(RG15State::RG15_UART_OPENED);

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

        updateDiagnosticsState(RG15State::RG15_CONFIGURING);
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

        updateDiagnosticsState(RG15State::RG15_CONFIGURED);
    }

    bool RG15Sensor::sendCommand(char cmd, const char *expectedAck)
    {
        if (!initialized || !serial)
        {
            diagnostics.lastError = "uart_not_opened";
            updateDiagnosticsState(RG15State::RG15_DISABLED);
            return false;
        }

        while (serial->available())
        {
            serial->read();
        }

        diagnostics.lastCommand = std::string(1, cmd);
        diagnostics.lastCommandMs = millis();
        diagnostics.lastBytesWritten = 0;
        diagnostics.expectedAck = expectedAck ? std::optional<std::string>(std::string(expectedAck)) : std::nullopt;
        if (expectedAck)
        {
            diagnostics.lastAck.reset();
            diagnostics.lastRawResponse.reset();
        }
        diagnostics.lastError.reset();

        updateDiagnosticsState(RG15State::RG15_COMMAND_SENT);

        if (debugUart)
        {
            if (expectedAck)
            {
                Logger::info(TAG, "TX \"%c\" expect ack \"%s\"", cmd, expectedAck);
            }
            else
            {
                Logger::info(TAG, "TX \"%c\"", cmd);
            }
        }

        size_t written = 0;
        written += serial->write(static_cast<uint8_t>(cmd));
        written += serial->write(static_cast<uint8_t>('\n'));
        serial->flush();
        diagnostics.lastBytesWritten = written;

        updateDiagnosticsState(RG15State::RG15_AWAITING_RESPONSE);

        if (!expectedAck)
        {
            return true;
        }

        std::string ack;
        const uint32_t ackStartedAt = millis();
        bool gotAck = false;
        while (millis() - ackStartedAt < RESPONSE_TIMEOUT_MS)
        {
            if (!readLine(ack))
            {
                continue;
            }

            diagnostics.lastRawResponse = ack;
            diagnostics.lastResponseMs = millis();

            if (ack.size() == 1 && ack[0] == expectedAck[0])
            {
                gotAck = true;
                break;
            }

            if (handleControlLine(ack))
            {
                continue;
            }

            if (debugUart)
            {
                Logger::info(TAG, "unexpected response while waiting for ack \"%s\": \"%s\"", expectedAck, ack.c_str());
            }
        }

        if (!gotAck)
        {
            diagnostics.timeouts++;
            diagnostics.lastError = "timeout_waiting_for_ack";
            updateDiagnosticsState(RG15State::RG15_TIMEOUT);
            Logger::warn(TAG, "timeout waiting for ack \"%s\" after %u ms", expectedAck, RESPONSE_TIMEOUT_MS);
            return false;
        }

        diagnostics.lastAck = ack;
        diagnostics.lastAckMs = millis();
        diagnostics.lastRawResponse = ack;
        diagnostics.lastResponseMs = diagnostics.lastAckMs;
        markCommunicationOk();
        updateDiagnosticsState(RG15State::RG15_ACKNOWLEDGED);

        if (debugUart)
        {
            Logger::info(TAG, "RX ack \"%s\" after %ums", ack.c_str(), diagnostics.lastAckMs - diagnostics.lastCommandMs);
        }

        return true;
    }

    bool RG15Sensor::queryLineCommand(char cmd, const char *expectedPrefix)
    {
        if (!sendCommand(cmd))
        {
            return false;
        }

        std::string line;
        const uint32_t startedAt = millis();
        while (millis() - startedAt < RESPONSE_TIMEOUT_MS)
        {
            if (!readLine(line))
            {
                continue;
            }

            diagnostics.lastRawResponse = line;
            diagnostics.lastResponseMs = millis();
            markCommunicationOk();

            if (debugUart)
            {
                Logger::info(TAG, "RX raw: \"%s\"", line.c_str());
            }

            if (expectedPrefix == nullptr || line.rfind(expectedPrefix, 0) == 0)
            {
                diagnostics.lastError.reset();
                updateDiagnosticsState(RG15State::RG15_ACKNOWLEDGED);
                return true;
            }

            if (handleControlLine(line))
            {
                continue;
            }

            diagnostics.parseErrors++;
            diagnostics.lastError = "unexpected_response";
            updateDiagnosticsState(RG15State::RG15_PARSE_ERROR);
            if (debugUart)
            {
                Logger::info(TAG, "unexpected response to \"%c\": expected prefix \"%s\"", cmd, expectedPrefix);
            }
            return false;
        }

        diagnostics.timeouts++;
        diagnostics.lastError = "timeout_waiting_for_response";
        updateDiagnosticsState(RG15State::RG15_TIMEOUT);
        if (debugUart)
        {
            Logger::info(TAG, "timeout waiting for response to \"%c\" after %u ms", cmd, RESPONSE_TIMEOUT_MS);
        }
        return false;
    }

    void RG15Sensor::update()
    {
        MutexGuard guard(stateMutex);

        if (!initialized)
        {
            reading.status = enabledConfig ? SensorStatus::NOT_INITIALIZED : SensorStatus::NOT_INITIALIZED;
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
                reading.status = SensorStatus::TIMEOUT;
                updateDiagnosticsState(RG15State::RG15_TIMEOUT);
                return;
            }

            const uint32_t lastProofMs = diagnostics.lastResponseMs != 0 ? diagnostics.lastResponseMs : diagnostics.lastSuccessfulReadMs;
            const uint32_t proofAge = lastProofMs == 0 ? 0 : now - lastProofMs;
            const uint32_t staleTimeoutMs = effectiveStaleTimeoutMs();
            if (lastProofMs == 0 || proofAge > staleTimeoutMs)
            {
                if (reading.status == SensorStatus::OK)
                {
                    Logger::warn(TAG, "No data for %u ms, marking stale", staleTimeoutMs);
                }
                reading.status = SensorStatus::TIMEOUT;
                reading.online = diagnostics.online;
                reading.stale = true;
                updateDiagnosticsState(RG15State::RG15_STALE);
                diagnostics.stale = true;
            }
            else if (reading.status != SensorStatus::OK)
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
            updateDiagnosticsState(RG15State::RG15_DISABLED);
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
        updateDiagnosticsState(RG15State::RG15_TIMEOUT);
        reading.online = diagnostics.online;
        reading.stale = true;
        reading.status = SensorStatus::TIMEOUT;
        if (debugUart)
        {
            Logger::info(TAG, "timeout waiting for response after %u ms", RESPONSE_TIMEOUT_MS);
        }
        return false;
    }

    bool RG15Sensor::drainBuffer()
    {
        bool gotAny = false;
        std::string line;

        while (serial && serial->available())
        {
            if (readLine(line))
            {
                diagnostics.lastRawResponse = line;
                diagnostics.lastResponseMs = millis();
                markCommunicationOk();
                gotAny = true;

                if (debugUart)
                {
                    Logger::info(TAG, "RX raw: \"%s\"", line.c_str());
                }

                if (handleControlLine(line))
                {
                    continue;
                }

                handleRainLine(line);
            }
        }

        return gotAny;
    }

    bool RG15Sensor::handleControlLine(const std::string &line)
    {
        if (line.length() == 1)
        {
            const char c = line[0];
            if (c == 'p' || c == 'c' || c == 'm' || c == 'i' || c == 'h' || c == 'l' || c == 's' || c == 'x' || c == 'y' || c == 'o')
            {
                diagnostics.lastAck = line;
                diagnostics.lastAckMs = diagnostics.lastResponseMs;
                diagnostics.lastError.reset();
                updateDiagnosticsState(RG15State::RG15_ACKNOWLEDGED);
                if (debugUart)
                {
                    Logger::info(TAG, "RX async ack \"%s\"", line.c_str());
                }
                return true;
            }
        }

        if (line.rfind("Baud ", 0) == 0 || line.rfind("Reset ", 0) == 0 || line.rfind("SW ", 0) == 0 || line.rfind("Emitters ", 0) == 0 ||
            line.rfind("EmTotal ", 0) == 0 || line.rfind("PwrDays ", 0) == 0 || line.rfind("Event", 0) == 0 || line.rfind(";", 0) == 0)
        {
            diagnostics.lastStatusLine = line;
            if (line.rfind("Reset ", 0) == 0)
            {
                diagnostics.resetReason = line.substr(6);
            }
            else if (line.rfind("SW ", 0) == 0)
            {
                const size_t versionStart = 3;
                const size_t versionEnd = line.find(' ', versionStart);
                if (versionEnd != std::string::npos)
                {
                    diagnostics.softwareVersion = line.substr(versionStart, versionEnd - versionStart);
                    const size_t buildStart = line.find_first_not_of(' ', versionEnd);
                    if (buildStart != std::string::npos)
                    {
                        diagnostics.softwareBuildDate = line.substr(buildStart);
                    }
                }
            }
            else if (line.rfind("PwrDays ", 0) == 0)
            {
                char *end = nullptr;
                const float days = std::strtof(line.c_str() + 8, &end);
                if (end != line.c_str() + 8)
                {
                    diagnostics.powerOnDays = days;
                }
            }
            else if (line.rfind("Emitters ", 0) == 0)
            {
                int emitter1 = 0;
                int emitter2 = 0;
                int emitterTotal = 0;
                if (std::sscanf(line.c_str(), "Emitters %d %d", &emitter1, &emitter2) >= 2)
                {
                    diagnostics.emitter1 = emitter1;
                    diagnostics.emitter2 = emitter2;
                }
                if (extractIntField(line, "EmTotal", emitterTotal))
                {
                    diagnostics.emitterTotal = emitterTotal;
                }
            }
            else if (line.rfind("EmTotal ", 0) == 0)
            {
                int total = 0;
                if (std::sscanf(line.c_str(), "EmTotal %d", &total) == 1)
                {
                    diagnostics.emitterTotal = total;
                }
            }

            diagnostics.lastError.reset();
            if (debugUart)
            {
                Logger::info(TAG, "RX status line \"%s\"", line.c_str());
            }
            return true;
        }

        return false;
    }

    bool RG15Sensor::handleRainLine(const std::string &line)
    {
        updateDiagnosticsState(RG15State::RG15_READING_RECEIVED);

        if (!parseLine(line))
        {
            diagnostics.parseErrors++;
            diagnostics.lastError = "parse_failed_expected_fields";
            updateDiagnosticsState(RG15State::RG15_PARSE_ERROR);
            reading.online = diagnostics.online;
            reading.stale = true;
            reading.status = SensorStatus::INVALID_DATA;
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
        updateDiagnosticsState(RG15State::RG15_ONLINE);

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

    bool RG15Sensor::readLine(std::string &line)
    {
        line.clear();
        line.reserve(LINE_BUFFER_SIZE);

        const uint32_t start = millis();
        uint32_t lastByteMs = start;

        while (millis() - start < RESPONSE_TIMEOUT_MS)
        {
            while (serial && serial->available())
            {
                const char c = static_cast<char>(serial->read());
                lastByteMs = millis();

                if (c == '\n')
                {
                    if (!line.empty() && line.back() == '\r')
                    {
                        line.pop_back();
                    }
                    return !line.empty();
                }

                if (line.length() < LINE_BUFFER_SIZE - 1)
                {
                    line += c;
                }
            }

            if (!line.empty() && millis() - lastByteMs > ACK_QUIET_PERIOD_MS)
            {
                if (!line.empty() && line.back() == '\r')
                {
                    line.pop_back();
                }
                return true;
            }

            yield();
        }

        return false;
    }

    bool RG15Sensor::readAck(char expectedAck, std::string &ack)
    {
        ack.clear();

        const uint32_t start = millis();
        uint32_t lastByteMs = start;

        while (millis() - start < RESPONSE_TIMEOUT_MS)
        {
            while (serial && serial->available())
            {
                const char c = static_cast<char>(serial->read());
                lastByteMs = millis();

                if (c == '\r' || c == '\n')
                {
                    if (!ack.empty())
                    {
                        return ack.size() == 1 && ack[0] == expectedAck;
                    }
                    continue;
                }

                if (ack.length() < 8)
                {
                    ack += c;
                }
            }

            if (!ack.empty() && millis() - lastByteMs > ACK_QUIET_PERIOD_MS)
            {
                return ack.size() == 1 && ack[0] == expectedAck;
            }

            yield();
        }

        return false;
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
        reading.status = SensorStatus::OK;
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
        reading.localEventAcc = latch.eventAccumulation;
        diagnostics.lastRainDetectedMs = latch.lastRainMs;
    }

    void RG15Sensor::maybeRunScheduledTotalReset(uint32_t now)
    {
        if (!dailyResetEnabled || !initialized || !serial)
        {
            return;
        }

        const time_t currentTime = time(nullptr);
        if (currentTime < 1704067200) // dep: D-24 - Core::CLOCK_VALID_EPOCH, "the device doesn't know the time yet"
        {
            return;
        }

        tm localTime{};
        if (localtime_r(&currentTime, &localTime) == nullptr)
        {
            return;
        }

        if (localTime.tm_hour != dailyResetHour || localTime.tm_min != dailyResetMinute ||
            localTime.tm_yday == diagnostics.lastDailyResetYearDay)
        {
            return;
        }

        if (debugUart)
        {
            Logger::info(TAG, "daily total reset: TX \"O\"");
        }

        diagnostics.lastDailyResetYearDay = localTime.tm_yday;
        if (sendCommand('O'))
        {
            diagnostics.lastTotalResetMs = now;
            reading.totalAcc = 0.0f;
        }
    }

    RG15Reading RG15Sensor::copyReading() const
    {
        MutexGuard guard(stateMutex);
        RG15Reading copy = reading;
        if (copy.timestamp != 0)
        {
            copy.ageMs = millis() - copy.timestamp;
        }
        return copy;
    }

    RG15Diagnostics RG15Sensor::getDiagnostics() const
    {
        MutexGuard guard(stateMutex);
        RG15Diagnostics snapshot = diagnostics;
        const uint32_t now = millis();

        snapshot.enabled = enabledConfig;
        snapshot.configured = enabledConfig;
        snapshot.uartOpened = initialized;
        snapshot.online = reading.online;
        snapshot.stale = reading.stale;
        snapshot.debugUart = debugUart;
        snapshot.rxPin = rxPin;
        snapshot.txPin = txPin;
        snapshot.baudRate = baudRate;
        snapshot.mode = mode;
        snapshot.resolution = resolution;
        snapshot.units = units;
        snapshot.pollIntervalMs = pollIntervalMs;
        snapshot.rainClearDelayMs = rainClearDelayMs;
        snapshot.dailyResetEnabled = dailyResetEnabled;
        snapshot.dailyResetHour = dailyResetHour;
        snapshot.dailyResetMinute = dailyResetMinute;
        snapshot.staleTimeoutMs = effectiveStaleTimeoutMs();

        if (snapshot.lastCommandMs != 0 && snapshot.lastCommandMs <= now)
        {
            // keep as-is; JSON serialization will derive age
        }

        if (snapshot.lastAckMs != 0 && snapshot.lastAckMs <= now)
        {
            // keep as-is
        }

        if (snapshot.lastResponseMs != 0 && snapshot.lastResponseMs <= now)
        {
            // keep as-is
        }

        if (snapshot.lastSuccessfulReadMs != 0 && snapshot.lastSuccessfulReadMs <= now)
        {
            // keep as-is
        }

        return snapshot;
    }

    // The full diagnostics are in /api/status (WebServer::appendRainDiagnostics);
    // this is the plain reading, one name per value.
    std::string RG15Sensor::toJson() const
    {
        StaticJsonDocument<384> doc;
        const RG15Reading current = copyReading();
        doc["online"] = current.online;
        doc["stale"] = current.stale;
        doc["ageMs"] = current.ageMs;
        doc["raining"] = current.isRaining || current.rainLatched;
        doc["rainingNow"] = current.isRaining;
        doc["intensity"] = current.rInt;
        doc["eventAccumulation"] = current.localEventAcc;
        doc["sensorEventAccumulation"] = current.eventAcc;
        doc["totalAccumulation"] = current.totalAcc;
        doc["imperial"] = current.imperial;
        doc["lensFault"] = current.lensBad;
        doc["emitterSaturated"] = current.emSat;
        std::string output;
        serializeJson(doc, output);
        return output;
    }

    bool RG15Sensor::testCommunication()
    {
        MutexGuard guard(stateMutex);
        if (!guard.isLocked())
        {
            return false;
        }

        applyConfig();
        return pollReading();
    }

    bool RG15Sensor::resetTotalAccumulation()
    {
        MutexGuard guard(stateMutex);
        if (!guard.isLocked() || !initialized || !serial)
        {
            return false;
        }

        if (debugUart)
        {
            Logger::info(TAG, "manual total reset: TX \"O\"");
        }

        const bool ok = sendCommand('O');
        if (ok)
        {
            diagnostics.lastTotalResetMs = millis();
            reading.totalAcc = 0.0f;
        }
        return ok;
    }

    bool RG15Sensor::rebootSensor()
    {
        MutexGuard guard(stateMutex);
        if (!guard.isLocked() || !initialized || !serial)
        {
            return false;
        }

        if (debugUart)
        {
            Logger::info(TAG, "reboot sensor: TX \"K\"");
        }

        const bool ok = sendCommand('K');
        diagnostics.lastRebootCommandMs = millis();
        diagnostics.online = false;
        reading.online = false;
        reading.stale = true;
        updateDiagnosticsState(RG15State::RG15_COMMAND_SENT);
        return ok;
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
        updateDiagnosticsState(RG15State::RG15_DISABLED);
        Logger::info(TAG, "RG-15 stopped");
    }

    void RG15Sensor::reconfigure(
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
        uint8_t newDailyResetMinute)
    {
        stop();

        rxPin = newRxPin;
        txPin = newTxPin;
        baudRate = newBaudRate;
        mode = newMode;
        resolution = newResolution;
        units = newUnits;
        debugUart = newDebugUart;
        pollIntervalMs = newPollIntervalMs;
        rainClearDelayMs = newRainClearDelayMs;
        dailyResetEnabled = newDailyResetEnabled;
        dailyResetHour = newDailyResetHour;
        dailyResetMinute = newDailyResetMinute;
        enabledConfig = true;

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

        start(false);
    }

} // namespace SQM
