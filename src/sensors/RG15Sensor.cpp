#include "sensors/RG15Sensor.h"
#include "Logger.h"
#include <ArduinoJson.h>
#include <cstdio>

namespace SQM
{

    RG15Sensor::RG15Sensor(uint8_t rxPin, uint8_t txPin, uint32_t baudRate,
                           const std::string &mode, const std::string &resolution,
                           const std::string &units)
        : rxPin(rxPin), txPin(txPin), baudRate(baudRate),
          mode(mode), resolution(resolution), units(units),
          serial(std::make_unique<HardwareSerial>(UART_NUM))
    {
    }

    bool RG15Sensor::begin()
    {
        Logger::info(TAG, "Initializing RG-15 (RX:%d TX:%d Baud:%d mode:%s res:%s units:%s)",
                     rxPin, txPin, baudRate, mode.c_str(), resolution.c_str(), units.c_str());

        serial->begin(baudRate, SERIAL_8N1, rxPin, txPin);

        if (!serial)
        {
            Logger::error(TAG, "Failed to initialize UART");
            initialized = false;
            return false;
        }

        initialized = true;

        // Give sensor time to wake up then apply configured mode
        delay(200);
        applyConfig();

        reading.status = SensorStatus::OK;
        Logger::info(TAG, "RG-15 initialized");
        return true;
    }

    void RG15Sensor::applyConfig()
    {
        // Set units
        if (units == "metric")
        {
            sendCommand('M');
        }
        else if (units == "imperial")
        {
            sendCommand('I');
        }
        // "switch" = leave as DIP switch, no command

        // Set resolution
        if (resolution == "high")
        {
            sendCommand('H');
        }
        else if (resolution == "low")
        {
            sendCommand('L');
        }
        // "switch" = leave as DIP switch, no command

        // Drain any acknowledgment bytes silently
        delay(200);
        while (serial->available())
        {
            serial->read();
        }
    }

    void RG15Sensor::sendCommand(char cmd)
    {
        serial->write(cmd);
        serial->write('\n');
        serial->flush();
        Logger::debug(TAG, "Sent command '%c'", cmd);
    }

    void RG15Sensor::update()
    {
        if (!initialized)
        {
            reading.status = SensorStatus::NOT_INITIALIZED;
            return;
        }

        bool gotReading = false;

        if (mode == "polling")
        {
            gotReading = pollReading();
        }
        else
        {
            gotReading = drainBuffer();
        }

        if (!gotReading)
        {
            // Check for stale data
            if (reading.status == SensorStatus::OK &&
                (millis() - lastUpdateTime) > STALE_TIMEOUT_MS)
            {
                Logger::warn(TAG, "No data for %d ms, marking stale", STALE_TIMEOUT_MS);
                reading.status = SensorStatus::TIMEOUT;
            }
            else if (reading.status != SensorStatus::OK)
            {
                reading.status = SensorStatus::READ_ERROR;
            }
        }
    }

    bool RG15Sensor::pollReading()
    {
        // Drain any stale bytes before sending poll command
        while (serial->available())
        {
            serial->read();
        }

        sendCommand('R');

        std::string line;
        if (!readLine(line))
        {
            Logger::debug(TAG, "No response to poll");
            return false;
        }

        return parseLine(line);
    }

    bool RG15Sensor::drainBuffer()
    {
        bool gotAny = false;
        std::string line;

        // Parse all complete lines in the buffer, keep the last valid one
        while (serial->available())
        {
            if (readLine(line))
            {
                if (parseLine(line))
                {
                    gotAny = true;
                }
            }
        }

        return gotAny;
    }

    bool RG15Sensor::readLine(std::string &line)
    {
        line.clear();
        line.reserve(LINE_BUFFER_SIZE);

        const uint32_t deadline = millis() + RESPONSE_TIMEOUT_MS;

        while (millis() < deadline)
        {
            while (serial->available())
            {
                char c = static_cast<char>(serial->read());
                if (c == '\n')
                {
                    // Strip trailing CR
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
        }

        return false;
    }

    bool RG15Sensor::parseLine(const std::string &line)
    {
        // Expected format: "Acc  X.XX, EventAcc  X.XX, TotalAcc  X.XX, RInt  X.XX mm/h"
        if (line.length() < 40)
        {
            Logger::debug(TAG, "Line too short to parse: '%s'", line.c_str());
            return false;
        }

        float acc = 0.0f, eventAcc = 0.0f, totalAcc = 0.0f, rInt = 0.0f;

        int n = sscanf(line.c_str(),
                       "Acc %f, EventAcc %f, TotalAcc %f, RInt %f",
                       &acc, &eventAcc, &totalAcc, &rInt);

        if (n != 4)
        {
            Logger::debug(TAG, "Parse failed (matched %d/4 fields): '%s'", n, line.c_str());
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

        // Check for optional flag characters after the unit suffix
        size_t unitPos = line.find("mm");
        if (unitPos == std::string::npos)
        {
            unitPos = line.find(" in");
        }

        reading.lensBad = false;
        reading.emSat = false;

        if (unitPos != std::string::npos)
        {
            // Flags appear after the unit token (e.g. "mm/h i" or "mm/h o")
            const size_t flagsStart = line.find(' ', unitPos + 2);
            if (flagsStart != std::string::npos && flagsStart < line.length())
            {
                const std::string flags = line.substr(flagsStart);
                reading.lensBad = (flags.find('i') != std::string::npos);
                reading.emSat = (flags.find('o') != std::string::npos);
            }
        }

        reading.timestamp = millis();
        reading.status = SensorStatus::OK;
        lastUpdateTime = reading.timestamp;

        Logger::debug(TAG, "RG-15: acc=%.3f event=%.3f total=%.3f rInt=%.3f raining=%d",
                      acc, eventAcc, totalAcc, rInt, reading.isRaining);
        return true;
    }

    std::string RG15Sensor::toJson() const
    {
        StaticJsonDocument<256> doc;

        doc["sensor"] = "RG15";
        doc["timestamp"] = reading.timestamp;
        doc["status"] = static_cast<int>(reading.status);
        doc["isRaining"] = reading.isRaining;
        doc["acc"] = reading.acc;
        doc["eventAcc"] = reading.eventAcc;
        doc["totalAcc"] = reading.totalAcc;
        doc["rInt"] = reading.rInt;
        doc["lensBad"] = reading.lensBad;
        doc["emSat"] = reading.emSat;

        std::string output;
        serializeJson(doc, output);
        return output;
    }

    void RG15Sensor::stop()
    {
        if (initialized)
        {
            serial->end();
            initialized = false;
            reading = RG15Reading{};
            Logger::info(TAG, "RG-15 stopped");
        }
    }

    void RG15Sensor::reconfigure(uint8_t newRxPin, uint8_t newTxPin, uint32_t newBaudRate,
                                 const std::string &newMode, const std::string &newResolution,
                                 const std::string &newUnits)
    {
        stop();

        rxPin = newRxPin;
        txPin = newTxPin;
        baudRate = newBaudRate;
        mode = newMode;
        resolution = newResolution;
        units = newUnits;

        begin();
    }

} // namespace SQM
