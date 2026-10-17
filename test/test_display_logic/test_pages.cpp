#include <unity.h>

#include <cstring>
#include <functional>
#include <string>

#include "DisplayFrame.h"

// Each page's layout (contracts/screen.md): the rendered frame must equal one
// drawn by hand from the contract's text at the layout positions.

using namespace SQM::Display;

namespace
{
    uint8_t actualBuf[FRAME_BYTES];
    uint8_t expectedBuf[FRAME_BYTES];

    using Draw = std::function<void(Frame &, int dx, int dy)>;

    void expectPage(const View &view, Page page, bool night, uint8_t shift, const Draw &draw, const char *what)
    {
        Frame actual(actualBuf);
        render(actual, page, view, night, shift);
        Frame expected(expectedBuf);
        expected.clear();
        const Shift s = shiftOffset(shift);
        draw(expected, s.dx, s.dy);
        TEST_ASSERT_EQUAL_MEMORY_MESSAGE(expectedBuf, actualBuf, FRAME_BYTES, what);
    }

    int centred(const char *text)
    {
        return (WIDTH - Frame::largeWidth(text)) / 2;
    }

    void drawNight(Frame &f, int dx, int dy, const char *text)
    {
        f.drawLarge(centred(text) + dx, Layout::NIGHT_Y + dy, text);
    }

    bool anyInverse(const uint8_t *buf)
    {
        // A filled block: some byte column fully lit across a whole page.
        for (size_t i = 0; i < FRAME_BYTES; ++i)
            if (buf[i] == 0xFF)
                return true;
        return false;
    }

    View skyView()
    {
        View v;
        v.sky.available = true;
        v.sky.valid = true;
        v.sky.sqm = 21.344F;
        v.sky.bortle = 3;
        v.sky.nelm = 6.42F;
        v.sky.label = "SQM";
        return v;
    }

    View fullView()
    {
        View v = skyView();
        v.environment = {true, true, 12.44F, 78.4F, 8.61F};
        v.cloud.available = v.cloud.valid = true;
        v.cloud.skyTemperatureC = -18.24F;
        v.cloud.condition = "clear";
        v.safety.known = true;
        v.safety.safe = true;
        v.gps.available = v.gps.valid = v.gps.fix = true;
        v.gps.satellites = 9;
        v.gps.latitude = 51.50741;
        v.gps.longitude = -0.12781;
        v.battery.available = v.battery.valid = true;
        v.battery.percent = 82;
        v.battery.state = "Chg";
        std::strcpy(v.network.name, "Observatory");
        v.network.mode = View::NetMode::Wifi;
        std::strcpy(v.network.ip, "192.168.1.128");
        return v;
    }
} // namespace

// --- US1: the Sky page ---

void test_sky_day_layout(void)
{
    const View v = skyView();
    expectPage(
        v,
        Page::Sky,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "SQM mag");
            f.drawLarge(dx, Layout::LARGE_Y + dy, "21.34");
            f.drawText(dx, Layout::LINE_A_Y + dy, "Bortle 3  NELM 6.4");
        },
        "sky day");
}

void test_sky_names_the_authoritative_sensor_and_shows_the_other(void)
{
    View v = skyView();
    v.sky.label = "SQM-L";
    v.sky.other.present = true;
    v.sky.other.valid = true;
    v.sky.other.sqm = 21.1F;
    v.sky.other.label = "SQM";
    expectPage(
        v,
        Page::Sky,
        false,
        2,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "SQM-L mag");
            f.drawLarge(dx, Layout::LARGE_Y + dy, "21.34");
            f.drawText(dx, Layout::LINE_A_Y + dy, "Bortle 3  NELM 6.4");
            f.drawText(dx, Layout::LINE_B_Y + dy, "SQM 21.10");
        },
        "sky with the other reading, shifted");

    // The other reading only when it reports.
    v.sky.other.valid = false;
    expectPage(
        v,
        Page::Sky,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "SQM-L mag");
            f.drawLarge(dx, Layout::LARGE_Y + dy, "21.34");
            f.drawText(dx, Layout::LINE_A_Y + dy, "Bortle 3  NELM 6.4");
        },
        "other invalid: not shown");
}

void test_sky_invalid_shows_no_data_marker(void)
{
    View v = skyView();
    v.sky.valid = false; // missing, faulted or stale
    expectPage(
        v,
        Page::Sky,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "SQM mag");
            f.drawLarge(dx, Layout::LARGE_Y + dy, "--");
            f.drawText(dx, Layout::LINE_A_Y + dy, "Bortle --  NELM --");
        },
        "sky invalid");
}

// --- US3: night layouts ---

