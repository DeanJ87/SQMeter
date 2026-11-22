#include "RainTrust.h"

#include <cmath>

namespace SQM
{
    namespace Alpaca
    {

        RainClass classifyRain(const RainEvidence &in, const RainCorroboration &setting)
        {
            if (!in.raining)
                return RainClass::Dry;
            if (!setting.enabled)
                return RainClass::Unchecked;
            // Only a fresh, healthy, real number may overrule the rain sensor.
            if (!in.skyUsable || !std::isfinite(in.cloudCoverPercent))
                return RainClass::NoSkyReading;
            if (in.cloudCoverPercent >= setting.levelPercent)
                return RainClass::Cloud;
            if (in.latched)
                return RainClass::Held;
            return RainClass::DewSuspected;
        }

        RainClass RainTrustLatch::update(RainEvidence evidence, const RainCorroboration &setting)
        {
            evidence.latched = trusted;
            const RainClass rain = classifyRain(evidence, setting);
            trusted = rainTrusted(rain);
            return rain;
        }

    } // namespace Alpaca
} // namespace SQM
