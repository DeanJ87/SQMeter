#pragma once

namespace SQM
{
    // Magnus formula (a = 17.27, b = 237.7 °C), valid for about -40..50 °C.
    // Returns 0 when humidity is outside (0, 100] or the result isn't finite.
    float dewpointMagnus(float temperatureC, float humidityPercent);

    // Humidity the cloud model assumes when there's no humidity sensor.
    constexpr float ASSUMED_HUMIDITY_PERCENT = 53.0f;
} // namespace SQM
