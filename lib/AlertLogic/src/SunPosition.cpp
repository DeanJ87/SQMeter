#include "SunPosition.h"

#include <cmath>

namespace SQM
{
    namespace Astro
    {

        namespace
        {
            constexpr double PI = 3.14159265358979323846;
            constexpr double DEG = PI / 180.0;
        }

        double sunElevationDeg(int64_t unixSeconds, double latitudeDeg, double longitudeDeg)
        {
            // Julian century since J2000.0
            const double julianDay = static_cast<double>(unixSeconds) / 86400.0 + 2440587.5;
            const double t = (julianDay - 2451545.0) / 36525.0;

            const double meanLongitude = std::fmod(280.46646 + t * (36000.76983 + t * 0.0003032), 360.0);
            const double meanAnomaly = 357.52911 + t * (35999.05029 - 0.0001537 * t);
            const double eccentricity = 0.016708634 - t * (0.000042037 + 0.0000001267 * t);
            const double center = std::sin(meanAnomaly * DEG) * (1.914602 - t * (0.004817 + 0.000014 * t)) +
                                  std::sin(2 * meanAnomaly * DEG) * (0.019993 - 0.000101 * t) +
                                  std::sin(3 * meanAnomaly * DEG) * 0.000289;
            const double trueLongitude = meanLongitude + center;
            const double omega = 125.04 - 1934.136 * t;
            const double apparentLongitude = trueLongitude - 0.00569 - 0.00478 * std::sin(omega * DEG);
            const double meanObliquity = 23.0 + (26.0 + (21.448 - t * (46.815 + t * (0.00059 - t * 0.001813))) / 60.0) / 60.0;
            const double obliquity = meanObliquity + 0.00256 * std::cos(omega * DEG);
            const double declination = std::asin(std::sin(obliquity * DEG) * std::sin(apparentLongitude * DEG));

            const double y = std::tan(obliquity * DEG / 2) * std::tan(obliquity * DEG / 2);
            const double equationOfTimeMinutes =
                4.0 / DEG *
                (y * std::sin(2 * meanLongitude * DEG) - 2 * eccentricity * std::sin(meanAnomaly * DEG) +
                 4 * eccentricity * y * std::sin(meanAnomaly * DEG) * std::cos(2 * meanLongitude * DEG) -
                 0.5 * y * y * std::sin(4 * meanLongitude * DEG) - 1.25 * eccentricity * eccentricity * std::sin(2 * meanAnomaly * DEG));

            const double minutesUtc = std::fmod(static_cast<double>(unixSeconds), 86400.0) / 60.0;
            double trueSolarTime = std::fmod(minutesUtc + equationOfTimeMinutes + 4.0 * longitudeDeg, 1440.0);
            if (trueSolarTime < 0)
                trueSolarTime += 1440.0;
            const double hourAngle = trueSolarTime / 4.0 - 180.0;

            const double cosZenith = std::sin(latitudeDeg * DEG) * std::sin(declination) +
                                     std::cos(latitudeDeg * DEG) * std::cos(declination) * std::cos(hourAngle * DEG);
            const double zenith = std::acos(std::fmax(-1.0, std::fmin(1.0, cosZenith)));
            return 90.0 - zenith / DEG;
        }

    } // namespace Astro
} // namespace SQM
