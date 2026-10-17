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
                fromNormal(in, events);
                break;
            case PowerState::Saving:
                fromSaving(in, events);
                break;
            case PowerState::ShutdownPending:
                fromPending(in, events);
                break;
            }
            return events;
        }

        void PowerPolicy::backToNormal(PolicyEvents &events)
        {
            current = PowerState::Normal;
            belowShutdown = 0;
            events.powerRestored = true;
        }

        void PowerPolicy::fromNormal(const PolicyInput &in, PolicyEvents &events)
        {
            if (in.newRead && in.valid && !onUsb(in) && in.percent < static_cast<float>(in.criticalPercent))
            {
                current = PowerState::Saving;
                belowShutdown = 0;
                events.enteredSaving = true;
            }
        }

        void PowerPolicy::fromSaving(const PolicyInput &in, PolicyEvents &events)
        {
            const bool fresh = in.newRead && in.valid; // anything else is no news: never a step towards sleep
            if (onUsb(in) || (fresh && in.percent >= static_cast<float>(in.criticalPercent) + RECOVERY_PERCENT))
            {
                backToNormal(events);
                return;
            }
            if (!fresh)
                return;
            belowShutdown = in.percent <= static_cast<float>(in.shutdownPercent) ? static_cast<uint8_t>(belowShutdown + 1) : 0;
            if (belowShutdown >= READS_TO_SHUTDOWN)
            {
                current = PowerState::ShutdownPending;
                pendingSinceMs = in.nowMs;
                events.shutdownStarted = true;
            }
        }

        void PowerPolicy::fromPending(const PolicyInput &in, PolicyEvents &events)
        {
            if (onUsb(in))
            {
                backToNormal(events);
                return;
            }
            const uint32_t elapsed = in.nowMs - pendingSinceMs;
            const uint32_t grace = static_cast<uint32_t>(in.graceSeconds) * 1000U;
            events.sleepNow = (elapsed >= grace && in.dispatcherIdle) || elapsed >= grace + MAX_EXTRA_MS;
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
