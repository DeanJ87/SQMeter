#include "OledDisplay.h"

#include <Arduino.h>

#include <new>

#include "Logger.h"

#include "BoardFeatures.h"

#if SQM_HAS_DISPLAY
namespace SQM
{
    namespace
    {
        constexpr const char *TAG = "Display";
        constexpr uint8_t CONTROL_COMMANDS = 0x00;
        constexpr uint8_t CONTROL_DATA = 0x40;
        // Wire's buffer is 128 bytes: a page goes in four writes (the column
        // address advances by itself).
        constexpr size_t DATA_CHUNK = 32;
        constexpr uint8_t VCOMH_DAY = 0x40;
        constexpr uint8_t VCOMH_NIGHT = 0x00;
    } // namespace

    bool OledTransport::commands(const uint8_t *bytes, size_t count)
    {
        wire.beginTransmission(ADDRESS);
        wire.write(CONTROL_COMMANDS);
        wire.write(bytes, count);
        return wire.endTransmission() == 0;
    }

    bool OledTransport::probe()
    {
        wire.beginTransmission(ADDRESS);
        if (wire.endTransmission() != 0)
            return false;
        if (wire.requestFrom(static_cast<int>(ADDRESS), 1) != 1)
            return false;
        wire.read(); // status byte: not a reliable identity across clones (D3)
        return true;
    }

    bool OledTransport::init(bool rotate180, uint8_t contrast, bool night)
    {
        static const uint8_t SEQUENCE[] = {
            0xAE, // display off
            0xD5,
            0x80, // clock divide
            0xA8,
            0x3F, // multiplex: 64 rows
            0xD3,
            0x00, // no offset
            0x40, // start line 0
            0x8D,
            0x14, // charge pump on
            0x20,
            0x02, // page addressing
            0xDA,
            0x12, // COM pins for 128 x 64
            0xD9,
            0xF1, // pre-charge
            0xA4, // show RAM
            0xA6, // not inverted
        };
        return commands(SEQUENCE, sizeof SEQUENCE) && setRotation(rotate180) && setContrast(contrast, night);
    }

    bool OledTransport::setPower(bool on)
    {
        const uint8_t cmd = on ? 0xAF : 0xAE;
        return commands(&cmd, 1);
    }

    bool OledTransport::setRotation(bool rotate180)
    {
        const uint8_t remap[] = {static_cast<uint8_t>(rotate180 ? 0xA0 : 0xA1), static_cast<uint8_t>(rotate180 ? 0xC0 : 0xC8)};
        return commands(remap, sizeof remap);
    }

    bool OledTransport::setContrast(uint8_t contrast, bool night)
    {
        const uint8_t cmd[] = {0x81, contrast, 0xDB, night ? VCOMH_NIGHT : VCOMH_DAY};
        return commands(cmd, sizeof cmd);
    }

    bool OledTransport::writePage(int page, const uint8_t *data)
    {
        const uint8_t address[] = {static_cast<uint8_t>(0xB0 | (page & 0x07)), 0x00, 0x10};
        if (!commands(address, sizeof address))
            return false;
        for (size_t offset = 0; offset < static_cast<size_t>(Display::WIDTH); offset += DATA_CHUNK)
        {
            wire.beginTransmission(ADDRESS);
            wire.write(CONTROL_DATA);
            wire.write(data + offset, DATA_CHUNK);
            if (wire.endTransmission() != 0)
                return false;
        }
        return true;
    }

    void OledDisplay::applySettings(const Display::Settings &next)
    {
        if (settings.enabled && !next.enabled)
        {
            // One last command so a lit panel goes dark, then silence (FR-016).
            if (panelOn)
                transport.setPower(false);
            link.disable();
            frame.reset();
            buffer.reset();
            panelOn = rendered = false;
            detectedNow = onNow = false;
        }
        if (next.enabled && !buffer)
        {
            buffer.reset(new (std::nothrow) uint8_t[Display::FRAME_BYTES]);
            if (buffer)
            {
                frame = std::make_unique<Display::Frame>(buffer.get());
                frame->clear();
            }
            else
            {
                Logger::warn(TAG, "No memory for the display framebuffer");
            }
        }
        const uint8_t pin = next.enabled ? next.buttonPin : Display::Settings::BUTTON_UNSET;
        if (pin != buttonPin && pin != Display::Settings::BUTTON_UNSET)
        {
            pinMode(pin, INPUT_PULLUP);
            button = Display::Button{};
        }
        buttonPin = pin;
        settings = next;
    }

    void OledDisplay::setView(const Display::View &next, const Display::Night &nextNight)
    {
        view = next;
        night = nextNight;
        viewChanged = true;
    }

