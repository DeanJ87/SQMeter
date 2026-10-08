#include "AlertSchedule.h"

namespace SQM
{
    namespace Alerts
    {
        const char *scheduleReasonName(ScheduleReason reason)
        {
            switch (reason)
            {
            case ScheduleReason::UserUi:
                return "user-ui";
            case ScheduleReason::UserRest:
                return "user-rest";
            case ScheduleReason::UserMqtt:
                return "user-mqtt";
            case ScheduleReason::ClientConnected:
                return "client-connected";
            case ScheduleReason::ClientDisconnected:
                return "client-disconnected";
            case ScheduleReason::WaitingForClient:
                return "waiting-for-client";
            case ScheduleReason::Migrated:
                return "migrated";
            case ScheduleReason::None:
                break;
            }
            return "none";
        }

        const char *sendModeName(SendMode mode)
        {
            return mode == SendMode::WhileConnected ? "whileConnected" : "any";
        }

        void AlertSchedule::restore(bool sending, bool reasonSaved, uint8_t reason, int64_t sinceEpoch)
        {
            current = ScheduleState{};
            current.sending = sending;
            if (!reasonSaved)
                current.reason = sending ? ScheduleReason::None : ScheduleReason::Migrated;
            else
                current.reason = reason <= SCHEDULE_REASON_MAX ? static_cast<ScheduleReason>(reason) : ScheduleReason::None;
            current.sinceEpoch = sinceEpoch;
        }

        void AlertSchedule::set(bool sending, ScheduleReason reason, uint32_t nowMs, int64_t epoch)
        {
            current.sending = sending;
            current.reason = reason;
            current.sinceKnown = true;
            current.sinceMs = nowMs;
            current.sinceEpoch = epoch;
        }

        bool AlertSchedule::command(bool send, ScheduleReason source, uint32_t nowMs, int64_t epoch)
        {
            if (send == current.sending && source == current.reason)
                return false;
            set(send, source, nowMs, epoch);
            return true;
        }

        bool AlertSchedule::update(SendMode mode, bool anyConnected, uint32_t nowMs, int64_t epoch)
        {
            if (!started)
            {
                // Whatever was saved stands; only changes from here on count.
                started = true;
                lastMode = mode;
                lastConnected = anyConnected;
                return false;
            }
            const ScheduleState before = current;
            if (mode != lastMode)
                modeChanged(mode, anyConnected, nowMs, epoch);
            else if (mode == SendMode::WhileConnected && anyConnected != lastConnected)
                set(anyConnected, anyConnected ? ScheduleReason::ClientConnected : ScheduleReason::ClientDisconnected, nowMs, epoch);
            lastMode = mode;
            lastConnected = anyConnected;
            return current.sending != before.sending || current.reason != before.reason;
        }

        void AlertSchedule::modeChanged(SendMode mode, bool anyConnected, uint32_t nowMs, int64_t epoch)
        {
            if (mode == SendMode::WhileConnected)
            {
                if (anyConnected)
                    set(true, ScheduleReason::ClientConnected, nowMs, epoch);
                else if (!pausedByUser())
                    set(false, ScheduleReason::WaitingForClient, nowMs, epoch);
                return;
            }
            // Back to "any time": a pause the old mode caused ends; a user's stays.
            if (!current.sending &&
                (current.reason == ScheduleReason::ClientDisconnected || current.reason == ScheduleReason::WaitingForClient))
                set(true, ScheduleReason::None, nowMs, epoch);
        }

        bool AlertSchedule::pausedByUser() const
        {
            return !current.sending && (current.reason == ScheduleReason::UserUi || current.reason == ScheduleReason::UserRest ||
                                        current.reason == ScheduleReason::UserMqtt);
        }

    } // namespace Alerts
} // namespace SQM
