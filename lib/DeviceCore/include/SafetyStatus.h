#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace SQM
{

    // Latest SafetyMonitor verdict, re-evaluated every second so the safe
    // delay, alerts and the dashboard all see one consistent state.
    struct SafetyStatus
    {
        bool isSafe = false;  // reported verdict (after the safe delay)
        bool rawSafe = false; // instantaneous rule evaluation
        uint32_t reasonFlags = 0;
        std::vector<std::string> reasons;
        uint32_t secondsUntilSafe = 0;
        uint32_t evaluatedAtMs = 0;
        uint32_t changedAtMs = 0; // when isSafe last changed
    };

} // namespace SQM
