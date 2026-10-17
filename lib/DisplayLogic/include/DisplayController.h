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

    // Whether it's night at the device (Core::night): unknown counts as night.
    struct Night
    {
        bool known = false;
        bool isNight = false;
    };

    // One loop pass's inputs.
    struct Inputs
    {
        uint32_t nowMs = 0;
        bool detected = false; // Link::detected()
        Night night;
        uint8_t availableMask = 0; // bit per Page with a data source (View::available)
        bool wake = false;         // POST /api/display/wake
        bool blank = false;        // POST /api/display/blank
        bool button = false;       // Button::update() reported a press
        bool forcedOff = false;    // power saving (spec 032)
    };

    struct Output
    {
        bool on = false;
        Page page = Page::Sky; // when on
        bool nightMode = true; // night layout, minimum brightness
        uint8_t contrast = 0;  // SSD1306 contrast (0x81) value
        uint8_t shift = 0;     // burn-in shift position, 0..3
        bool rotate180 = false;
    };

    // Power, timeout, night and page cycling (research D5/D6).
    class Controller
    {
    public:
        static constexpr uint32_t NIGHT_TIMEOUT_MS = 60000; // "never" at night
        static constexpr uint8_t CONTRAST_LOW = 0x10;
        static constexpr uint8_t CONTRAST_MEDIUM = 0x7F;
        static constexpr uint8_t CONTRAST_HIGH = 0xFF;
        static constexpr uint8_t CONTRAST_NIGHT = 0x00;

        Output tick(const Settings &settings, const Inputs &in);

    private:
        void turnOn(const Settings &settings, const Inputs &in);
        void nextPage(const Settings &settings, const Inputs &in);
        bool timedOut(const Settings &settings, const Inputs &in, bool nightMode) const;

        bool booted = false; // the boot wake happened (again after re-enabling)
        bool lit = false;
        Page page = Page::Sky;
        uint32_t onSinceMs = 0;   // last wake or button press: the timeout runs from here
        uint32_t pageSinceMs = 0; // for cycling
        uint8_t shiftPos = 0;
    };

    // The pages to show, in the configured order: selected and available,
    // or Network when none is (research D4). Returns how many.
    uint8_t visiblePages(const Settings &settings, uint8_t availableMask, Page (&out)[PAGE_COUNT]);
} // namespace SQM::Display
