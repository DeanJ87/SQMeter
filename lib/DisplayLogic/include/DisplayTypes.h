#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>

// Names shared by the display settings, the status document and the
// renderer (specs/031-oled-display, data-model.md). No Arduino headers.

namespace SQM::Display
{
    enum class Page : uint8_t
    {
        Sky = 0,
        Environment,
        Cloud,
        Safety,
        Gps,
        Battery,
        Network,
    };
    constexpr size_t PAGE_COUNT = 7;

    enum class Brightness : uint8_t
    {
        Low = 0,
        Medium,
        High,
    };

    // The display settings (Config::display, contracts/config.md). Defined
    // here so the controller can read them without the config library.
    struct Settings
    {
        static constexpr uint8_t BUTTON_UNSET = 255;
        static constexpr uint8_t MIN_CYCLE_SECONDS = 2;
        static constexpr uint8_t MAX_CYCLE_SECONDS = 60;
        static constexpr uint16_t MIN_TIMEOUT_SECONDS = 10;
        static constexpr uint16_t MAX_TIMEOUT_SECONDS = 1800;

        bool enabled = false;
        // A fixed array, not a vector: Config is copied in many places.
        Page pages[PAGE_COUNT] = {Page::Sky};
        uint8_t pageCount = 1;
        uint8_t cycleSeconds = 5;
        Brightness brightness = Brightness::Medium;
        uint16_t timeoutSeconds = 60; // 0 = never (by day; night always times out)
        bool wakeOnBoot = true;
        bool stayOnAtNight = false; // only counts when night is known
        bool rotate180 = false;
        uint8_t buttonPin = BUTTON_UNSET; // wake button, active low; 255 = none
    };

    inline const char *pageName(Page page)
    {
        static const char *const NAMES[PAGE_COUNT] = {"sky", "environment", "cloud", "safety", "gps", "battery", "network"};
        const auto index = static_cast<size_t>(page);
        return index < PAGE_COUNT ? NAMES[index] : "";
    }

    // Exact names only; false leaves `out` unchanged.
    inline bool parsePage(const char *name, Page &out)
    {
        if (name == nullptr)
            return false;
        for (size_t i = 0; i < PAGE_COUNT; ++i)
        {
            if (std::strcmp(name, pageName(static_cast<Page>(i))) == 0)
            {
                out = static_cast<Page>(i);
                return true;
            }
        }
        return false;
    }

    inline const char *brightnessName(Brightness brightness)
    {
        switch (brightness)
        {
        case Brightness::Low:
            return "low";
        case Brightness::High:
            return "high";
        case Brightness::Medium:
        default:
            return "medium";
        }
    }

    inline bool parseBrightness(const char *name, Brightness &out)
    {
        if (name == nullptr)
            return false;
        for (Brightness b : {Brightness::Low, Brightness::Medium, Brightness::High})
        {
            if (std::strcmp(name, brightnessName(b)) == 0)
            {
                out = b;
                return true;
            }
        }
        return false;
    }
} // namespace SQM::Display
