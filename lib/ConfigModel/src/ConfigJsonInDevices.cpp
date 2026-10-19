#include "Config.h"
#include "ConfigInternal.h"
#include <ArduinoJson.h>

// Settings as JSON, in: the optional devices' sections - the OLED display
// (spec 031) and the battery (spec 032). The rest is in ConfigJsonIn.cpp.

namespace SQM::ConfigDetail
{
    namespace
    {
        // Display pages: 1..7 known names, no repeats, in display order. A typo
        // is rejected, not dropped (spec 031, contracts/config.md).
        bool parseDisplayPages(JsonVariant value, DisplayConfig &d)
        {
            if (!value.is<JsonArray>())
                return false;
            JsonArray list = value.as<JsonArray>();
            if (list.size() == 0 || list.size() > Display::PAGE_COUNT)
                return false;
            Display::Page pages[Display::PAGE_COUNT] = {};
            uint8_t count = 0;
            for (JsonVariant item : list)
            {
                Display::Page page = Display::Page::Sky;
                if (!item.is<const char *>() || !Display::parsePage(item.as<const char *>(), page))
                    return false;
                for (uint8_t i = 0; i < count; ++i)
                    if (pages[i] == page)
                        return false;
                pages[count++] = page;
            }
            for (uint8_t i = 0; i < count; ++i)
                d.pages[i] = pages[i];
            d.pageCount = count;
            return true;
        }

    } // namespace

    bool applyDisplay(const Section &display, DisplayConfig &d, std::string *error)
    {
        display.take("enabled", d.enabled);
        if (display.has("pages") && !parseDisplayPages(display["pages"], d))
            return setError(error, "Display: choose 1 to 7 different pages");
        display.take("cycleSeconds", d.cycleSeconds);
        if (display.has("brightness"))
        {
            JsonVariant value = display["brightness"];
            if (!Display::parseBrightness(value.is<const char *>() ? value.as<const char *>() : nullptr, d.brightness))
                return setError(error, "Display: brightness must be low, medium or high");
        }
        display.take("timeoutSeconds", d.timeoutSeconds);
        display.take("wakeOnBoot", d.wakeOnBoot);
        display.take("stayOnAtNight", d.stayOnAtNight);
        display.take("rotate180", d.rotate180);
        display.take("buttonPin", d.buttonPin);
        return true;
    }

    void applyBattery(const Section &battery, BatteryConfig &b)
    {
        battery.take("enabled", b.enabled);
        battery.take("lowPercent", b.lowPercent);
        battery.take("criticalPercent", b.criticalPercent);
        battery.take("shutdownPercent", b.shutdownPercent);
        battery.take("capacityMah", b.capacityMah);
        battery.take("powerSaving", b.powerSaving);
        battery.take("saveReadIntervalSeconds", b.saveReadIntervalSeconds);
        battery.take("shutdownGraceSeconds", b.shutdownGraceSeconds);
        battery.take("alertPin", b.alertPin);
        battery.take("stat1Pin", b.stat1Pin);
        battery.take("stat2Pin", b.stat2Pin);
        battery.take("pgPin", b.pgPin);
    }

} // namespace SQM::ConfigDetail
