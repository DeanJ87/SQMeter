#pragma once

#include <cstdint>
#include <string>

namespace SQM
{
    namespace Ble
    {

        constexpr const char *ALARM_CHAR_UUID = "c5a10005-7d1e-4b8a-9f3c-2e5d6a7b8c90";
        constexpr const char *ACK_CHAR_UUID = "c5a10006-7d1e-4b8a-9f3c-2e5d6a7b8c90";
        constexpr const char *HEARTBEAT_CHAR_UUID = "c5a10007-7d1e-4b8a-9f3c-2e5d6a7b8c90";

        enum AlarmLevel : uint8_t
        {
            ALARM_NONE = 0,  // nothing active (or the last alarm was acknowledged)
            ALARM_INFO = 1,  // something worth showing, not worth waking anyone for
            ALARM_ACTIVE = 2 // wake someone up; repeats until acknowledged
        };

        // Phone alarm state for the BLE alarm characteristic. An ACTIVE alarm
        // stays active - and keeps being re-sent - until a phone acknowledges
        // its sequence number, even if the condition clears meanwhile: if it
        // rained with the roof open, someone should still know. Pure logic,
        // no BLE, so it's unit-testable.
        class AlarmTracker
        {
        public:
            static constexpr uint32_t RESEND_INTERVAL_MS = 30000;

            // Raise a new ACTIVE alarm (or refresh the reasons of the current one).
            // Returns true when the characteristic should be sent now.
            bool raiseAlarm(uint32_t reasonFlags, uint32_t epoch, uint32_t nowMs);

            // Informational event. Ignored while an alarm is active so it
            // can't overwrite (and silently cancel) the alarm on the phone.
            bool raiseInfo(uint32_t reasonFlags, uint32_t epoch, uint32_t nowMs);

            // Acknowledge by sequence number. Stale or unknown numbers are
            // rejected so a late ack can't cancel a newer alarm.
            bool acknowledge(uint32_t seq, uint32_t nowMs);

            // True when an unacknowledged alarm is due to be re-sent.
            bool resendDue(uint32_t nowMs) const;
            void markSent(uint32_t nowMs) { lastSentMs = nowMs; }

            bool active() const { return level == ALARM_ACTIVE; }
            uint32_t sequence() const { return seq; }
            AlarmLevel currentLevel() const { return level; }
            uint32_t acknowledgedSeq() const { return ackedSeq; }

            // [seq u32][level u8][reasonFlags u32][epoch u32] (13 bytes, little-endian)
            std::string encode() const;

        private:
            uint32_t seq = 0;
            AlarmLevel level = ALARM_NONE;
            uint32_t reasons = 0;
            uint32_t epochSeconds = 0;
            uint32_t lastSentMs = 0;
            uint32_t ackedSeq = 0;
        };

        // Heartbeat characteristic: [seq u32][uptime seconds u32] (8 bytes)
        std::string encodeHeartbeat(uint32_t seq, uint32_t uptimeSeconds);

        // Ack characteristic payload: [seq u32]. Returns false for anything else.
        bool decodeAck(const uint8_t *data, size_t length, uint32_t &seq);

        // Pairing passkey: exactly 6 digits, "000000" excluded. Empty = unset.
        bool parsePasskey(const std::string &text, uint32_t &passkey);

    } // namespace Ble
} // namespace SQM
