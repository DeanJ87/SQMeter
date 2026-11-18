#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "SafetyHistoryLog.h"

namespace SQM
{
    // The last safety changes, restarts and safety alerts, kept in RTC
    // memory so they survive a software restart, crash or OTA (not a power
    // cut). Answers "it went unsafe and I got no alert - why?".
    namespace SafetyHistory
    {
        // Once, early in setup().
        void begin(uint8_t resetReason);
        // holdLabels: the external safety holds' labels (spec 035).
        void recordChange(bool safe, bool held, uint32_t flags, const std::vector<std::string> &holdLabels);
        // A hold set or cleared, described by Core::holdSetEntry/holdClearedEntry; stamped here.
        void recordHold(const Entry &described);
        void recordAlert(bool safe);
        void recordArmed(bool armed);
        // Oldest first; returns how many were written.
        size_t entries(Entry *out, size_t max);
        uint16_t currentBoot();
        // The newest safe/unsafe alert sent, from any boot; false if none.
        bool lastAlert(bool &safe);
    } // namespace SafetyHistory
} // namespace SQM
