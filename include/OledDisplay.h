#pragma once

#include <Wire.h>

#include <atomic>
#include <cstdint>
#include <memory>

#include "DeviceCore.h"
#include "DisplayController.h"
#include "DisplayFrame.h"

// The optional SSD1306 128 x 64 OLED at 0x3C on the main I2C bus
// (specs/031-oled-display, research D1-D3). Everything that isn't I2C is in
// lib/DisplayLogic; everything here runs on the loop task, like every other
// I2C transaction, so the bus needs no lock.

namespace SQM
{
    // The SSD1306 commands this firmware uses, over Arduino Wire only.
    class OledTransport
    {
    public:
        static constexpr uint8_t ADDRESS = 0x3C;

        explicit OledTransport(TwoWire &bus)
            : wire(bus)
        {
        }

        // ACK at 0x3C and a one-byte status read (research D3).
        bool probe();
        // The init sequence (display left off), rotation and contrast.
        bool init(bool rotate180, uint8_t contrast, bool night);
        bool setPower(bool on);
        bool setRotation(bool rotate180);
        // Contrast, and VCOMH at its lowest at night: the dimmest the panel goes.
        bool setContrast(uint8_t contrast, bool night);
        // One 128-byte framebuffer page, in chunks that fit Wire's buffer.
        bool writePage(int page, const uint8_t *data);

    private:
        bool commands(const uint8_t *bytes, size_t count);
        TwoWire &wire;
    };

    // The display as the loop sees it: detection, the controller, rendering and
    // one dirty page per pass (research D2). No bus traffic while disabled.
    class OledDisplay
    {
    public:
        explicit OledDisplay(TwoWire &bus)
            : transport(bus)
        {
        }

        // Settings at boot (loop task).
        void configure(const Display::Settings &next) { applySettings(next); }
        // After a save, which runs on the web server's task: the next tick()
        // applies the settings it is given.
        void requestReconfigure() { reconfigurePending = true; }
        // From the web server's task: applied by the loop within one pass.
        void requestWake() { wakePending = true; }
        void requestBlank() { blankPending = true; }

        // A new reading set (after each sensor refresh).
        void setView(const Display::View &view, const Display::Night &night);
        // Every loop pass, with the current settings.
        void tick(uint32_t nowMs, const Display::Settings &current);

        bool enabled() const { return settings.enabled; }
        // For /api/status and the settings dependencies; safe from any task.
        bool detected() const { return detectedNow.load(); }
        bool forcedOff() const { return false; } // power saving: spec 032
        Core::DisplayStatus status() const;

    private:
        void applySettings(const Display::Settings &next);
        void handleLink(uint32_t nowMs);
        void applyOutput(const Display::Output &out, uint32_t nowMs);
        void sendOneDirtyPage(uint32_t nowMs, bool wantOn);
        bool sent(bool ok, uint32_t nowMs);
        uint8_t availableMask() const;

        OledTransport transport;
        Display::Settings settings;
        std::unique_ptr<uint8_t[]> buffer; // 1 KB, only while enabled (FR-016)
        std::unique_ptr<Display::Frame> frame;
        Display::Link link;
        Display::Controller controller;
        Display::Button button;
        Display::View view;
        Display::Night night;
        bool viewChanged = true;
        Display::Output shown; // what the panel was last told
        bool panelOn = false;
        bool rendered = false; // `frame` holds the shown page
        uint8_t buttonPin = Display::Settings::BUTTON_UNSET;

        std::atomic<bool> reconfigurePending{false};
        std::atomic<bool> wakePending{false};
        std::atomic<bool> blankPending{false};
        std::atomic<bool> detectedNow{false};
        std::atomic<bool> onNow{false};
        std::atomic<uint8_t> pageNow{0};
        std::atomic<bool> nightNow{true};
    };
} // namespace SQM
