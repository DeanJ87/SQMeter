#include "calculations/CloudDetection.h"
#include <algorithm>

namespace SQM
{

    CloudMetrics CloudDetection::calculate(float skyTemp, float ambientTemp, float relativeHumidity,
                                            float clearSkyThreshold, float cloudyThreshold, float humidityCorrection)
    {
        CloudMetrics metrics;

        // Calculate raw temperature delta (sky is typically colder than ambient)
        metrics.temperatureDelta = skyTemp - ambientTemp;

        // Apply humidity correction
        // High humidity makes the sky appear warmer, reducing the delta
        metrics.correctedDelta = applyHumidityCorrection(metrics.temperatureDelta, relativeHumidity, humidityCorrection);

        // Estimate cloud cover percentage
        metrics.cloudCoverPercent = estimateCloudCover(metrics.correctedDelta, clearSkyThreshold, cloudyThreshold);

        // Classify condition
        metrics.condition = classifyCondition(metrics.correctedDelta, clearSkyThreshold, cloudyThreshold);
        metrics.description = getConditionDescription(metrics.condition);

        return metrics;
    }

    float CloudDetection::applyHumidityCorrection(float tempDelta, float humidity, float correctionFactor)
    {
        // Clamp humidity to valid range
        humidity = std::max(0.0f, std::min(100.0f, humidity));

        // AAG CloudWatcher correction formula
        // Humidity increases the apparent sky temperature, reducing the delta
        float correction = (correctionFactor / 100.0f) * humidity;

        return tempDelta - correction;
    }

    CloudCondition CloudDetection::classifyCondition(float correctedDelta, float clearThreshold, float cloudyThreshold)
    {
        // Conservative thresholds for astronomical observations (zenith sensor)
        // Errs on the side of caution - better to overestimate cloud cover
        if (correctedDelta < clearThreshold)
        {
            return CloudCondition::CLEAR;
        }
        else if (correctedDelta < cloudyThreshold)
        {
            return CloudCondition::CLOUDY;
        }
        else
        {
            return CloudCondition::OVERCAST;
        }
    }

    float CloudDetection::estimateCloudCover(float correctedDelta, float clearThreshold, float cloudyThreshold)
    {
        // Linear interpolation between clear and overcast thresholds

        if (correctedDelta < clearThreshold)
        {
            return 0.0f; // Truly clear
        }
        else if (correctedDelta >= cloudyThreshold)
        {
            return 100.0f; // Overcast
        }
        else
        {
            // Linear interpolation in the cloudy range
            float range = cloudyThreshold - clearThreshold;
            float position = correctedDelta - clearThreshold;
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
