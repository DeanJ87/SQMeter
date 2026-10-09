#pragma once

#include <cstdint>

namespace SQM
{
    namespace Alerts
    {
        // When alerts go out (specs/021): any time unless paused, or only
        // while an imaging app has an Alpaca device connected. Pausing and
        // resuming work in both modes; the state says why and since when.
        enum class SendMode : uint8_t
        {
            Any = 0,
            WhileConnected = 1,
        };

        // Why alerts are being sent or paused. Persisted (as its number) with
        // the state, so keep the values stable.
        enum class ScheduleReason : uint8_t
        {
            None = 0,
            UserUi = 1,             // the web UI's Pause/Resume button
            UserRest = 2,           // POST /api/alerts/arm or /disarm
            UserMqtt = 3,           // <topic>/alerts/armed/set
            ClientConnected = 4,    // an imaging app connected (while-connected mode)
            ClientDisconnected = 5, // the last device was disconnected cleanly
            WaitingForClient = 6,   // while-connected mode, nothing connected yet
            Migrated = 7,           // paused before an update that added reasons
        };
        constexpr uint8_t SCHEDULE_REASON_MAX = 7;

        const char *scheduleReasonName(ScheduleReason reason); // "user-ui", "client-disconnected", ...
        const char *sendModeName(SendMode mode);               // "any", "whileConnected"

        struct ScheduleState
        {
            bool sending = true;
            ScheduleReason reason = ScheduleReason::None;
            bool sinceKnown = false; // changed during this boot (sinceMs valid)
            uint32_t sinceMs = 0;    // uptime
            int64_t sinceEpoch = 0;  // Unix seconds, 0 = clock not set
        };

        class AlertSchedule
        {
        public:
            // The state saved before a restart. `reasonSaved` is false for
            // state saved by firmware from before reasons existed: a pause
            // from then reads as "paused before the update".
            void restore(bool sending, bool reasonSaved, uint8_t reason, int64_t sinceEpoch);

            // Pause (send = false) or resume from the UI, REST or MQTT.
            // Returns true if the state (sending or reason) changed.
            bool command(bool send, ScheduleReason source, uint32_t nowMs, int64_t epoch);

            // Every pass: the setting and whether any Alpaca device is
            // connected. Silence doesn't change "connected", so it never
            // pauses alerts - only a clean disconnect does. Returns true if
            // the state (sending or reason) changed.
            bool update(SendMode mode, bool anyConnected, uint32_t nowMs, int64_t epoch);

            const ScheduleState &state() const { return current; }

        private:
            void set(bool sending, ScheduleReason reason, uint32_t nowMs, int64_t epoch);
            void modeChanged(SendMode mode, bool anyConnected, uint32_t nowMs, int64_t epoch);
            bool pausedByUser() const;

            ScheduleState current;
            bool started = false;
            SendMode lastMode = SendMode::Any;
            bool lastConnected = false;
        };

    } // namespace Alerts
} // namespace SQM
