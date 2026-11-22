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
        bool isSafe = false;  // reported verdict (after the safe and unsafe delays)
        bool rawSafe = false; // instantaneous rule evaluation
        uint32_t reasonFlags = 0;
        std::vector<std::string> reasons;
        uint32_t secondsUntilSafe = 0;
        uint32_t secondsUntilUnsafe = 0; // unsafe delay running: raw unsafe, reported safe
        uint32_t evaluatedAtMs = 0;
        uint32_t changedAtMs = 0; // when isSafe last changed
        // Rain needs cloud (spec 036): rain under a healthy clear sky. Not an
        // unsafe reason; the rate and cloud cover of the latest pass.
        bool dewSuspected = false;
        float dewRainRateMmPerHour = 0.0f;
        float dewCloudCoverPercent = 0.0f;
        uint32_t dewSinceMs = 0; // when dew was first suspected in this spell
    };

} // namespace SQM
