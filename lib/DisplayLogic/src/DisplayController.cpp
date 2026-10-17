#include "DisplayController.h"

namespace SQM::Display
{
    namespace
    {
        bool contains(const Page *pages, uint8_t count, Page page)
        {
            for (uint8_t i = 0; i < count; ++i)
                if (pages[i] == page)
                    return true;
            return false;
        }

        uint8_t dayContrast(Brightness brightness)
        {
            switch (brightness)
            {
            case Brightness::Low:
                return Controller::CONTRAST_LOW;
            case Brightness::High:
                return Controller::CONTRAST_HIGH;
            case Brightness::Medium:
            default:
                return Controller::CONTRAST_MEDIUM;
            }
        }
    } // namespace

    uint8_t visiblePages(const Settings &settings, uint8_t availableMask, Page (&out)[PAGE_COUNT])
    {
        uint8_t count = 0;
        for (uint8_t i = 0; i < settings.pageCount && i < PAGE_COUNT; ++i)
        {
            const Page p = settings.pages[i];
            if (((availableMask >> static_cast<unsigned>(p)) & 1U) != 0 && !contains(out, count, p))
                out[count++] = p;
        }
        if (count == 0)
            out[count++] = Page::Network; // so the device can always be found
        return count;
    }

    void Controller::turnOn(const Settings &settings, const Inputs &in)
    {
        if (!lit)
        {
            Page pages[PAGE_COUNT];
            visiblePages(settings, in.availableMask, pages);
            page = pages[0];
            pageSinceMs = in.nowMs;
        }
        lit = true;
        onSinceMs = in.nowMs;
    }

    // The next visible page after the current one in the configured order;
    // a page that has become unavailable is skipped here.
    void Controller::nextPage(const Settings &settings, const Inputs &in)
    {
        Page pages[PAGE_COUNT];
        const uint8_t count = visiblePages(settings, in.availableMask, pages);
        Page next = pages[0];
        int currentOrder = -1;
        for (uint8_t i = 0; i < settings.pageCount; ++i)
            if (settings.pages[i] == page)
                currentOrder = i;
        if (currentOrder >= 0)
        {
            for (uint8_t step = 1; step <= settings.pageCount; ++step)
            {
                const Page candidate = settings.pages[(currentOrder + step) % settings.pageCount];
                if (contains(pages, count, candidate))
                {
                    next = candidate;
                    break;
                }
            }
        }
        if (next != page)
            shiftPos = static_cast<uint8_t>((shiftPos + 1) % 4);
        page = next;
        pageSinceMs = in.nowMs;
    }

    bool Controller::timedOut(const Settings &settings, const Inputs &in, bool nightMode) const
    {
        const bool knownNight = in.night.known && in.night.isNight;
        const bool forever = (!nightMode && settings.timeoutSeconds == 0) || (knownNight && settings.stayOnAtNight);
        if (forever)
            return false;
        const uint32_t limitMs = settings.timeoutSeconds == 0 ? NIGHT_TIMEOUT_MS : settings.timeoutSeconds * 1000U;
        return in.nowMs - onSinceMs >= limitMs;
    }

    // Boot, wake, blank and the button, in that order.
    void Controller::handleRequests(const Settings &settings, const Inputs &in)
    {
        if (!booted)
        {
            booted = true;
            if (settings.wakeOnBoot)
                turnOn(settings, in);
        }
        if (in.blank)
            lit = false;
        if (in.wake)
            turnOn(settings, in);
        if (!in.button)
            return;
        if (!lit)
        {
            turnOn(settings, in);
            return;
        }
        nextPage(settings, in);
        onSinceMs = in.nowMs; // a press while lit also restarts the timeout
    }

    // A page that went away is replaced at the next change, or now when
    // nothing cycles.
    void Controller::cycle(const Settings &settings, const Inputs &in)
    {
        Page pages[PAGE_COUNT];
        const uint8_t count = visiblePages(settings, in.availableMask, pages);
        const bool shown = contains(pages, count, page);
        const bool cycleDue = count > 1 && in.nowMs - pageSinceMs >= settings.cycleSeconds * 1000U;
        if (cycleDue || (!shown && count == 1))
            nextPage(settings, in);
    }

    Output Controller::tick(const Settings &settings, const Inputs &in)
    {
        Output out;
        out.nightMode = !in.night.known || in.night.isNight;
        out.contrast = out.nightMode ? CONTRAST_NIGHT : dayContrast(settings.brightness);
        out.rotate180 = settings.rotate180;
        if (!settings.enabled)
            booted = false; // switching it on again counts as a boot
        if (!settings.enabled || !in.detected || in.forcedOff)
        {
            lit = false;
            out.shift = shiftPos;
            return out;
        }
        handleRequests(settings, in);
        if (lit && timedOut(settings, in, out.nightMode))
            lit = false;
        if (lit)
            cycle(settings, in);
        out.on = lit;
        out.page = page;
        out.shift = shiftPos;
        return out;
    }
} // namespace SQM::Display
