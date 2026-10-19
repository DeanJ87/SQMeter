#include "DisplayFrame.h"

#include <cmath>
#include <cstdio>
#include <cstring>

// Page layouts (contracts/screen.md). Labels are fixed English (FR-011);
// `--` stands in for every value that is missing, faulted or stale (FR-005).

namespace SQM::Display
{
    namespace
    {
        constexpr const char *NO_DATA = "--";
        constexpr const char *DEG_C = "\xC2\xB0"
                                      "C";

        struct Text
        {
            char s[48];
        };

        Text format(const char *fmt, double value)
        {
            Text t{};
            std::snprintf(t.s, sizeof t.s, fmt, value);
            return t;
        }

        bool usable(bool valid, double value)
        {
            return valid && std::isfinite(value);
        }

        Text valueOr(bool valid, const char *fmt, double value)
        {
            if (!usable(valid, value))
            {
                Text t{};
                std::snprintf(t.s, sizeof t.s, "%s", NO_DATA);
                return t;
            }
            return format(fmt, value);
        }

        Text celsius(bool valid, double value)
        {
            Text t = valueOr(valid, "%.1f", value);
            if (usable(valid, value))
                std::strncat(t.s, DEG_C, sizeof t.s - std::strlen(t.s) - 1);
            return t;
        }

        // Positions shifted by the burn-in offset.
        struct Pen
        {
            Frame &f;
            int dx;
            int dy;

            void small(int y, const char *text) const { f.drawText(dx, y + dy, text, WIDTH - dx); }
            void large(const char *text) const { f.drawLarge(dx, Layout::LARGE_Y + dy, text); }
            void night(const char *text) const { f.drawLarge((WIDTH - Frame::largeWidth(text)) / 2 + dx, Layout::NIGHT_Y + dy, text); }
        };

        // ".1.128" from "192.168.1.128": the end of the address, which tells
        // devices on one network apart.
        Text lastOctets(const char *ip)
        {
            Text t{};
            const char *third = nullptr;
            int dots = 0;
            for (const char *p = ip; *p != '\0'; ++p)
                if (*p == '.' && ++dots == 2)
                    third = p;
            std::snprintf(t.s, sizeof t.s, "%s", dots == 3 && third != nullptr ? third : NO_DATA);
            return t;
        }

        void drawSky(const Pen &pen, const View::Sky &sky, bool night)
        {
            const Text sqm = valueOr(sky.valid, "%.2f", sky.sqm);
            if (night)
            {
                pen.night(sqm.s);
                return;
            }
            Text title{};
            std::snprintf(title.s, sizeof title.s, "%s mag", sky.label);
            pen.small(Layout::TITLE_Y, title.s);
            pen.large(sqm.s);
            const Text bortle = sky.valid && sky.bortle > 0 ? format("%.0f", sky.bortle) : valueOr(false, "", 0.0);
            const Text nelm = valueOr(sky.valid, "%.1f", sky.nelm);
            Text line{};
            std::snprintf(line.s, sizeof line.s, "Bortle %s  NELM %s", bortle.s, nelm.s);
            pen.small(Layout::LINE_A_Y, line.s);
            if (sky.other.present && usable(sky.other.valid, sky.other.sqm))
            {
                Text other{};
                std::snprintf(other.s, sizeof other.s, "%s %.2f", sky.other.label, static_cast<double>(sky.other.sqm));
                pen.small(Layout::LINE_B_Y, other.s);
            }
        }

        void drawEnvironment(const Pen &pen, const View::Environment &env, bool night)
        {
            const Text temp = celsius(env.valid, env.temperatureC);
            if (night)
            {
                pen.night(temp.s);
                return;
            }
            pen.small(Layout::TITLE_Y, "Temp");
            pen.large(temp.s);
            Text hum = valueOr(env.valid, "%.0f", env.humidity);
            if (usable(env.valid, env.humidity))
                std::strncat(hum.s, "%", sizeof hum.s - std::strlen(hum.s) - 1);
            const Text dew = celsius(env.valid, env.dewpointC);
            Text line{};
            std::snprintf(line.s, sizeof line.s, "Hum %s  Dew %s", hum.s, dew.s);
            pen.small(Layout::LINE_A_Y, line.s);
        }

        void drawCloud(const Pen &pen, const View::Cloud &cloud, bool night)
        {
            const Text sky = celsius(cloud.valid, cloud.skyTemperatureC);
            if (night)
            {
                pen.night(sky.s);
                return;
            }
            pen.small(Layout::TITLE_Y, "Sky");
            pen.large(sky.s);
            Text line{};
            const bool known = cloud.valid && cloud.condition != nullptr && cloud.condition[0] != '\0';
            std::snprintf(line.s, sizeof line.s, "Cloud %s", known ? cloud.condition : NO_DATA);
            pen.small(Layout::LINE_A_Y, line.s);
        }

