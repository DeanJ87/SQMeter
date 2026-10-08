#pragma once

#include <cstdint>
#include <cmath>

namespace SQM
{

    struct SkyQualityMetrics
    {
        float lux;    // Input illuminance in lux
        float sqm;    // Sky Quality Meter value (mag/arcsec²)
        float nelm;   // Naked Eye Limiting Magnitude
        float bortle; // Bortle Dark Sky Scale (1-9)

        SkyQualityMetrics() : lux(0.0f), sqm(0.0f), nelm(0.0f), bortle(0.0f) {}

        bool isValid() const
        {
            return lux >= 0.0f && sqm >= 0.0f && nelm >= 0.0f;
        }
    };

    class SkyQuality
    {
    public:
        /**
         * Calculate sky quality metrics from lux measurement
         *
         * @param lux Illuminance in lux from light sensor
         * @return SkyQualityMetrics containing SQM, NELM, and Bortle scale
         */
        static SkyQualityMetrics calculate(float lux);

        /**
         * Convert lux to SQM (magnitudes per square arcsecond)
         * Formula: MPSAS = 12.6 - 2.5 * log10(lux)
         * Higher values = darker sky
         *
         * @param lux Illuminance in lux
         * @return SQM value in mag/arcsec²
         */
        static float luxToSQM(float lux);

        /**
         * Convert SQM to NELM (Naked Eye Limiting Magnitude)
         * Unihedron formula: NELM = 7.93 - 5*log10(10^(4.316-(SQM/5))+1)
         *
         * @param sqm Sky Quality Meter value
         * @return Estimated naked eye limiting magnitude
         */
        static float sqmToNELM(float sqm);

        /**
         * Convert SQM to Bortle Dark Sky Scale (1-9)
         * 1 = Excellent dark site, 9 = Inner city
         *
         * @param sqm Sky Quality Meter value
         * @return Bortle scale value (1-9)
         */
        static float sqmToBortle(float sqm);

        /**
         * Get human-readable description of sky quality
         *
         * @param bortle Bortle scale value
         * @return Description string
         */
        static const char *getBortleDescription(float bortle);

    private:
        // Constants for SQM calculation
        static constexpr float LUX_REFERENCE = 108000.0f;
        static constexpr float MAG_FACTOR = -0.4f;

        // Minimum valid lux value (to avoid log(0))
        static constexpr float MIN_LUX = 0.0001f;
    };

} // namespace SQM
