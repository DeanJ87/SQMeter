#pragma once

#include <cstddef>
#include <cstdint>

namespace SQM
{
    // The last safety changes, restarts and safety alerts, kept in RTC
    // memory so they survive a software restart, crash or OTA (not a power
    // cut). Answers "it went unsafe and I got no alert - why?".
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

        // Once, early in setup().
        void begin(uint8_t resetReason);
        void recordChange(bool safe, bool held, uint32_t flags);
        void recordAlert(bool safe);
        void recordArmed(bool armed);
        // Oldest first; returns how many were written.
        size_t entries(Entry *out, size_t max);
        uint16_t currentBoot();
        // The newest safe/unsafe alert sent, from any boot; false if none.
        bool lastAlert(bool &safe);
    } // namespace SafetyHistory
} // namespace SQM