        void drawSafety(const Pen &pen, const View::Safety &safety, bool night)
        {
            const char *verdict = !safety.known ? NO_DATA : safety.safe ? "Safe" : "UNSAFE";
            if (night)
            {
                pen.night(verdict); // plain text: no lit block at night
                return;
            }
            if (safety.known && !safety.safe)
            {
                const int pad = Layout::INVERSE_PAD;
                pen.f.fillRect(pen.dx, Layout::LARGE_Y + pen.dy - pad, Frame::largeWidth(verdict) + 2 * pad, LARGE_H + 2 * pad, true);
                pen.f.drawLarge(pen.dx + pad, Layout::LARGE_Y + pen.dy, verdict, false);
                pen.small(Layout::LINE_A_Y, safety.firstReason);
                return;
            }
            pen.large(verdict);
            if (!safety.known)
                pen.small(Layout::LINE_A_Y, NO_DATA);
        }

        void drawGps(const Pen &pen, const View::Gps &gps, bool night)
        {
            const Text sats = gps.valid ? format("%.0f", gps.satellites) : valueOr(false, "", 0.0);
            if (night)
            {
                pen.night(sats.s);
                return;
            }
            pen.small(Layout::TITLE_Y, "GPS");
            pen.large(!gps.valid ? NO_DATA : gps.fix ? "Fix" : "No fix");
            Text line{};
            std::snprintf(line.s, sizeof line.s, "Sats %s", sats.s);
            pen.small(Layout::LINE_A_Y, line.s);
            Text where{};
            if (gps.valid && gps.fix && std::isfinite(gps.latitude) && std::isfinite(gps.longitude))
                std::snprintf(where.s, sizeof where.s, "%.4f %.4f", gps.latitude, gps.longitude);
            else
                std::snprintf(where.s, sizeof where.s, "%s %s", NO_DATA, NO_DATA);
            pen.small(Layout::LINE_B_Y, where.s);
        }

        void drawBattery(const Pen &pen, const View::Battery &battery, bool night)
        {
            Text percent = battery.valid ? format("%.0f", battery.percent) : valueOr(false, "", 0.0);
            if (battery.valid)
                std::strncat(percent.s, "%", sizeof percent.s - std::strlen(percent.s) - 1);
            if (night)
            {
                pen.night(percent.s);
                return;
            }
            pen.small(Layout::TITLE_Y, "Batt");
            pen.large(percent.s);
            const bool stateKnown = battery.valid && battery.state != nullptr && battery.state[0] != '\0';
            pen.small(Layout::LINE_A_Y, stateKnown ? battery.state : NO_DATA);
        }

        void drawNetwork(const Pen &pen, const View::Network &net, bool night)
        {
            const bool haveIp = net.mode != View::NetMode::None && net.ip[0] != '\0';
            if (night)
            {
                if (net.mode == View::NetMode::Ap)
                    pen.night("AP");
                else
                    pen.night(haveIp ? lastOctets(net.ip).s : NO_DATA);
                return;
            }
            pen.small(Layout::TITLE_Y, net.name);
            Text mode{};
            if (net.mode == View::NetMode::Ap)
                std::snprintf(mode.s, sizeof mode.s, "AP %s", net.apName);
            else
                std::snprintf(mode.s, sizeof mode.s, "%s", net.mode == View::NetMode::Wifi ? "WiFi" : "WiFi --");
            pen.small(Layout::NET_A_Y, mode.s);
            if (haveIp)
            {
                Text ip{};
                std::snprintf(ip.s, sizeof ip.s, "IP %s", net.ip);
                pen.small(Layout::NET_B_Y, ip.s);
            }
        }
    } // namespace

    bool View::available(Page page) const
    {
        switch (page)
        {
        case Page::Sky:
            return sky.available;
        case Page::Environment:
            return environment.available;
        case Page::Cloud:
            return cloud.available;
        case Page::Gps:
            return gps.available;
        case Page::Battery:
            return battery.available;
        case Page::Safety:
        case Page::Network:
            return true;
        }
        return false;
    }

    void render(Frame &frame, Page page, const View &view, bool nightMode, uint8_t shift)
    {
        frame.clear();
        const Shift s = shiftOffset(shift);
        const Pen pen{frame, s.dx, s.dy};
        switch (page)
        {
        case Page::Sky:
            drawSky(pen, view.sky, nightMode);
            break;
        case Page::Environment:
            drawEnvironment(pen, view.environment, nightMode);
            break;
        case Page::Cloud:
            drawCloud(pen, view.cloud, nightMode);
            break;
        case Page::Safety:
            drawSafety(pen, view.safety, nightMode);
            break;
        case Page::Gps:
            drawGps(pen, view.gps, nightMode);
            break;
        case Page::Battery:
            drawBattery(pen, view.battery, nightMode);
            break;
        case Page::Network:
            drawNetwork(pen, view.network, nightMode);
            break;
        }
        frame.commit();
    }
} // namespace SQM::Display
