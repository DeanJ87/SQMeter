#pragma once

#include <cstddef>
#include <cstdint>

#include "DisplayTypes.h"

// The 128 x 64 monochrome framebuffer of an SSD1306 OLED and the two fonts
// drawn into it (specs/031-oled-display, research D1/D2, contracts/screen.md).
// Pure C++: the firmware (src/OledDisplay.cpp) only copies dirty pages to the
// display.

namespace SQM::Display
{
    constexpr int WIDTH = 128;
    constexpr int HEIGHT = 64;
    constexpr int PAGES = 8; // 8 rows of pixels per page byte
    constexpr size_t FRAME_BYTES = static_cast<size_t>(WIDTH) * PAGES;

    // Small font: 5 x 7 glyphs, 6 px advance, 8 px line (21 glyphs a line).
    constexpr int SMALL_W = 5;
    constexpr int SMALL_H = 7;
    constexpr int SMALL_ADVANCE = 6;
    constexpr int LINE_H = 8;
    // Large font: the small glyphs doubled, 10 x 14 in a 10 x 16 cell, 12 px advance.
    constexpr int LARGE_W = 10;
    constexpr int LARGE_H = 16;
    constexpr int LARGE_ADVANCE = 12;

    // The burn-in shift: (0,0) (1,0) (1,1) (0,1), one step per page change.
    struct Shift
    {
        int dx;
        int dy;
    };
    Shift shiftOffset(uint8_t position);

    class Frame
    {
    public:
        // `buffer` holds FRAME_BYTES bytes and outlives the frame (the firmware
        // allocates it only when the display is enabled).
        explicit Frame(uint8_t *buffer);

        void clear();
        void setPixel(int x, int y, bool on);
        bool pixel(int x, int y) const;
        void fillRect(int x, int y, int w, int h, bool on);

        // Small text at (x, y) (top left), cut with a trailing ellipsis when
        // wider than `maxWidth`. Returns the width drawn in pixels.
        int drawText(int x, int y, const char *text, int maxWidth = WIDTH, bool on = true);
        // Large text (any small glyph, doubled). Returns the width drawn.
        int drawLarge(int x, int y, const char *text, bool on = true);

        static int textWidth(const char *text);
        static int largeWidth(const char *text);

        // After drawing a whole picture: marks the pages whose bytes differ
        // from the last committed picture.
        void commit();
        void markAllDirty() { dirty = 0xFF; }
        void markClean(int page);
        uint8_t dirtyMask() const { return dirty; }
        // The lowest dirty page, or -1.
        int nextDirtyPage() const;
        const uint8_t *page(int index) const { return buf + static_cast<size_t>(index) * WIDTH; }

    private:
        uint8_t *buf;
        uint8_t dirty = 0;
        uint64_t pageHash[PAGES] = {}; // of the last committed picture (a blank screen at first)
    };

    // Where things go on a page (contracts/screen.md); the burn-in shift is
    // added to every position.
    namespace Layout
    {
        constexpr int TITLE_Y = 0;   // line 1, small
        constexpr int LARGE_Y = 10;  // the primary value, large
        constexpr int LINE_A_Y = 30; // lines below, small
        constexpr int LINE_B_Y = 40;
        constexpr int NET_A_Y = 20; // the Network page has no large value
        constexpr int NET_B_Y = 32;
        constexpr int NIGHT_Y = 24; // night: one large value, centred
        constexpr int INVERSE_PAD = 2;
    } // namespace Layout

    // What the screen may show, already decided by Core::displayView from the
    // readings snapshot (stale or faulted data is invalid, never a value).
    // Fixed-size text so a view is cheap to copy.
    struct View
    {
        struct Sky
        {
            bool available = false; // the authoritative sky sensor is on
            bool valid = false;
            float sqm = 0.0F;
            int bortle = 0;
            float nelm = 0.0F;
            const char *label = "SQM"; // "SQM" or "SQM-L"
            struct Other
            {
                bool present = false; // the other sky sensor is on
                bool valid = false;
                float sqm = 0.0F;
                const char *label = "SQM-L";
            } other;
        } sky;
        struct Environment
        {
            bool available = false; // BME280 detected
            bool valid = false;
            float temperatureC = 0.0F;
            float humidity = 0.0F;
            float dewpointC = 0.0F;
        } environment;
        struct Cloud
        {
            bool available = false; // MLX90614 detected
            bool valid = false;
            float skyTemperatureC = 0.0F;
            const char *condition = ""; // clear | cloudy | overcast
        } cloud;
        struct Safety
        {
            bool known = false;
            bool safe = false;
            char firstReason[48] = ""; // cut to fit when drawn
        } safety;
        struct Gps
        {
            bool available = false; // GPS enabled
            bool valid = false;
            bool fix = false;
            uint32_t satellites = 0;
            double latitude = 0.0;
            double longitude = 0.0;
        } gps;
        struct Battery
        {
            bool available = false; // spec 032: a battery fitted
            bool valid = false;
            uint8_t percent = 0;
            const char *state = ""; // Chg | Full | Disch | Fault | No batt
        } battery;
        enum class NetMode : uint8_t
        {
            None,
            Wifi,
            Ap,
        };
        struct Network
        {
            char name[33] = "";
            NetMode mode = NetMode::None;
            char ip[40] = ""; // the address in use, "" when none
            char apName[33] = "";
        } network;

        // Whether `page` has a data source (research D4).
        bool available(Page page) const;
    };

    // Draws `page` (day or night layout) shifted by `shift`, then commits the
    // frame so only changed pages are sent.
    void render(Frame &frame, Page page, const View &view, bool nightMode, uint8_t shift);
} // namespace SQM::Display
