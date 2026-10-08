#include "calculations/Dewpoint.h"

#include <cmath>

namespace SQM
{
    float dewpointMagnus(float temperatureC, float humidityPercent)
    {
        constexpr float a = 17.27f;
        constexpr float b = 237.7f;

        if (!(humidityPercent > 0.0f && humidityPercent <= 100.0f))
            return 0.0f;

        const float alpha = ((a * temperatureC) / (b + temperatureC)) + std::log(humidityPercent / 100.0f);
        const float dewpoint = (b * alpha) / (a - alpha);
        if (!std::isfinite(dewpoint))
            return 0.0f;
        return dewpoint;
    }
} // namespace SQM
