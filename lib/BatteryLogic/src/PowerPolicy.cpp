#include "BatteryLogic.h"

// The power policy (specs/032-battery research D7).

namespace SQM
{
    namespace Battery
    {
        namespace
        {
            bool onUsb(const PolicyInput &in)
            {
                return in.state == ChargeState::Charging || in.state == ChargeState::Charged || (in.usbPower && *in.usbPower);
            }
        } // namespace

        PolicyEvents PowerPolicy::update(const PolicyInput &in)
        {
            PolicyEvents events;
            const bool usb = onUsb(in);
            if (!in.powerSaving)
            {
                // Off: no slowing, no display change and no shutdown.
                current = PowerState::Normal;
                belowShutdown = 0;
                return events;
            }

            switch (current)
            {
            case PowerState::Normal:
                if (in.newRead && in.valid && !usb && in.percent < static_cast<float>(in.criticalPercent))
                {
                    current = PowerState::Saving;
                    belowShutdown = 0;
                    events.enteredSaving = true;
                }
                break;
            case PowerState::Saving:
                if (usb || (in.newRead && in.valid && in.percent >= static_cast<float>(in.criticalPercent) + RECOVERY_PERCENT))
                {
                    current = PowerState::Normal;
                    belowShutdown = 0;
                    events.powerRestored = true;
                    break;
                }
                if (!in.newRead || !in.valid)
                    break; // no news: never a step towards sleep
                if (in.percent <= static_cast<float>(in.shutdownPercent))
                {
                    if (++belowShutdown >= READS_TO_SHUTDOWN)
                    {
                        current = PowerState::ShutdownPending;
                        pendingSinceMs = in.nowMs;
                        events.shutdownStarted = true;
                    }
                }
                else
                    belowShutdown = 0;
                break;
            case PowerState::ShutdownPending:
            {
                if (usb)
                {
                    current = PowerState::Normal;
                    belowShutdown = 0;
                    events.powerRestored = true;
                    break;
                }
                const uint32_t elapsed = in.nowMs - pendingSinceMs;
                const uint32_t grace = static_cast<uint32_t>(in.graceSeconds) * 1000U;
                events.sleepNow = (elapsed >= grace && in.dispatcherIdle) || elapsed >= grace + MAX_EXTRA_MS;
                break;
            }
            }
            return events;
        }

        uint32_t PowerPolicy::secondsLeft(uint32_t nowMs, uint16_t graceSeconds) const
        {
            if (current != PowerState::ShutdownPending)
                return 0;
            const uint32_t elapsed = (nowMs - pendingSinceMs) / 1000U;
            return elapsed >= graceSeconds ? 0 : graceSeconds - elapsed;
        }
    } // namespace Battery
} // namespace SQM
