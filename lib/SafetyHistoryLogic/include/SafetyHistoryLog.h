#pragma once

#include <cstddef>
#include <cstdint>

namespace SQM
{
    namespace SafetyHistory
    {
        enum class Kind : uint8_t
        {
            Boot = 0,   // device started; resetReason says why
            Change = 1, // reported verdict changed; flags = reasons
            Alert = 2,  // a safe/unsafe alert was sent
            Armed = 3,  // alerts switched on (safe=1) or off (safe=0)
        };

        struct Entry
        {
            uint32_t epoch;    // 0 if the clock wasn't set (filled in later where possible)
            uint32_t uptimeS;  // seconds since that boot
            uint32_t flags;    // UnsafeReasonFlag bits at the time
            uint16_t boot;     // which boot it belongs to
            Kind kind;
            uint8_t safe : 1;
            uint8_t held : 1;  // unsafe only because of the safe delay
            uint8_t resetReason : 6;
        };

        constexpr size_t CAPACITY = 32;
        constexpr uint32_t MAGIC = 0x53484931; // "SHI1"

        // The ring buffer itself - plain data so it can live in RTC memory,
        // which keeps whatever was there before a restart (garbage after a
        // power cut). The functions below are the logic, without locking.
        struct Log
        {
            uint32_t magic;
            uint32_t head; // next slot to write
            uint32_t count;
            uint16_t boot;
            Entry entries[CAPACITY];
        };

        // Starts a new boot: keeps the log if it's intact and the reset kept
        // RTC memory (`keep`), else clears it. Returns the new boot number.
        uint16_t startBoot(Log &log, bool keep);
        void push(Log &log, const Entry &entry);
        // Oldest first, the newest `max`; returns how many were written.
        size_t copy(const Log &log, Entry *out, size_t max);
        // The newest safe/unsafe alert sent, from any boot; false if none.
        bool lastAlert(const Log &log, bool &safe);
        // Dates entries from boot `boot` made before the clock was set, from
        // their uptime, now that it's `epochNow` at uptime `uptimeS`.
        void backfillEpochs(Entry *entries, size_t n, uint16_t boot, uint32_t epochNow, uint32_t uptimeS);
    } // namespace SafetyHistory
} // namespace SQM
