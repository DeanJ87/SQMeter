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

// RG-15 commands and serial-line I/O: sending a command, waiting for its ack,
// reading lines and recording the status lines in between.

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

    bool RG15Sensor::sendCommand(char cmd, const char *expectedAck)
    {
        if (!initialized || !serial)
        {
            diagnostics.lastError = "uart_not_opened";
            updateDiagnosticsState(RG15State::Disabled);
            return false;
        }
        writeCommand(cmd, expectedAck);
        if (!expectedAck)
            return true;

        std::string ack;
        if (!waitForAck(expectedAck, ack))
        {
            diagnostics.timeouts++;
            diagnostics.lastError = "timeout_waiting_for_ack";
            updateDiagnosticsState(RG15State::Timeout);
            Logger::warn(TAG, "timeout waiting for ack \"%s\" after %u ms", expectedAck, RESPONSE_TIMEOUT_MS);
            return false;
        }
        diagnostics.lastAck = ack;
        diagnostics.lastAckMs = millis();
        diagnostics.lastRawResponse = ack;
        diagnostics.lastResponseMs = diagnostics.lastAckMs;
        markCommunicationOk();
        updateDiagnosticsState(RG15State::Acknowledged);
        if (debugUart)
            Logger::info(TAG, "RX ack \"%s\" after %ums", ack.c_str(), diagnostics.lastAckMs - diagnostics.lastCommandMs);
        return true;
    }

    // Clears anything pending, then sends `cmd` and a newline.
    void RG15Sensor::writeCommand(char cmd, const char *expectedAck)
    {
        while (serial->available())
            serial->read();

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
        updateDiagnosticsState(RG15State::CommandSent);

        if (debugUart)
        {
            if (expectedAck)
                Logger::info(TAG, "TX \"%c\" expect ack \"%s\"", cmd, expectedAck);
            else
                Logger::info(TAG, "TX \"%c\"", cmd);
        }

        size_t written = 0;
        written += serial->write(static_cast<uint8_t>(cmd));
        written += serial->write(static_cast<uint8_t>('\n'));
        serial->flush();
        diagnostics.lastBytesWritten = written;
        updateDiagnosticsState(RG15State::AwaitingResponse);
    }

    // Reads lines until the one-character ack arrives or the response
    // timeout passes; status lines and async acks on the way are recorded.
    bool RG15Sensor::waitForAck(const char *expectedAck, std::string &ack)
    {
        const uint32_t ackStartedAt = millis();
        while (millis() - ackStartedAt < RESPONSE_TIMEOUT_MS)
        {
            if (!readLine(ack))
                continue;
            diagnostics.lastRawResponse = ack;
            diagnostics.lastResponseMs = millis();
            if (ack.size() == 1 && ack[0] == expectedAck[0])
                return true;
            if (handleControlLine(ack))
                continue;
            if (debugUart)
                Logger::info(TAG, "unexpected response while waiting for ack \"%s\": \"%s\"", expectedAck, ack.c_str());
        }
        return false;
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
                updateDiagnosticsState(RG15State::Acknowledged);
                return true;
            }

            if (handleControlLine(line))
            {
                continue;
            }

            diagnostics.parseErrors++;
            diagnostics.lastError = "unexpected_response";
            updateDiagnosticsState(RG15State::ParseError);
            if (debugUart)
            {
                Logger::info(TAG, "unexpected response to \"%c\": expected prefix \"%s\"", cmd, expectedPrefix);
            }
            return false;
        }

        diagnostics.timeouts++;
        diagnostics.lastError = "timeout_waiting_for_response";
        updateDiagnosticsState(RG15State::Timeout);
        if (debugUart)
        {
            Logger::info(TAG, "timeout waiting for response to \"%c\" after %u ms", cmd, RESPONSE_TIMEOUT_MS);
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
        if (line.length() == 1 && isAsyncAck(line[0]))
        {
            diagnostics.lastAck = line;
            diagnostics.lastAckMs = diagnostics.lastResponseMs;
            diagnostics.lastError.reset();
            updateDiagnosticsState(RG15State::Acknowledged);
            if (debugUart)
                Logger::info(TAG, "RX async ack \"%s\"", line.c_str());
            return true;
        }
        if (!isStatusLine(line))
            return false;
        diagnostics.lastStatusLine = line;
        recordStatusLine(line, diagnostics);
        diagnostics.lastError.reset();
        if (debugUart)
            Logger::info(TAG, "RX status line \"%s\"", line.c_str());
        return true;
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
        updateDiagnosticsState(RG15State::CommandSent);
        return ok;
    }
} // namespace SQM