    uint8_t OledDisplay::availableMask() const
    {
        uint8_t mask = 0;
        for (size_t i = 0; i < Display::PAGE_COUNT; ++i)
            if (view.available(static_cast<Display::Page>(i)))
                mask = static_cast<uint8_t>(mask | (1U << i));
        return mask;
    }

    bool OledDisplay::sent(bool ok, uint32_t nowMs)
    {
        link.writeResult(ok, nowMs);
        return ok;
    }

    void OledDisplay::handleLink(uint32_t nowMs)
    {
        if (link.shouldProbe(true, nowMs))
        {
            const bool found = transport.probe();
            link.probeResult(found, nowMs);
            if (found)
                Logger::info(TAG, "OLED found at 0x3C");
        }
        if (!link.needsInit())
            return;
        const bool nightMode = !night.known || night.isNight;
        const uint8_t contrast = nightMode ? Display::Controller::CONTRAST_NIGHT : Display::Controller::CONTRAST_MEDIUM;
        if (!sent(transport.init(settings.rotate180, contrast, nightMode), nowMs))
            return;
        link.initDone();
        shown = Display::Output{};
        shown.rotate180 = settings.rotate180;
        shown.contrast = contrast;
        shown.nightMode = nightMode;
        panelOn = rendered = false;
        frame->markAllDirty(); // the panel's memory is random after power-up
    }

    void OledDisplay::applyOutput(const Display::Output &out, uint32_t nowMs)
    {
        if (out.contrast != shown.contrast || out.nightMode != shown.nightMode)
            sent(transport.setContrast(out.contrast, out.nightMode), nowMs);
        if (out.rotate180 != shown.rotate180 && sent(transport.setRotation(out.rotate180), nowMs))
        {
            frame->markAllDirty(); // the column remap applies to new writes
            rendered = false;
        }
        if (out.on && (!rendered || viewChanged || out.page != shown.page || out.shift != shown.shift || out.nightMode != shown.nightMode))
        {
            Display::render(*frame, out.page, view, out.nightMode, out.shift);
            rendered = true;
            viewChanged = false;
        }
        if (!out.on && panelOn && sent(transport.setPower(false), nowMs))
            panelOn = false;
        shown = out;
    }

    // At most one page (~3-12 ms) per loop pass; the panel is switched on
    // once the picture is complete.
    void OledDisplay::sendOneDirtyPage(uint32_t nowMs, bool wantOn)
    {
        if (!wantOn)
            return;
        const int page = frame->nextDirtyPage();
        if (page >= 0)
        {
            if (sent(transport.writePage(page, frame->page(page)), nowMs))
                frame->markClean(page);
            return;
        }
        if (!panelOn && sent(transport.setPower(true), nowMs))
            panelOn = true;
    }

    void OledDisplay::tick(uint32_t nowMs, const Display::Settings &current)
    {
        if (reconfigurePending.exchange(false))
            applySettings(current);
        const bool wake = wakePending.exchange(false);
        const bool blank = blankPending.exchange(false);
        if (!settings.enabled || !frame)
            return;

        handleLink(nowMs);
        Display::Inputs in;
        in.nowMs = nowMs;
        in.detected = link.detected() && !link.needsInit();
        in.night = night;
        in.availableMask = availableMask();
        in.wake = wake;
        in.blank = blank;
        in.button = buttonPin != Display::Settings::BUTTON_UNSET && button.update(digitalRead(buttonPin) == HIGH, nowMs);
        in.forcedOff = forcedOff();
        const Display::Output out = controller.tick(settings, in);
        if (in.detected)
        {
            applyOutput(out, nowMs);
            sendOneDirtyPage(nowMs, out.on);
        }

        detectedNow = link.detected();
        onNow = out.on && panelOn;
        pageNow = static_cast<uint8_t>(out.page);
        nightNow = out.nightMode;
    }

    Core::DisplayStatus OledDisplay::status() const
    {
        Core::DisplayStatus s;
        s.detected = detectedNow.load();
        s.on = onNow.load();
        s.page = static_cast<Display::Page>(pageNow.load());
        s.nightMode = nightNow.load();
        return s;
    }
} // namespace SQM

#else // classic ESP32 builds: no driver (include/BoardFeatures.h)

namespace SQM
{
    void OledDisplay::applySettings(const Display::Settings &next)
    {
        settings = next;
        settings.enabled = false; // never probed, never fed
    }

    void OledDisplay::setView(const Display::View &, const Display::Night &) {}

    void OledDisplay::tick(uint32_t, const Display::Settings &) {}

    Core::DisplayStatus OledDisplay::status() const
    {
        return Core::DisplayStatus{};
    }
} // namespace SQM

#endif
