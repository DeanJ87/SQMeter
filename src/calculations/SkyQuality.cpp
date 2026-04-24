#include "calculations/SkyQuality.h"
#include <algorithm>

namespace SQM
{

    SkyQualityMetrics SkyQuality::calculate(float lux)
    {
        SkyQualityMetrics metrics;

        // Ensure minimum lux value
        metrics.lux = std::max(lux, MIN_LUX);

        // Calculate SQM
        metrics.sqm = luxToSQM(metrics.lux);

        // Calculate NELM from SQM
        metrics.nelm = sqmToNELM(metrics.sqm);

        // Calculate Bortle scale
        metrics.bortle = sqmToBortle(metrics.sqm);

        return metrics;
    }

    float SkyQuality::luxToSQM(float lux)
    {
        if (lux < MIN_LUX)
        {
            lux = MIN_LUX;
        }

        // Standard photometric conversion from lux to magnitudes per square arcsecond
        // MPSAS = 12.6 - 2.5 * log10(lux)
        // This gives positive values where higher = darker sky
        return 12.6f - 2.5f * log10f(lux);
    }

    float SkyQuality::sqmToNELM(float sqm)
    {
        // Unihedron formula:
        // NELM = 7.93 - 5 * log10(10^(4.316 - (SQM/5)) + 1)

        // For very bright conditions (indoors, city lights), return 0
        // since you can't see any stars anyway
        if (sqm < 15.0f)
        {
            return 0.0f;
        }

        float exponent = 4.316f - (sqm / 5.0f);
        float powerTerm = powf(10.0f, exponent);
        float logTerm = log10f(powerTerm + 1.0f);

        float nelm = 7.93f - 5.0f * logTerm;

        // Clamp to reasonable range (can't see negative magnitude stars with naked eye)
        return std::max(0.0f, nelm);
    }

    float SkyQuality::sqmToBortle(float sqm)
    {
        // Convert SQM to Bortle Dark Sky Scale
        // Based on typical SQM values for each Bortle class

        if (sqm >= 21.99f)
            return 1.0f; // Excellent dark site
        else if (sqm >= 21.89f)
            return 2.0f; // Typical dark site
        else if (sqm >= 21.69f)
            return 3.0f; // Rural sky
        else if (sqm >= 20.49f)
            return 4.0f; // Rural/suburban transition
        else if (sqm >= 19.50f)
            return 5.0f; // Suburban sky
        else if (sqm >= 18.94f)
            return 6.0f; // Bright suburban
        else if (sqm >= 18.38f)
            return 7.0f; // Suburban/urban transition
        else if (sqm >= 17.00f)
            return 8.0f; // City sky
        else
            return 9.0f; // Inner city
    }

    const char *SkyQuality::getBortleDescription(float bortle)
    {
        int bortleClass = static_cast<int>(bortle + 0.5f); // Round to nearest

        switch (bortleClass)
        {
        case 1:
            return "Excellent dark-sky site";
        case 2:
            return "Typical truly dark site";
        case 3:
            return "Rural sky";
        case 4:
            return "Rural/suburban transition";
        case 5:
            return "Suburban sky";
        case 6:
            return "Bright suburban sky";
        case 7:
            return "Suburban/urban transition";
        case 8:
            return "City sky";
        case 9:
            return "Inner-city sky";
        default:
            return "Unknown";
        }
    }

} // namespace SQM
