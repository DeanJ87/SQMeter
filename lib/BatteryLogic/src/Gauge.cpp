#include "BatteryLogic.h"

#include <cmath>

// MAX17048 register decoding and the estimating window (research D1, D2, D4).

namespace SQM
{
    namespace Battery
    {
        float voltageFromVcell(uint16_t vcell)
        {
            return static_cast<float>(vcell) * 78.125e-6f;
        }

        float percentFromSoc(uint16_t soc)
        {
            const float percent = static_cast<float>(soc) / 256.0f;
            return percent > 100.0f ? 100.0f : percent;
        }

        float rateFromCrate(int16_t crate)
        {
            return static_cast<float>(crate) * 0.208f;
        }

        bool isMax17048(uint16_t version)
        {
            return (version & 0xFFF0u) == 0x0010u;
        }

        bool resetIndicated(uint16_t status)
        {
            return (status & STATUS_RESET_INDICATOR) != 0;
        }

        GaugeDecoder::Output GaugeDecoder::update(const GaugeSample &sample, uint32_t nowMs)
        {
            Output out;
            out.voltage = voltageFromVcell(sample.vcell);
            out.percent = percentFromSoc(sample.soc);
            out.resetSeen = resetIndicated(sample.status);
            if (!started || out.resetSeen)
            {
                started = true;
                estimatingFrom = nowMs;
                rateStarted = false;
            }
            // 0 % above 3.5 V can't be right: the gauge hasn't settled yet.
            const bool impossible = sample.soc < 256 && out.voltage > EMPTY_IMPOSSIBLE_VOLTS;
            out.estimating = nowMs - estimatingFrom < ESTIMATING_MS || impossible;
            if (out.estimating)
            {
                rateStarted = false;
                return out;
            }

            const float rate = rateFromCrate(sample.crate);
            if (!rateStarted)
            {
                rateStarted = true;
                rateFrom = nowMs;
                smoothed = rate;
            }
            else
            {
                const float dt = static_cast<float>(nowMs - lastRateMs);
                const float alpha = 1.0f - std::exp(-dt / static_cast<float>(RATE_TIME_CONSTANT_MS));
                smoothed += alpha * (rate - smoothed);
            }
            lastRateMs = nowMs;
            out.rate = smoothed;
            out.rateValid = nowMs - rateFrom >= RATE_VALID_MS;
            return out;
        }
    } // namespace Battery
} // namespace SQM
