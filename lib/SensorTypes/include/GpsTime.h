#pragma once

// GPS date and time to Unix seconds, with the week-number rollover undone.
// No hardware headers: shared by the GPS driver and the native tests.

#include <cstdint>

namespace SQM
{
    namespace GpsTime
    {
        // GPS counts weeks in 10 bits, so its dates repeat every 1024 weeks.
        // Older receivers (the NEO-6M among them) pick the wrong 1024-week
        // span and report the right time of day on a date 19.6 years early.
        constexpr int64_t ROLLOVER_SECONDS = 1024LL * 7 * 86400;
        // No real fix is older than this (2024-01-01); earlier dates are rolled forward.
        constexpr int64_t EARLIEST_EPOCH = 1704067200;

        // Seconds since 1970 for a UTC calendar date (month 1-12, day 1-31).
        inline int64_t utcEpoch(int year, int month, int day, int hour, int minute, int second)
        {
            year -= month <= 2;
            const int64_t era = (year >= 0 ? year : year - 399) / 400;
            const int64_t yoe = year - era * 400;
            const int64_t doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
            const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
            const int64_t days = era * 146097 + doe - 719468;
            return days * 86400 + hour * 3600 + minute * 60 + second;
        }

        // The epoch moved forward in 1024-week steps until it isn't before EARLIEST_EPOCH.
        inline int64_t undoRollover(int64_t epoch)
        {
            while (epoch < EARLIEST_EPOCH)
                epoch += ROLLOVER_SECONDS;
            return epoch;
        }
    } // namespace GpsTime
} // namespace SQM
