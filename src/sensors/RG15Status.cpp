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

// RG-15 status: diagnostics and JSON, and the daily total reset (with the
// last reset day kept in NVS).

namespace SQM
{
    // The last reset day survives restarts (NVS), so a restart over the reset
    // time neither skips the day's reset nor repeats it.
    namespace
    {
        constexpr const char *RAIN_NVS_NAMESPACE = "rg15";
        constexpr const char *RESET_DAY_KEY = "resetDay";
    } // namespace

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

    const char *RG15Sensor::stateToString(RG15State state)
    {
        switch (state)
        {
        case RG15State::Disabled:
            return "disabled";
        case RG15State::Configured:
            return "configured";
        case RG15State::UartOpened:
            return "uart_opened";
        case RG15State::Configuring:
            return "configuring";
        case RG15State::CommandSent:
            return "command_sent";
        case RG15State::AwaitingResponse:
            return "awaiting_response";
        case RG15State::Acknowledged:
            return "acknowledged";
        case RG15State::ReadingReceived:
            return "reading_received";
        case RG15State::ParseError:
            return "parse_error";
        case RG15State::Timeout:
            return "timeout";
        case RG15State::Stale:
            return "stale";
        case RG15State::Online:
            return "online";
        default:
            return "unknown";
        }
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

        Rain::LocalTime local;
        local.year = localTime.tm_year + 1900;
        local.yearDay = localTime.tm_yday;
        local.hour = localTime.tm_hour;
        local.minute = localTime.tm_min;
        const int32_t day = Rain::resetDay(local, dailyResetHour, dailyResetMinute);
        const Rain::ResetDecision decision = Rain::dailyReset(local, dailyResetHour, dailyResetMinute, loadLastResetDay());
        if (decision == Rain::ResetDecision::Wait)
        {
            return;
        }
        // Recorded before sending: a failed send isn't retried every poll -
        // the RG-15's own total keeps counting until the next day's reset.
        saveLastResetDay(day);
        if (decision == Rain::ResetDecision::Adopt)
        {
            return;
        }

        if (debugUart)
        {
            Logger::info(TAG, "daily total reset: TX \"O\"");
        }
        if (sendCommand('O'))
        {
            diagnostics.lastTotalResetMs = now;
            reading.totalAcc = 0.0f;
        }
    }

    int32_t RG15Sensor::loadLastResetDay()
    {
        if (diagnostics.lastDailyResetDay != Rain::NO_RESET_DAY)
        {
            return diagnostics.lastDailyResetDay;
        }
        Preferences prefs;
        if (prefs.begin(RAIN_NVS_NAMESPACE, true))
        {
            diagnostics.lastDailyResetDay = prefs.getInt(RESET_DAY_KEY, Rain::NO_RESET_DAY);
            prefs.end();
        }
        return diagnostics.lastDailyResetDay;
    }

    void RG15Sensor::forgetLastResetDay()
    {
        diagnostics.lastDailyResetDay = Rain::NO_RESET_DAY;
        Preferences prefs;
        if (prefs.begin(RAIN_NVS_NAMESPACE, false))
        {
            prefs.remove(RESET_DAY_KEY);
            prefs.end();
        }
    }

    void RG15Sensor::saveLastResetDay(int32_t day)
    {
        diagnostics.lastDailyResetDay = day;
        Preferences prefs;
        if (!prefs.begin(RAIN_NVS_NAMESPACE, false))
        {
            Logger::warn(TAG, "Couldn't save the rain reset day");
            return;
        }
        prefs.putInt(RESET_DAY_KEY, day);
        prefs.end();
    }
} // namespace SQM
