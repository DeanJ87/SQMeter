#pragma once

#include <cstdint>

namespace SQM
{
    // Gets a device with a saved network back onto it without a human
    // (specs/014-wifi-setup FR-004, FR-007, FR-010 to FR-012). WiFiManager
    // feeds it the WiFi state every loop and carries out the action it
    // returns. Kept free of Arduino for tests.
    class WifiRecovery
    {
    public:
        // While the saved network isn't joined it's retried at least this often,
        // whatever the configured maximum back-off.
        static constexpr uint32_t RETRY_CAP_MS = 60000;
        // First retry delay while the hotspot is open (the setup screen scans).
        static constexpr uint32_t HOTSPOT_RETRY_MS = 30000;
        // With a saved network, a hotspot nobody has used for this long closes,
        // so an outage at boot doesn't leave the device in setup mode.
        static constexpr uint32_t HOTSPOT_IDLE_CLOSE_MS = 3 * 60000;
        // After closing an idle hotspot the station gets this long on its own
        // before the hotspot may open again.
        static constexpr uint32_t HOTSPOT_REOPEN_AFTER_MS = 2 * 60000;
        // Every this long without the network while it's in range, restart the
        // WiFi stack instead of only asking it to connect again.
        static constexpr uint32_t STACK_RESET_EVERY_MS = 3 * 60000;
        // The same while the network isn't in range (away in the field): rarely,
        // in case the driver no longer sees a network that is back.
        static constexpr uint32_t OUT_OF_RANGE_RESET_EVERY_MS = 15 * 60000;
        // The network in range (seen) but not joined for this long, and nobody
        // on the hotspot, no setup or update running: restart the device. A
        // restart has always recovered. Never while the network is out of
        // range: a unit away from its network keeps running.
        static constexpr uint32_t RESTART_AFTER_MS = 15 * 60000;
        // Someone on the hotspot, or a setup or update, holds the restart off
        // until this long after it ends.
        static constexpr uint32_t IN_USE_GRACE_MS = 60000;

        enum class Action
        {
            None,
            Retry,        // ask the station to join the saved network again
            ResetStack,   // switch WiFi off and on again, then join
            OpenHotspot,  // open the setup hotspot (the station keeps trying)
            CloseHotspot, // close it, then reset the stack and join
            Restart,      // restart the device (returned once)
        };

        struct State
        {
            uint32_t now = 0;
            bool hasCredentials = false;
            bool connected = false;
            bool hotspotOpen = false;
            bool hotspotInUse = false;  // a phone or laptop is on the hotspot
            bool busy = false;          // the setup screen is trying a network, or a firmware update runs
            bool autoReconnect = true;  // off: the station isn't retried while the hotspot is closed
            bool networkInRange = true; // false: the last attempt didn't find the saved network
        };

        WifiRecovery(uint32_t baseDelayMs, uint32_t maxDelayMs);

        // Trying started (boot): the first attempt was just made.
        void begin(uint32_t now);
        // New credentials were just tried: fresh back-off and timers.
        void credentialsChanged(uint32_t now);

        Action next(const State &state);

        uint32_t retryDelayMs() const { return delay; }
        uint32_t downForMs(uint32_t now) const { return down ? now - downSince : 0; }
        bool joinedSinceBoot() const { return joined; }

    private:
        uint32_t base;
        uint32_t cap;
        uint32_t delay;
        uint32_t lastAttemptAt = 0;
        uint32_t startedAt = 0;
        bool joined = false;
        bool down = false;
        uint32_t downSince = 0;
        uint32_t lastResetAt = 0;
        uint32_t lastUseAt = 0; // hotspot opened or used, setup or update running
        bool wasInRange = false;
        uint32_t inRangeSince = 0; // in range without a break since (within this outage)
        bool hotspotWasOpen = false;
        bool closedIdle = false;
        uint32_t closedAt = 0;
        bool restartSent = false;

        void attempted(uint32_t now);
        void hotspotOpened(uint32_t now);
        Action opening(uint32_t now);
        bool retryDue(uint32_t now) const { return now - lastAttemptAt >= delay; }
        bool hotspotWanted(const State &state) const;
        void noteDown(const State &state);
        bool restartDue(const State &state) const;
        Action whileHotspotOpen(const State &state);
        Action whileHotspotClosed(const State &state);
    };
} // namespace SQM
