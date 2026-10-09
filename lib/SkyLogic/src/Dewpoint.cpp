#include "calculations/Dewpoint.h"

#include <cmath>

namespace SQM
{
    float dewpointMagnus(float temperatureC, float humidityPercent)
    {
        // Magnus coefficients (Alduchov & Eskridge, over water).
        constexpr float MAGNUS_A = 17.27f;
        constexpr float MAGNUS_B = 237.7f;

        if (!(humidityPercent > 0.0f && humidityPercent <= 100.0f))
            return 0.0f;

        const float alpha = ((MAGNUS_A * temperatureC) / (MAGNUS_B + temperatureC)) + std::log(humidityPercent / 100.0f);
        const float dewpoint = (MAGNUS_B * alpha) / (MAGNUS_A - alpha);
        if (!std::isfinite(dewpoint))
            return 0.0f;
        return dewpoint;
    }
} // namespace SQM
