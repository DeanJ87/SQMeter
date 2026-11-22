#pragma once

#include <cstdint>

namespace SQM
{
    namespace Alpaca
    {

        // Rain needs cloud (specs/036-rain-needs-cloud): whether the rain the
        // rain sensor reports counts for the safety verdict. Rain under a
        // fresh, healthy clear-sky reading is dew suspected; every other case
        // trusts rain (Constitution Principle I).
        enum class RainClass : uint8_t
        {
            Dry,          // no rain and no hold-off
            Unchecked,    // the setting is off: rain counts as before
            Cloud,        // cloud cover at or above the level confirms it
            NoSkyReading, // the IR sky sensor can't be trusted, so rain counts
            Held,         // trusted earlier and the rain sensor isn't dry yet
            DewSuspected, // rain under a healthy clear sky: not unsafe
        };

        // Everything but Dry and DewSuspected makes rain count.
        constexpr bool rainTrusted(RainClass rain)
        {
            return rain != RainClass::Dry && rain != RainClass::DewSuspected;
        }

        // What the classification reads, from one safety pass.
        struct RainEvidence
        {
            bool raining = false;   // the rain sensor's rain, including its clear-delay hold-off
            bool skyUsable = false; // IR sky sensor found, OK, fresh and its cloud cover computable
            float cloudCoverPercent = 0.0f;
            bool latched = false; // rain was trusted earlier in this wet spell
        };

        // The setting: off, or on with the cloud cover that confirms rain (at or above it).
        struct RainCorroboration
        {
            bool enabled = false;
            float levelPercent = 10.0f;
        };

        // Pure classification (no state), as the table in data-model.md.
        RainClass classifyRain(const RainEvidence &evidence, const RainCorroboration &setting);

        // Keeps trusted rain trusted until the rain sensor is dry and its
        // hold-off has ended (FR-007), so a gap in the cloud mid-shower can't
        // make the verdict safe. One per safety pass owner (firmware, demo).
        class RainTrustLatch
        {
        public:
            // Classifies this pass's rain (evidence.latched is taken from the
            // latch) and updates the latch from the result.
            RainClass update(RainEvidence evidence, const RainCorroboration &setting);
            bool latched() const { return trusted; }

        private:
            bool trusted = false;
        };

    } // namespace Alpaca
} // namespace SQM
