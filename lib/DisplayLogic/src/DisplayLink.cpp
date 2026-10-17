#include "DisplayController.h"

namespace SQM::Display
{
    bool Link::shouldProbe(bool enabled, uint32_t nowMs) const
    {
        if (!enabled || found)
            return false;
        return !probed || nowMs - lastProbeMs >= PROBE_INTERVAL_MS;
    }

    void Link::probeResult(bool ok, uint32_t nowMs)
    {
        probed = true;
        lastProbeMs = nowMs;
        failures = 0;
        if (ok && !found)
            initPending = true;
        found = ok;
    }

    void Link::writeResult(bool ok, uint32_t nowMs)
    {
        if (!found)
            return;
        if (ok)
        {
            failures = 0;
            return;
        }
        if (++failures < MAX_WRITE_FAILURES)
            return;
        // Lost (unplugged or the bus is stuck): probe again after the interval.
        found = false;
        initPending = false;
        failures = 0;
        probed = true;
        lastProbeMs = nowMs;
    }

    void Link::disable()
    {
        found = false;
        initPending = false;
        probed = false;
        failures = 0;
    }

    bool Button::update(bool levelHigh, uint32_t nowMs)
    {
        if (!started)
        {
            started = true;
            stableHigh = rawHigh = levelHigh;
            rawSinceMs = nowMs;
            return false;
        }
        if (levelHigh != rawHigh)
        {
            rawHigh = levelHigh;
            rawSinceMs = nowMs;
        }
        if (rawHigh == stableHigh || nowMs - rawSinceMs < DEBOUNCE_MS)
            return false;
        stableHigh = rawHigh;
        return !stableHigh; // pressed = pulled low
    }
} // namespace SQM::Display
