#include "BleAlarm.h"

#include <cstddef>

namespace SQM
{
    namespace Ble
    {

        namespace
        {
            void putU32(std::string &out, uint32_t value)
            {
                for (int shift = 0; shift < 32; shift += 8)
                    out += static_cast<char>((value >> shift) & 0xFF);
            }
        }

        bool AlarmTracker::raiseAlarm(uint32_t reasonFlags, uint32_t epoch, uint32_t nowMs)
        {
            if (level == ALARM_ACTIVE)
            {
                // Same alarm, fresher detail: no new sequence number, so an ack
                // for it still counts.
                reasons |= reasonFlags;
                return false;
            }
            ++seq;
            level = ALARM_ACTIVE;
            reasons = reasonFlags;
            epochSeconds = epoch;
            lastSentMs = nowMs;
            return true;
        }

        bool AlarmTracker::raiseInfo(uint32_t reasonFlags, uint32_t epoch, uint32_t nowMs)
        {
            if (level == ALARM_ACTIVE)
                return false;
            ++seq;
            level = ALARM_INFO;
            reasons = reasonFlags;
            epochSeconds = epoch;
            lastSentMs = nowMs;
            return true;
        }

        bool AlarmTracker::acknowledge(uint32_t ackSeq, uint32_t nowMs)
        {
            if (level != ALARM_ACTIVE || ackSeq != seq)
                return false;
            level = ALARM_NONE;
            ackedSeq = ackSeq;
            lastSentMs = nowMs;
            return true;
        }

        bool AlarmTracker::resendDue(uint32_t nowMs) const
        {
            return level == ALARM_ACTIVE && nowMs - lastSentMs >= RESEND_INTERVAL_MS;
        }

        std::string AlarmTracker::encode() const
        {
            std::string out;
            putU32(out, seq);
            out += static_cast<char>(level);
            putU32(out, reasons);
            putU32(out, epochSeconds);
            return out;
        }

        std::string encodeHeartbeat(uint32_t seq, uint32_t uptimeSeconds)
        {
            std::string out;
            putU32(out, seq);
            putU32(out, uptimeSeconds);
            return out;
        }

        bool decodeAck(const uint8_t *data, size_t length, uint32_t &seq)
        {
            if (data == nullptr || length != 4)
                return false;
            seq = static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
                  (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
            return true;
        }

        bool parsePasskey(const std::string &text, uint32_t &passkey)
        {
            if (text.size() != 6)
                return false;
            uint32_t value = 0;
            for (char c : text)
            {
                if (c < '0' || c > '9')
                    return false;
                value = value * 10 + static_cast<uint32_t>(c - '0');
            }
            if (value == 0)
                return false;
            passkey = value;
            return true;
        }

    } // namespace Ble
} // namespace SQM
