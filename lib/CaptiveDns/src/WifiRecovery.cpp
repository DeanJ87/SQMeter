#include "WifiRecovery.h"

#include "CaptivePortal.h"

#include <algorithm>

namespace SQM
{
    WifiRecovery::WifiRecovery(uint32_t baseDelayMs, uint32_t maxDelayMs)
        : base(1),
          cap(std::max<uint32_t>(1, std::min(maxDelayMs, RETRY_CAP_MS))),
          delay(1)
    {
        base = std::max<uint32_t>(1, std::min(baseDelayMs, cap));
        delay = base;
    }

    void WifiRecovery::begin(uint32_t now)
    {
        startedAt = now;
        lastAttemptAt = now;
        delay = base;
    }

    void WifiRecovery::credentialsChanged(uint32_t now)
    {
        lastAttemptAt = now;
        delay = base;
        down = false; // the next disconnected loop starts the timers afresh
        closedIdle = false;
    }

    void WifiRecovery::attempted(uint32_t now)
    {
        lastAttemptAt = now;
        delay = std::min(delay * 2, cap);
    }

    void WifiRecovery::hotspotOpened(uint32_t now)
    {
        hotspotWasOpen = true;
        lastUseAt = now;
        // The station keeps trying, less often while the setup screen scans.
        delay = std::min(std::max(base, HOTSPOT_RETRY_MS), cap);
        lastAttemptAt = now;
    }

    WifiRecovery::Action WifiRecovery::opening(uint32_t now)
    {
        hotspotOpened(now);
        return Action::OpenHotspot;
    }

    bool WifiRecovery::hotspotWanted(const State &state) const
    {
        if (joined)
            return false; // a later outage only reconnects (FR-007)
        if (closedIdle)
            return state.now - closedAt >= HOTSPOT_REOPEN_AFTER_MS;
        return CaptivePortal::shouldOpenHotspot(true, false, state.now - startedAt);
    }

    void WifiRecovery::noteDown(const State &state)
    {
        if (!down)
        {
            down = true;
            downSince = state.now;
            lastResetAt = state.now;
            lastUseAt = state.now;
            inRangeSince = state.now;
        }
        else if (state.networkInRange && !wasInRange)
        {
            inRangeSince = state.now; // came into range during the outage
        }
        wasInRange = state.networkInRange;
        if (state.hotspotInUse || state.busy)
            lastUseAt = state.now;
    }

    bool WifiRecovery::restartDue(const State &state) const
    {
        const uint32_t now = state.now;
        // Seen but not joinable for the whole time: the stuck case a restart
        // fixes. Counted from the outage start, or from coming into range.
        const bool stuck = state.networkInRange && now - inRangeSince >= RESTART_AFTER_MS;
        return state.autoReconnect && !restartSent && stuck && !state.hotspotInUse && !state.busy && now - lastUseAt >= IN_USE_GRACE_MS;
    }

    WifiRecovery::Action WifiRecovery::next(const State &state)
    {
        const uint32_t now = state.now;

        if (state.hotspotOpen && !hotspotWasOpen)
            hotspotOpened(now); // opened by someone else
        hotspotWasOpen = state.hotspotOpen;

        if (!state.hasCredentials)
            return state.hotspotOpen ? Action::None : opening(now);

        if (state.connected)
        {
            // Healthy: the next outage starts from the short base delay again.
            joined = true;
            down = false;
            closedIdle = false;
            delay = base;
            return Action::None;
        }

        noteDown(state);

        if (restartDue(state))
        {
            restartSent = true;
            return Action::Restart;
        }

        return state.hotspotOpen ? whileHotspotOpen(state) : whileHotspotClosed(state);
    }

    WifiRecovery::Action WifiRecovery::whileHotspotOpen(const State &state)
    {
        const uint32_t now = state.now;
        // Nobody needs setup: the outage isn't a reason to stay in setup mode.
        if (state.autoReconnect && !state.hotspotInUse && !state.busy && now - lastUseAt >= HOTSPOT_IDLE_CLOSE_MS)
        {
            closedIdle = true;
            closedAt = now;
            hotspotWasOpen = false;
            lastResetAt = now;
            delay = base;
            attempted(now);
            return Action::CloseHotspot;
        }
        if (!state.busy && retryDue(now))
        {
            attempted(now);
            return Action::Retry;
        }
        return Action::None;
    }

    WifiRecovery::Action WifiRecovery::whileHotspotClosed(const State &state)
    {
        const uint32_t now = state.now;
        if (hotspotWanted(state))
            return opening(now);
        if (!state.autoReconnect || state.busy)
            return Action::None;
        if (now - lastResetAt >= (state.networkInRange ? STACK_RESET_EVERY_MS : OUT_OF_RANGE_RESET_EVERY_MS))
        {
            lastResetAt = now;
            attempted(now);
            return Action::ResetStack;
        }
        if (retryDue(now))
        {
            attempted(now);
            return Action::Retry;
        }
        return Action::None;
    }
} // namespace SQM
