#include "calculations/CloudDetection.h"
#include <algorithm>

namespace SQM
{

    CloudMetrics CloudDetection::calculate(float skyTemp, float ambientTemp, float relativeHumidity)
    {
        CloudMetrics metrics;

        // Calculate raw temperature delta (sky is typically colder than ambient)
        metrics.temperatureDelta = skyTemp - ambientTemp;

        // Apply humidity correction
        // High humidity makes the sky appear warmer, reducing the delta
        metrics.correctedDelta = applyHumidityCorrection(metrics.temperatureDelta, relativeHumidity);

        // Estimate cloud cover percentage
        metrics.cloudCoverPercent = estimateCloudCover(metrics.correctedDelta);

        // Classify condition
        metrics.condition = classifyCondition(metrics.correctedDelta);
        metrics.description = getConditionDescription(metrics.condition);

        return metrics;
    }

    float CloudDetection::applyHumidityCorrection(float tempDelta, float humidity)
    {
        // Clamp humidity to valid range
        humidity = std::max(0.0f, std::min(100.0f, humidity));

        // AAG CloudWatcher correction formula
        // Humidity increases the apparent sky temperature, reducing the delta
        float correction = (HUMIDITY_CORRECTION_FACTOR / 100.0f) * humidity;

        return tempDelta - correction;
    }

    CloudCondition CloudDetection::classifyCondition(float correctedDelta)
    {
        // Conservative thresholds for astronomical observations (zenith sensor)
        // Errs on the side of caution - better to overestimate cloud cover
        if (correctedDelta < CLEAR_SKY_THRESHOLD)
        {
            return CloudCondition::CLEAR;
        }
        else if (correctedDelta < CLOUDY_THRESHOLD)
        {
            return CloudCondition::CLOUDY;
        }
        else
        {
            return CloudCondition::OVERCAST;
        }
    }

    float CloudDetection::estimateCloudCover(float correctedDelta)
    {
        // Linear interpolation between clear and overcast
        // Conservative thresholds for zenith-pointing sensors:
        // Clear sky (< -13°C) = 0%
        // Overcast (≥ -3°C) = 100%
        // 10°C range for fine-grained detection

        if (correctedDelta < CLEAR_SKY_THRESHOLD)
        {
            return 0.0f; // Truly clear
        }
        else if (correctedDelta >= CLOUDY_THRESHOLD)
        {
            return 100.0f; // Overcast
        }
        else
        {
            // Linear interpolation in the cloudy range
            float range = CLOUDY_THRESHOLD - CLEAR_SKY_THRESHOLD; // 10°C range
            float position = correctedDelta - CLEAR_SKY_THRESHOLD;
            float percent = (position / range) * 100.0f;

            return std::max(0.0f, std::min(100.0f, percent));
        }
    }

    const char *CloudDetection::getConditionDescription(CloudCondition condition)
    {
        switch (condition)
        {
        case CloudCondition::CLEAR:
            return "Clear";
        case CloudCondition::CLOUDY:
            return "Cloudy";
        case CloudCondition::OVERCAST:
            return "Overcast";
        default:
            return "Unknown";
        }
    }

} // namespace SQM
