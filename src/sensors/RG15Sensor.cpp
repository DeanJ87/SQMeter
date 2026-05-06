#include "sensors/RG15Sensor.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace SQM
{
    namespace
    {
        class MutexGuard
        {
        public:
            explicit MutexGuard(SemaphoreHandle_t mutex, TickType_t timeoutTicks = pdMS_TO_TICKS(20))
                : mutex(mutex), locked(mutex != nullptr && xSemaphoreTake(mutex, timeoutTicks) == pdTRUE)
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

        bool extractFloatField(const std::string &line, const char *label, float &value)
        {
            size_t labelPos = line.find(label);
            const size_t labelLength = std::strlen(label);

            while (labelPos != std::string::npos)
            {
                const bool startsField = labelPos == 0 ||
                                         line[labelPos - 1] == ',' ||
                                         std::isspace(static_cast<unsigned char>(line[labelPos - 1]));
                const size_t valuePos = labelPos + labelLength;
                const bool hasValueSeparator = valuePos < line.length() &&
                                               std::isspace(static_cast<unsigned char>(line[valuePos]));

                if (startsField && hasValueSeparator)
                {
                    const char *cursor = line.c_str() + valuePos;
                    while (*cursor != '\0' && std::isspace(static_cast<unsigned char>(*cursor)))
                    {
                        cursor++;
                    }

                    char *end = nullptr;
                    const float parsed = std::strtof(cursor, &end);
                    if (end == cursor)
                    {
                        return false;
                    }

                    value = parsed;
                    return true;
                }

                labelPos = line.find(label, labelPos + 1);
            }

            return false;
        }

        bool extractIntField(const std::string &line, const char *label, int &value)
        {
            size_t labelPos = line.find(label);
            const size_t labelLength = std::strlen(label);

            while (labelPos != std::string::npos)
            {
                const bool startsField = labelPos == 0 ||
                                         line[labelPos - 1] == ',' ||
                                         std::isspace(static_cast<unsigned char>(line[labelPos - 1]));
                const size_t valuePos = labelPos + labelLength;
                const bool hasValueSeparator = valuePos < line.length() &&
                                               std::isspace(static_cast<unsigned char>(line[valuePos]));

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

        bool hasFlagToken(const std::string &flags, const char *token)
        {
            size_t pos = flags.find(token);
            const size_t tokenLength = std::strlen(token);

            while (pos != std::string::npos)
            {
                const bool leftOk = pos == 0 ||
                                    std::isspace(static_cast<unsigned char>(flags[pos - 1])) ||
                                    flags[pos - 1] == ',';
                const size_t right = pos + tokenLength;
                const bool rightOk = right >= flags.length() ||
                                     std::isspace(static_cast<unsigned char>(flags[right])) ||
                                     flags[right] == ',';
                if (leftOk && rightOk)
                {
                    return true;
                }

                pos = flags.find(token, pos + 1);
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

    RG15Sensor::RG15Sensor(uint8_t rxPin, uint8_t txPin, uint32_t baudRate,
                           const std::string &mode, const std::string &resolution,
                           const std::string &units, bool enabled, bool debugUart)
        : enabledConfig(enabled), debugUart(debugUart), rxPin(rxPin), txPin(txPin),
          baudRate(baudRate), mode(mode), resolution(resolution), units(units),
          serial(std::make_unique<HardwareSerial>(UART_NUM)), stateMutex(xSemaphoreCreateMutex())
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
        return mode == "continuous" ? CONTINUOUS_STALE_TIMEOUT_MS : STALE_TIMEOUT_MS;
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

        Logger::info(TAG, "UART begin rx=%u tx=%u baud=%u port=%u mode=%s res=%s units=%s",
                     rxPin, txPin, baudRate, UART_NUM, mode.c_str(), resolution.c_str(), units.c_str());

        serial->begin(baudRate, SERIAL_8N1, rxPin, txPin);
        initialized = true;
        diagnostics.uartOpened = true;
        diagnostics.configured = true;
        updateDiagnosticsState(RG15State::RG15_UART_OPENED);

        if (debugUart)
        {
            Logger::info(TAG, "UART configured: rx=%u tx=%u baud=%u port=%u debug=%d",
                         rxPin, txPin, baudRate, UART_NUM, debugUart ? 1 : 0);
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

        if (mode == "polling")
        {
            if (debugUart)
            {
                Logger::info(TAG, "forcing polling mode: TX \"P\"");
            }
            sendCommand('P', "p");
        }
        else if (mode == "continuous")
        {
            if (debugUart)
            {
                Logger::info(TAG, "forcing continuous mode: TX \"C\"");
            }
            sendCommand('C', "c");
        }

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
                Logger::info(TAG, "unexpected response while waiting for ack \"%s\": \"%s\"",
                             expectedAck, ack.c_str());
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
        bool gotCommunication = false;
        if (mode == "polling")
        {
            gotCommunication = pollReading();
        }
        else
        {
            gotCommunication = drainBuffer();
            const uint32_t lastProbe = diagnostics.lastHealthCheckMs == 0 ? diagnostics.lastResponseMs : diagnostics.lastHealthCheckMs;
            if (lastProbe == 0 || now - lastProbe >= CONTINUOUS_HEALTH_CHECK_MS)
            {
                gotCommunication = queryHealth() || gotCommunication;
            }
        }

        if (!gotCommunication)
        {
            const uint32_t age = reading.timestamp == 0 ? 0 : now - reading.timestamp;
            reading.ageMs = age;

            if (reading.timestamp == 0)
            {
                reading.online = diagnostics.online;
                reading.stale = enabledConfig;
                if (mode == "polling")
                {
                    reading.status = SensorStatus::TIMEOUT;
                    updateDiagnosticsState(RG15State::RG15_TIMEOUT);
                }
                else if (diagnostics.state != RG15State::RG15_PARSE_ERROR)
                {
                    updateDiagnosticsState(RG15State::RG15_CONFIGURED);
                }
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

    bool RG15Sensor::queryHealth()
    {
        if (debugUart)
        {
            Logger::info(TAG, "health query baud: TX \"B\"");
        }
        diagnostics.lastHealthCheckMs = millis();
        return queryLineCommand('B', "Baud");
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
            if (c == 'p' || c == 'c' || c == 'm' || c == 'i' || c == 'h' || c == 'l' || c == 's' || c == 'x' || c == 'y')
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

        if (line.rfind("Baud ", 0) == 0 ||
            line.rfind("Reset ", 0) == 0 ||
            line.rfind("SW ", 0) == 0 ||
            line.rfind("Emitters ", 0) == 0 ||
            line.rfind("EmTotal ", 0) == 0 ||
            line.rfind("PwrDays ", 0) == 0 ||
            line.rfind("Event", 0) == 0 ||
            line.rfind(";", 0) == 0)
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
        reading.online = true;
        reading.stale = false;
        reading.ageMs = 0;
        updateDiagnosticsState(RG15State::RG15_ONLINE);

        if (debugUart)
        {
            Logger::info(TAG, "parsed acc=%.2f event=%.2f total=%.2f intensity=%.2f unit=%s",
                         reading.acc, reading.eventAcc, reading.totalAcc, reading.rInt,
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
        if (line.length() < 20)
        {
            if (debugUart)
            {
                Logger::info(TAG, "line too short to parse: \"%s\"", line.c_str());
            }
            return false;
        }

        float acc = 0.0f, eventAcc = 0.0f, totalAcc = 0.0f, rInt = 0.0f;
        if (!extractFloatField(line, "Acc", acc) ||
            !extractFloatField(line, "EventAcc", eventAcc) ||
            !extractFloatField(line, "TotalAcc", totalAcc) ||
            !extractFloatField(line, "RInt", rInt))
        {
            return false;
        }

        if (acc < 0.0f || acc > 9999.0f ||
            eventAcc < 0.0f || eventAcc > 9999.0f ||
            totalAcc < 0.0f || totalAcc > 999999.0f ||
            rInt < 0.0f || rInt > 9999.0f)
        {
            Logger::warn(TAG, "Out-of-range values in line: '%s'", line.c_str());
            return false;
        }

        reading.acc = acc;
        reading.eventAcc = eventAcc;
        reading.totalAcc = totalAcc;
        reading.rInt = rInt;
        reading.isRaining = (rInt > 0.0f);

        reading.lensBad = false;
        reading.emSat = false;

        const size_t rIntPos = line.find("RInt");
        size_t unitPos = rIntPos == std::string::npos ? std::string::npos : line.find("mmph", rIntPos);
        if (unitPos == std::string::npos)
        {
            unitPos = rIntPos == std::string::npos ? std::string::npos : line.find("iph", rIntPos);
        }

        if (unitPos != std::string::npos)
        {
            const size_t flagsStart = line.find(' ', unitPos);
            if (flagsStart != std::string::npos && flagsStart < line.length())
            {
                const std::string flags = line.substr(flagsStart);
                reading.lensBad = hasFlagToken(flags, "i") || hasFlagToken(flags, "LensBad");
                reading.emSat = hasFlagToken(flags, "o") || hasFlagToken(flags, "EmSat");
            }
        }

        reading.timestamp = millis();
        reading.ageMs = 0;
        reading.status = SensorStatus::OK;
        reading.online = true;
        reading.stale = false;
        lastUpdateTime = reading.timestamp;
        diagnostics.lastSuccessfulReadMs = reading.timestamp;

        return true;
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

    std::string RG15Sensor::toJson() const
    {
        StaticJsonDocument<2048> doc;
        const RG15Reading current = copyReading();
        const RG15Diagnostics diag = getDiagnostics();
        const uint32_t now = millis();

        doc["sensor"] = "hydreon_rg15";
        doc["enabled"] = diag.enabled;
        doc["initialized"] = diag.uartOpened;
        doc["online"] = current.online;
        doc["stale"] = current.stale;
        doc["state"] = stateToString(diag.state);
        doc["timestamp"] = current.timestamp;
        doc["ageMs"] = current.ageMs;
        doc["status"] = static_cast<int>(current.status);
        doc["isRaining"] = current.isRaining;
        doc["acc"] = current.acc;
        doc["eventAcc"] = current.eventAcc;
        doc["totalAcc"] = current.totalAcc;
        doc["rInt"] = current.rInt;
        doc["accumulation_since_last_read"] = current.acc;
        doc["event_accumulation"] = current.eventAcc;
        doc["total_accumulation"] = current.totalAcc;
        doc["rain_intensity"] = current.rInt;
        doc["lensBad"] = current.lensBad;
        doc["emSat"] = current.emSat;

        JsonObject uart = doc.createNestedObject("uart");
        uart["configured"] = diag.configured;
        uart["opened"] = diag.uartOpened;
        uart["rx_pin"] = diag.rxPin;
        uart["tx_pin"] = diag.txPin;
        uart["baud_rate"] = diag.baudRate;
        uart["uart_port"] = diag.uartPort;
        uart["mode"] = diag.mode;
        uart["resolution"] = diag.resolution;
        uart["units"] = diag.units;
        uart["debug_uart"] = diag.debugUart;
        if (diag.lastCommand)
            uart["last_command"] = diag.lastCommand->c_str();
        else
            uart["last_command"] = nullptr;
        if (diag.lastCommandMs != 0)
            uart["last_command_ms"] = static_cast<uint32_t>(diag.lastCommandMs);
        else
            uart["last_command_ms"] = nullptr;
        uart["last_bytes_written"] = diag.lastBytesWritten;
        if (diag.expectedAck)
            uart["expected_ack"] = diag.expectedAck->c_str();
        else
            uart["expected_ack"] = nullptr;
        if (diag.lastAck)
            uart["last_ack"] = diag.lastAck->c_str();
        else
            uart["last_ack"] = nullptr;
        if (diag.lastAckMs != 0)
            uart["last_ack_ms"] = static_cast<uint32_t>(diag.lastAckMs);
        else
            uart["last_ack_ms"] = nullptr;
        if (diag.lastRawResponse)
            uart["last_raw_response"] = diag.lastRawResponse->c_str();
        else
            uart["last_raw_response"] = nullptr;
        if (diag.lastResponseMs != 0)
            uart["last_response_ms"] = static_cast<uint32_t>(diag.lastResponseMs);
        else
            uart["last_response_ms"] = nullptr;
        if (diag.lastError)
            uart["last_error"] = diag.lastError->c_str();
        else
            uart["last_error"] = nullptr;
        uart["timeouts"] = diag.timeouts;
        uart["parse_errors"] = diag.parseErrors;
        uart["successful_reads"] = diag.successfulReads;
        uart["response_timeout_ms"] = diag.responseTimeoutMs;
        uart["stale_timeout_ms"] = diag.staleTimeoutMs;
        if (diag.lastHealthCheckMs != 0)
            uart["last_health_check_ms"] = static_cast<uint32_t>(diag.lastHealthCheckMs);
        else
            uart["last_health_check_ms"] = nullptr;
        if (diag.lastStatusLine)
            uart["last_status_line"] = diag.lastStatusLine->c_str();
        else
            uart["last_status_line"] = nullptr;
        if (diag.softwareVersion)
            uart["software_version"] = diag.softwareVersion->c_str();
        else
            uart["software_version"] = nullptr;
        if (diag.softwareBuildDate)
            uart["software_build_date"] = diag.softwareBuildDate->c_str();
        else
            uart["software_build_date"] = nullptr;
        if (diag.resetReason)
            uart["reset_reason"] = diag.resetReason->c_str();
        else
            uart["reset_reason"] = nullptr;
        if (diag.powerOnDays)
            uart["power_on_days"] = *diag.powerOnDays;
        else
            uart["power_on_days"] = nullptr;
        if (diag.emitter1)
            uart["emitter_1"] = *diag.emitter1;
        else
            uart["emitter_1"] = nullptr;
        if (diag.emitter2)
            uart["emitter_2"] = *diag.emitter2;
        else
            uart["emitter_2"] = nullptr;
        if (diag.emitterTotal)
            uart["emitter_total"] = *diag.emitterTotal;
        else
            uart["emitter_total"] = nullptr;
        if (diag.lastResponseMs != 0)
            uart["last_response_age_ms"] = static_cast<uint32_t>(now - diag.lastResponseMs);
        else
            uart["last_response_age_ms"] = nullptr;
        if (diag.lastSuccessfulReadMs != 0)
            uart["last_successful_read_ms"] = static_cast<uint32_t>(diag.lastSuccessfulReadMs);
        else
            uart["last_successful_read_ms"] = nullptr;
        if (diag.lastSuccessfulReadMs != 0)
            uart["last_successful_read_age_ms"] = static_cast<uint32_t>(now - diag.lastSuccessfulReadMs);
        else
            uart["last_successful_read_age_ms"] = nullptr;

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

    void RG15Sensor::reconfigure(uint8_t newRxPin, uint8_t newTxPin, uint32_t newBaudRate,
                                 const std::string &newMode, const std::string &newResolution,
                                 const std::string &newUnits, bool newDebugUart)
    {
        stop();

        rxPin = newRxPin;
        txPin = newTxPin;
        baudRate = newBaudRate;
        mode = newMode;
        resolution = newResolution;
        units = newUnits;
        debugUart = newDebugUart;
        enabledConfig = true;

        diagnostics.rxPin = rxPin;
        diagnostics.txPin = txPin;
        diagnostics.baudRate = baudRate;
        diagnostics.mode = mode;
        diagnostics.resolution = resolution;
        diagnostics.units = units;
        diagnostics.debugUart = debugUart;

        start(false);
    }

} // namespace SQM
