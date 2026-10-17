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
