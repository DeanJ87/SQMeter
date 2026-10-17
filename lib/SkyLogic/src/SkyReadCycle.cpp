#include "calculations/SkyReadCycle.h"

namespace SQM
{
    SkyReadCycle::Action SkyReadCycle::next(uint32_t nowMs, uint16_t integrationMs) const
    {
        switch (current)
        {
        case State::Absent:
            return retryNow || reached(nowMs, retryAtMs) ? Action::Begin : Action::None;
        case State::Idle:
            return !startedOnce || reached(nowMs, lastStartMs + interval) ? Action::Start : Action::None;
        case State::Integrating:
            return reached(nowMs, lastStartMs + integrationMs + MARGIN_MS) ? Action::Collect : Action::None;
        }
        return Action::None;
    }

    void SkyReadCycle::beginDone(uint32_t nowMs, bool ok)
    {
        retryNow = false;
        if (ok)
        {
            current = State::Idle;
            startedOnce = false;
            failures = 0;
            backoffMs = BACKOFF_FIRST_MS;
            return;
        }
        current = State::Absent;
        retryAtMs = nowMs + backoffMs;
        backoffMs = backoffMs >= BACKOFF_MAX_MS / 2 ? BACKOFF_MAX_MS : backoffMs * 2;
    }

    void SkyReadCycle::started(uint32_t nowMs)
    {
        current = State::Integrating;
        startedOnce = true;
        lastStartMs = nowMs;
    }

    void SkyReadCycle::startFailed(uint32_t nowMs)
    {
        startedOnce = true;
        lastStartMs = nowMs;
        failed(nowMs);
    }

    void SkyReadCycle::collected(uint32_t nowMs, bool ok)
    {
        if (ok)
        {
            current = State::Idle;
            failures = 0;
            return;
        }
        failed(nowMs);
    }

    void SkyReadCycle::failed(uint32_t nowMs)
    {
        if (failures < 255)
            ++failures;
        if (failures < REBEGIN_AFTER)
        {
            current = State::Idle;
            return;
        }
        // Lost: re-begin after the first back-off, doubling from there.
        current = State::Absent;
        retryNow = false;
        retryAtMs = nowMs + BACKOFF_FIRST_MS;
        backoffMs = BACKOFF_FIRST_MS * 2;
    }
} // namespace SQM