void test_night_layouts_one_large_value(void)
{
    View v = fullView();
    const struct
    {
        Page page;
        const char *text;
    } cases[] = {
        {Page::Sky, "21.34"},
        {Page::Environment, "12.4\xC2\xB0"
                            "C"},
        {Page::Cloud, "-18.2\xC2\xB0"
                      "C"},
        {Page::Safety, "Safe"},
        {Page::Gps, "9"},
        {Page::Battery, "82%"},
        {Page::Network, ".1.128"},
    };
    for (const auto &c : cases)
    {
        const char *text = c.text;
        expectPage(
            v, c.page, true, 1, [text](Frame &f, int dx, int dy) { drawNight(f, dx, dy, text); }, pageName(c.page));
    }
}

void test_night_has_no_inverse_and_shows_no_data_marker(void)
{
    View v = fullView();
    v.safety.safe = false;
    std::strcpy(v.safety.firstReason, "SQM 18.21 < 19.50");
    expectPage(v, Page::Safety, true, 0, [](Frame &f, int dx, int dy) { drawNight(f, dx, dy, "UNSAFE"); }, "unsafe at night");
    TEST_ASSERT_FALSE(anyInverse(actualBuf));

    v.sky.valid = false;
    expectPage(v, Page::Sky, true, 0, [](Frame &f, int dx, int dy) { drawNight(f, dx, dy, "--"); }, "sky invalid at night");
    v.network.mode = View::NetMode::Ap;
    std::strcpy(v.network.ip, "192.168.4.1");
    expectPage(v, Page::Network, true, 0, [](Frame &f, int dx, int dy) { drawNight(f, dx, dy, "AP"); }, "AP at night");
    v.network.mode = View::NetMode::None;
    v.network.ip[0] = '\0';
    expectPage(v, Page::Network, true, 0, [](Frame &f, int dx, int dy) { drawNight(f, dx, dy, "--"); }, "no network at night");
}

// --- US2: the other day layouts ---

void test_environment_and_cloud_day(void)
{
    View v = fullView();
    expectPage(
        v,
        Page::Environment,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "Temp");
            f.drawLarge(dx, Layout::LARGE_Y + dy, "12.4\xC2\xB0"
                                                  "C");
            f.drawText(dx, Layout::LINE_A_Y + dy, "Hum 78%  Dew 8.6\xC2\xB0"
                                                  "C");
        },
        "environment");
    expectPage(
        v,
        Page::Cloud,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "Sky");
            f.drawLarge(dx, Layout::LARGE_Y + dy, "-18.2\xC2\xB0"
                                                  "C");
            f.drawText(dx, Layout::LINE_A_Y + dy, "Cloud clear");
        },
        "cloud");
    v.environment.valid = false;
    v.cloud.valid = false;
    expectPage(
        v,
        Page::Environment,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "Temp");
            f.drawLarge(dx, Layout::LARGE_Y + dy, "--");
            f.drawText(dx, Layout::LINE_A_Y + dy, "Hum --  Dew --");
        },
        "environment invalid");
    expectPage(
        v,
        Page::Cloud,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "Sky");
            f.drawLarge(dx, Layout::LARGE_Y + dy, "--");
            f.drawText(dx, Layout::LINE_A_Y + dy, "Cloud --");
        },
        "cloud invalid");
}

void test_safety_day(void)
{
    View v = fullView();
    expectPage(
        v, Page::Safety, false, 0, [](Frame &f, int dx, int dy) { f.drawLarge(dx, Layout::LARGE_Y + dy, "Safe"); }, "safe");

    v.safety.safe = false;
    std::strcpy(v.safety.firstReason, "Raining (intensity 0.25 mm/h, limit any rain)");
    expectPage(
        v,
        Page::Safety,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            const int pad = Layout::INVERSE_PAD;
            f.fillRect(dx, Layout::LARGE_Y + dy - pad, Frame::largeWidth("UNSAFE") + 2 * pad, LARGE_H + 2 * pad, true);
            f.drawLarge(dx + pad, Layout::LARGE_Y + dy, "UNSAFE", false);
            f.drawText(dx, Layout::LINE_A_Y + dy, "Raining (intensity 0.25 mm/h, limit any rain)", WIDTH - dx);
        },
        "unsafe by day: inverse block, first reason cut");
    // The reason is cut with the ellipsis glyph within the line.
    Frame check(actualBuf);
    for (int y = Layout::LINE_A_Y; y < Layout::LINE_A_Y + LINE_H; ++y)
        TEST_ASSERT_FALSE(check.pixel(127, y));

    v.safety.known = false;
    expectPage(
        v,
        Page::Safety,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawLarge(dx, Layout::LARGE_Y + dy, "--");
            f.drawText(dx, Layout::LINE_A_Y + dy, "--");
        },
        "safety unknown");
}

