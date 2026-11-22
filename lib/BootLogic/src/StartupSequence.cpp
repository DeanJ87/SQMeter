#include "StartupSequence.h"

namespace SQM
{
    namespace Boot
    {
        namespace
        {
            constexpr uint32_t ALWAYS = stepBit(Step::ConfirmImage) | stepBit(Step::WifiFullPower);

            bool inSet(uint32_t steps, uint8_t index)
            {
                return ((steps >> index) & 1UL) != 0;
            }

            // The first step after the confirmation also waits out the settle pause.
            uint32_t pauseBeforeMs(Step step, bool firstAfterConfirm)
            {
                const uint32_t gap = gapBeforeMs(step);
                return firstAfterConfirm && gap < SETTLE_AFTER_CONFIRM_MS ? SETTLE_AFTER_CONFIRM_MS : gap;
            }
        } // namespace

        uint32_t gapBeforeMs(Step step)
        {
            switch (step)
            {
            case Step::ConfirmImage:
                return 0;
            case Step::Bluetooth:
                return BLUETOOTH_GAP_MS;
            default:
                return STEP_GAP_MS;
            }
        }

        uint32_t plannedMsUntil(uint32_t steps, Step last)
        {
            steps |= ALWAYS;
            uint32_t total = 0;
            for (uint8_t i = 1; i <= static_cast<uint8_t>(last) && i < STEP_COUNT; ++i)
            {
                if (!inSet(steps, i))
                    continue;
                total += pauseBeforeMs(static_cast<Step>(i), total == 0);
            }
            return total;
        }

        uint32_t plannedMs(uint32_t steps)
        {
            return plannedMsUntil(steps, Step::WifiFullPower);
        }

        StartupSequence::StartupSequence(uint32_t steps)
            : enabled((steps & ALL_STEPS) | ALWAYS)
        {
        }

        void StartupSequence::start(uint32_t nowMs)
        {
            started = true;
            cursor = 0;
            lastRunAtMs = nowMs;
            lastWasConfirm = false;
        }

        std::optional<Step> StartupSequence::next(uint32_t nowMs)
        {
            if (!started)
                return std::nullopt;
            // Steps not in the set are passed over at once, without a pause.
            while (cursor < STEP_COUNT && !inSet(enabled, cursor))
                ++cursor;
            if (finished())
                return std::nullopt;
            const Step step = static_cast<Step>(cursor);
            if (nowMs - lastRunAtMs < pauseBeforeMs(step, lastWasConfirm))
                return std::nullopt;
            lastRunAtMs = nowMs;
            lastWasConfirm = step == Step::ConfirmImage;
            ++cursor;
            return step;
        }

        bool StartupSequence::done(Step step) const
        {
            return started && static_cast<uint8_t>(step) < cursor;
        }
    } // namespace Boot
} // namespace SQM
