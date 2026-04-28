#pragma once

#include <cstdint>
#include <cmath>

namespace SQM
{

    enum class CloudCondition
    {
        UNKNOWN = 0,
        CLEAR = 1,
        CLOUDY = 2,
        OVERCAST = 3
    };

    struct CloudMetrics
    {
        float temperatureDelta;   // Sky - Ambient temp difference (°C)
        float correctedDelta;     // Humidity-corrected delta (°C)
        float cloudCoverPercent;  // Estimated cloud cover (0-100%)
        CloudCondition condition; // Clear/Cloudy/Overcast classification
        const char *description;  // Human-readable description

        CloudMetrics()
            : temperatureDelta(0.0f),
              correctedDelta(0.0f),
              cloudCoverPercent(0.0f),
              condition(CloudCondition::UNKNOWN),
              description("Unknown") {}
    };

    class CloudDetection
    {
    public:
        /**
         * Calculate cloud conditions from IR temperature readings
         *
         * Modern approach using humidity correction instead of K constants.
         * Based on AAG CloudWatcher and modern meteorological models.
         *
         * @param skyTemp Sky temperature from IR sensor (°C)
         * @param ambientTemp Ambient temperature (°C)
         * @param relativeHumidity Relative humidity (0-100%), default 53%
         * @return CloudMetrics containing cloud cover estimation
         */
        static CloudMetrics calculate(
            float skyTemp,
            float ambientTemp,
            float relativeHumidity = 53.0f,
            float clearSkyThreshold = CLEAR_SKY_THRESHOLD,
            float cloudyThreshold = CLOUDY_THRESHOLD,
            float humidityCorrection = HUMIDITY_CORRECTION_FACTOR
        );

        /**
         * Apply humidity correction to temperature delta
         *
         * The AAG CloudWatcher formula accounts for humidity's effect on IR readings.
         * High humidity reduces the apparent sky temperature difference.
         *
         * Formula: corrected_delta = raw_delta - (k1/100) * humidity
         * Where k1 is empirically determined (~0.75 for typical conditions)
         *
         * @param tempDelta Raw temperature difference (sky - ambient)
         * @param humidity Relative humidity (0-100%)
         * @param correctionFactor k1 humidity correction factor
         * @return Humidity-corrected temperature delta
         */
        static float applyHumidityCorrection(float tempDelta, float humidity, float correctionFactor = HUMIDITY_CORRECTION_FACTOR);

        /**
         * Classify cloud condition from corrected temperature delta
         *
         * Conservative thresholds calibrated for zenith-pointing sensors:
         * - Clear: delta < -13°C (very cold sky, truly clear)
         * - Cloudy: -13°C ≤ delta < -3°C (partial cloud cover)
         * - Overcast: delta ≥ -3°C (heavy cloud cover)
         *
         * Errs on the side of caution for astronomical observations.
         *
         * @param correctedDelta Humidity-corrected temperature delta
         * @param clearThreshold corrected delta below which sky is clear
         * @param cloudyThreshold corrected delta above which sky is overcast
         * @return Cloud condition classification
         */
        static CloudCondition classifyCondition(float correctedDelta, float clearThreshold = CLEAR_SKY_THRESHOLD, float cloudyThreshold = CLOUDY_THRESHOLD);

        /**
         * Estimate cloud cover percentage
         *
         * Linear interpolation between clear and overcast thresholds.
         * More sophisticated than binary classification.
         *
         * @param correctedDelta Humidity-corrected temperature delta
         * @param clearThreshold corrected delta below which sky is clear
         * @param cloudyThreshold corrected delta above which sky is overcast
         * @return Estimated cloud cover (0-100%)
         */
        static float estimateCloudCover(float correctedDelta, float clearThreshold = CLEAR_SKY_THRESHOLD, float cloudyThreshold = CLOUDY_THRESHOLD);

        /**
         * Get human-readable description of cloud condition
         */
        static const char *getConditionDescription(CloudCondition condition);

    private:
        // Calibrated for zenith-pointing sensors (conservative for astronomy)
        // Based on real-world observations: -11.4°C ≈ 15% cloud cover
        static constexpr float HUMIDITY_CORRECTION_FACTOR = 0.75f; // k1
        static constexpr float CLEAR_SKY_THRESHOLD = -13.0f;       // °C (truly clear sky)
        static constexpr float CLOUDY_THRESHOLD = -3.0f;           // °C (overcast)
    };

} // namespace SQM
