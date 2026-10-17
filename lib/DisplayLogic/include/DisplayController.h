#pragma once

#include <cstdint>

#include "DisplayTypes.h"

// When the OLED is found, lit, and which page it shows (specs/031-oled-display,
// research D3/D6/D7). Pure logic ticked from the loop with millis(); the
// firmware supplies probe and write results and the button level.

namespace SQM::Display
{
    // Detection and fault back-off (research D3). A disabled display is never
    // probed or written to.
    class Link
    {
    public:
        static constexpr uint32_t PROBE_INTERVAL_MS = 5000;
        static constexpr uint8_t MAX_WRITE_FAILURES = 3;

        // Whether to probe 0x3C now: enabled, not detected, and never probed
        // or the last probe was PROBE_INTERVAL_MS ago.
        bool shouldProbe(bool enabled, uint32_t nowMs) const;
        void probeResult(bool ok, uint32_t nowMs);
        // A data or command write; three failures in a row lose the display.
        void writeResult(bool ok, uint32_t nowMs);
        // Display switched off in settings: forget it, probe at once when on again.
        void disable();

        bool detected() const { return found; }
        // The init sequence (and a full redraw) is due after each (re)detection.
        bool needsInit() const { return found && initPending; }
        void initDone() { initPending = false; }

    private:
        bool found = false;
        bool initPending = false;
        bool probed = false;
        uint32_t lastProbeMs = 0;
        uint8_t failures = 0;
    };

    // 30 ms debounce of an active-low button sampled every loop pass
    // (research D7); one press per press. Held at boot isn't a press.
    class Button
    {
    public:
        static constexpr uint32_t DEBOUNCE_MS = 30;

        // `levelHigh`: the raw pin level. True once per press.
        bool update(bool levelHigh, uint32_t nowMs);

    private:
        bool started = false;
        bool stableHigh = true;
        bool rawHigh = true;
        uint32_t rawSinceMs = 0;
    };
} // namespace SQM::Display
