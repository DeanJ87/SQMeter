#pragma once

#include <cstdint>

namespace SQM
{
    namespace Astro
    {

        // Sun elevation above the horizon in degrees (negative = below), from
        // NOAA's solar position approximation - accurate to well under a
        // degree, plenty for "is it dark yet".
        double sunElevationDeg(int64_t unixSeconds, double latitudeDeg, double longitudeDeg);

        // Standard thresholds for the sun's centre.
        constexpr double SUNSET_DEG = -0.833;      // upper limb at the horizon, with refraction
        constexpr double NAUTICAL_DARK_DEG = -12.0;
        constexpr double ASTRONOMICAL_DARK_DEG = -18.0;

    } // namespace Astro
} // namespace SQM
