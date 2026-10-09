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

        // The daily total reset (spec 003 FR-004). Each "reset day" starts at
        // the configured local HH:MM, so the reset fires on the first check at
        // or after that time - a restart, stall or DST jump over the exact
        // minute can't skip it - and only once per day.
        struct LocalTime
        {
            int year = 1970; // e.g. 2026
            int yearDay = 0; // 0-365, as tm_yday
            int hour = 0;
            int minute = 0;
        };

        constexpr int32_t NO_RESET_DAY = -1;

        // Days since 1970-01-01 of the reset day `now` falls in.
        int32_t resetDay(const LocalTime &now, uint8_t resetHour, uint8_t resetMinute);

        enum class ResetDecision : uint8_t
        {
            Wait,      // already done for this reset day (or the clock went backwards)
            Reset,     // send the reset now, then record the returned day
            Adopt,     // nothing recorded yet: record this day without resetting
        };

        // `lastResetDay` is the day last reset (or adopted), NO_RESET_DAY if
        // none is recorded - then the current day is adopted rather than
        // resetting, so a first boot or an upgrade never wipes today's total.
        ResetDecision dailyReset(const LocalTime &now, uint8_t resetHour, uint8_t resetMinute, int32_t lastResetDay);
    } // namespace Rain
} // namespace SQM