void test_gps_and_battery_day(void)
{
    View v = fullView();
    expectPage(
        v,
        Page::Gps,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "GPS");
            f.drawLarge(dx, Layout::LARGE_Y + dy, "Fix");
            f.drawText(dx, Layout::LINE_A_Y + dy, "Sats 9");
            f.drawText(dx, Layout::LINE_B_Y + dy, "51.5074 -0.1278");
        },
        "gps fix");
    v.gps.fix = false;
    v.gps.satellites = 2;
    expectPage(
        v,
        Page::Gps,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "GPS");
            f.drawLarge(dx, Layout::LARGE_Y + dy, "No fix");
            f.drawText(dx, Layout::LINE_A_Y + dy, "Sats 2");
            f.drawText(dx, Layout::LINE_B_Y + dy, "-- --");
        },
        "gps no fix");
    v.gps.valid = false;
    expectPage(
        v,
        Page::Gps,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "GPS");
            f.drawLarge(dx, Layout::LARGE_Y + dy, "--");
            f.drawText(dx, Layout::LINE_A_Y + dy, "Sats --");
            f.drawText(dx, Layout::LINE_B_Y + dy, "-- --");
        },
        "gps invalid");

    expectPage(
        v,
        Page::Battery,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "Batt");
            f.drawLarge(dx, Layout::LARGE_Y + dy, "82%");
            f.drawText(dx, Layout::LINE_A_Y + dy, "Chg");
        },
        "battery");
    v.battery.valid = false;
    expectPage(
        v,
        Page::Battery,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "Batt");
            f.drawLarge(dx, Layout::LARGE_Y + dy, "--");
            f.drawText(dx, Layout::LINE_A_Y + dy, "--");
        },
        "battery invalid");
}

void test_network_day(void)
{
    View v = fullView();
    expectPage(
        v,
        Page::Network,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "Observatory");
            f.drawText(dx, Layout::NET_A_Y + dy, "WiFi");
            f.drawText(dx, Layout::NET_B_Y + dy, "IP 192.168.1.128");
        },
        "wifi");
    v.network.mode = View::NetMode::Ap;
    std::strcpy(v.network.apName, "SQMeter-Setup");
    std::strcpy(v.network.ip, "192.168.4.1");
    std::strcpy(v.network.name, "A very long device name, longer");
    expectPage(
        v,
        Page::Network,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "A very long device name, longer", WIDTH - dx);
            f.drawText(dx, Layout::NET_A_Y + dy, "AP SQMeter-Setup");
            f.drawText(dx, Layout::NET_B_Y + dy, "IP 192.168.4.1");
        },
        "setup hotspot, long name cut");
    v.network.mode = View::NetMode::None;
    v.network.ip[0] = '\0';
    expectPage(
        v,
        Page::Network,
        false,
        0,
        [](Frame &f, int dx, int dy)
        {
            f.drawText(dx, Layout::TITLE_Y + dy, "A very long device name, longer", WIDTH - dx);
            f.drawText(dx, Layout::NET_A_Y + dy, "WiFi --");
        },
        "no connection");
}

void test_page_availability(void)
{
    View v;
    TEST_ASSERT_FALSE(v.available(Page::Sky));
    TEST_ASSERT_FALSE(v.available(Page::Environment));
    TEST_ASSERT_FALSE(v.available(Page::Cloud));
    TEST_ASSERT_TRUE(v.available(Page::Safety));
    TEST_ASSERT_FALSE(v.available(Page::Gps));
    TEST_ASSERT_FALSE(v.available(Page::Battery));
    TEST_ASSERT_TRUE(v.available(Page::Network));
    v = fullView();
    for (size_t i = 0; i < PAGE_COUNT; ++i)
        TEST_ASSERT_TRUE(v.available(static_cast<Page>(i)));
}

void run_page_tests(void)
{
    RUN_TEST(test_sky_day_layout);
    RUN_TEST(test_sky_names_the_authoritative_sensor_and_shows_the_other);
    RUN_TEST(test_sky_invalid_shows_no_data_marker);
    RUN_TEST(test_night_layouts_one_large_value);
    RUN_TEST(test_night_has_no_inverse_and_shows_no_data_marker);
    RUN_TEST(test_environment_and_cloud_day);
    RUN_TEST(test_safety_day);
    RUN_TEST(test_gps_and_battery_day);
    RUN_TEST(test_network_day);
    RUN_TEST(test_page_availability);
}
