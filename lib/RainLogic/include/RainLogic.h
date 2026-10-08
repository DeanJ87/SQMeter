#pragma once

#include <cstdint>
#include <string>

namespace SQM
{
    // Hydreon RG-15 polled-reading parsing and the rain latch - the rain input
    // to the safety verdict - kept free of Arduino so they're unit-tested.
    namespace Rain
    {
        // "Acc 0.01 mm, EventAcc 0.20 mm, TotalAcc 12.60 mm, RInt 0.47 mmph i"
        struct Line
        {
            float acc = 0.0f;      // since the previous reading
            float eventAcc = 0.0f; // the RG-15's own event total
            float totalAcc = 0.0f;
            float rInt = 0.0f;     // intensity per hour
            bool imperial = false; // "iph" rather than "mmph"
            bool lensBad = false;  // flag "i" / "LensBad"
            bool emSat = false;    // flag "o" / "EmSat"
        };

        enum class ParseResult : uint8_t
        {
            Ok,
            TooShort,     // under 20 characters - a fragment
            MissingField, // Acc, EventAcc, TotalAcc or RInt absent or not a number
            OutOfRange,   // negative or implausibly large
        };

        ParseResult parseLine(const std::string &line, Line &out);

        // Raining is held ("latched") for `clearDelayMs` after the last reading
        // that saw rain, so a roof doesn't re-open between showers.
        struct Latch
        {
            bool latched = false;
            float eventAccumulation = 0.0f; // sum of Acc while latched
            uint32_t lastRainMs = 0;        // 0 = never
        };

        // Feed one successful reading taken at `now`.
        void observe(Latch &latch, float rInt, float acc, uint32_t now, uint32_t clearDelayMs);
        // Release the latch once the clear delay has passed with no rain
        // (called between readings too, e.g. while the sensor is silent).
        void expire(Latch &latch, uint32_t now, uint32_t clearDelayMs);
    } // namespace Rain
} // namespace SQM
