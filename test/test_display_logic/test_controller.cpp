#include <unity.h>

#include "DisplayController.h"

// The power, night and page state machine (research D5/D6, data-model.md).

using namespace SQM::Display;

namespace
{
    constexpr uint8_t ALL_PAGES = 0x7F;

    Settings on()
    {
        Settings s;
        s.enabled = true;
        return s;
    }

    Night day()
    {
        return {true, false};
    }
    Night night()
    {
        return {true, true};
    }
    Night unknown()
    {
        return {false, false};
    }

    Inputs at(uint32_t now, Night n = day(), uint8_t available = ALL_PAGES)
    {
        Inputs in;
        in.nowMs = now;
        in.detected = true;
        in.night = n;
        in.availableMask = available;
        return in;
    }

    uint8_t mask(std::initializer_list<Page> pages)
    {
        uint8_t m = 0;
        for (Page p : pages)
            m = static_cast<uint8_t>(m | (1U << static_cast<unsigned>(p)));
        return m;
    }
} // namespace

// --- US3: power, timeout and night ---

void test_wakes_on_boot_for_the_timeout(void)
{
    Controller c;
    const Settings s = on(); // 60 s timeout, wake on boot
    Output out = c.tick(s, at(1000));
    TEST_ASSERT_TRUE(out.on);
    TEST_ASSERT_TRUE(out.page == Page::Sky);
    out = c.tick(s, at(60999));
    TEST_ASSERT_TRUE(out.on);
    out = c.tick(s, at(61000));
    TEST_ASSERT_FALSE(out.on);
    out = c.tick(s, at(200000));
    TEST_ASSERT_FALSE(out.on);
}

void test_no_wake_on_boot_when_turned_off(void)
{
    Controller c;
    Settings s = on();
    s.wakeOnBoot = false;
    TEST_ASSERT_FALSE(c.tick(s, at(1000)).on);
}

void test_timeout_never_by_day_but_sixty_seconds_at_night(void)
{
    Controller c;
    Settings s = on();
    s.timeoutSeconds = 0;
    TEST_ASSERT_TRUE(c.tick(s, at(0)).on);
    TEST_ASSERT_TRUE(c.tick(s, at(10 * 60 * 60 * 1000U)).on); // ten hours by day
    // Dusk: the never-timeout becomes 60 s from the last wake, so it goes off at once.
    TEST_ASSERT_FALSE(c.tick(s, at(10 * 60 * 60 * 1000U + 10, night())).on);

    // A wake at night lasts 60 s.
    Controller n;
    Output out = n.tick(s, at(0, night()));
    TEST_ASSERT_TRUE(out.on);
    TEST_ASSERT_TRUE(n.tick(s, at(59999, night())).on);
    TEST_ASSERT_FALSE(n.tick(s, at(60000, night())).on);
}

void test_unknown_night_behaves_as_night(void)
{
    Controller c;
    Settings s = on();
    s.timeoutSeconds = 0;
    s.stayOnAtNight = true; // needs a known night: no effect here
    Output out = c.tick(s, at(0, unknown()));
    TEST_ASSERT_TRUE(out.on);
    TEST_ASSERT_TRUE(out.nightMode);
    TEST_ASSERT_EQUAL_HEX8(0x00, out.contrast);
    TEST_ASSERT_FALSE(c.tick(s, at(60000, unknown())).on);
}

void test_stay_on_at_night_only_when_night_is_known(void)
{
    Controller c;
    Settings s = on();
    s.stayOnAtNight = true;
    Output out = c.tick(s, at(0, night()));
    TEST_ASSERT_TRUE(out.on);
    TEST_ASSERT_TRUE(out.nightMode);
    TEST_ASSERT_TRUE(c.tick(s, at(5 * 60 * 60 * 1000U, night())).on);
    // Dawn: the day timeout applies again (60 s since the last wake: off).
    TEST_ASSERT_FALSE(c.tick(s, at(5 * 60 * 60 * 1000U + 10, day())).on);
}

void test_contrast_by_brightness_and_night(void)
{
    const struct
    {
        Brightness b;
        uint8_t contrast;
    } cases[] = {{Brightness::Low, 0x10}, {Brightness::Medium, 0x7F}, {Brightness::High, 0xFF}};
    for (const auto &k : cases)
    {
        Controller c;
        Settings s = on();
        s.brightness = k.b;
        Output out = c.tick(s, at(0));
        TEST_ASSERT_FALSE(out.nightMode);
        TEST_ASSERT_EQUAL_HEX8(k.contrast, out.contrast);
        out = c.tick(s, at(10, night()));
        TEST_ASSERT_TRUE(out.nightMode);
        TEST_ASSERT_EQUAL_HEX8(0x00, out.contrast);
    }
}

void test_off_when_disabled_or_not_detected_or_forced_off(void)
{
    Controller c;
    Settings s = on();
    TEST_ASSERT_TRUE(c.tick(s, at(0)).on);
    Inputs lost = at(10);
    lost.detected = false;
    TEST_ASSERT_FALSE(c.tick(s, lost).on);
    Settings off = s;
    off.enabled = false;
    TEST_ASSERT_FALSE(c.tick(off, at(20)).on);
    Inputs forced = at(30);
    forced.forcedOff = true;
    forced.wake = true;
    TEST_ASSERT_FALSE(c.tick(s, forced).on);
}

void test_rotation_passes_through(void)
{
    Controller c;
    Settings s = on();
    s.rotate180 = true;
    TEST_ASSERT_TRUE(c.tick(s, at(0)).rotate180);
}

void test_millis_wrap(void)
{
    Controller c;
    const Settings s = on();
    TEST_ASSERT_TRUE(c.tick(s, at(0xFFFFFFF0U)).on);
    TEST_ASSERT_TRUE(c.tick(s, at(1000)).on); // wrapped, ~1 s later
    TEST_ASSERT_FALSE(c.tick(s, at(60000)).on);
}

// --- US2: pages ---

void test_cycles_selected_available_pages_in_order(void)
{
    Controller c;
    Settings s = on();
    s.timeoutSeconds = 0;
    s.pageCount = 3;
    s.pages[0] = Page::Network;
    s.pages[1] = Page::Environment; // not available below
    s.pages[2] = Page::Sky;
    const uint8_t avail = mask({Page::Sky, Page::Safety, Page::Network});
    Output out = c.tick(s, at(0, day(), avail));
    TEST_ASSERT_TRUE(out.page == Page::Network);
    TEST_ASSERT_EQUAL(0, out.shift);
    out = c.tick(s, at(4999, day(), avail));
    TEST_ASSERT_TRUE(out.page == Page::Network);
    out = c.tick(s, at(5000, day(), avail));
    TEST_ASSERT_TRUE(out.page == Page::Sky);
    TEST_ASSERT_EQUAL(1, out.shift);
    out = c.tick(s, at(10000, day(), avail));
    TEST_ASSERT_TRUE(out.page == Page::Network);
    TEST_ASSERT_EQUAL(2, out.shift);
    // The interval is the setting.
    s.cycleSeconds = 20;
    TEST_ASSERT_TRUE(c.tick(s, at(29999, day(), avail)).page == Page::Network);
    TEST_ASSERT_TRUE(c.tick(s, at(30000, day(), avail)).page == Page::Sky);
}

void test_one_visible_page_does_not_cycle(void)
{
    Controller c;
    Settings s = on();
    s.timeoutSeconds = 0;
    Output out = c.tick(s, at(0));
    for (uint32_t t = 0; t < 60000; t += 1000)
    {
        out = c.tick(s, at(t));
        TEST_ASSERT_TRUE(out.page == Page::Sky);
        TEST_ASSERT_EQUAL(0, out.shift);
    }
}

void test_no_visible_page_falls_back_to_network(void)
{
    Controller c;
    Settings s = on();
    s.pages[0] = Page::Battery;
    const Output out = c.tick(s, at(0, day(), mask({Page::Safety, Page::Network})));
    TEST_ASSERT_TRUE(out.on);
    TEST_ASSERT_TRUE(out.page == Page::Network);
}

void test_page_becoming_unavailable_is_skipped(void)
{
    Controller c;
    Settings s = on();
    s.timeoutSeconds = 0;
    s.pageCount = 3;
    s.pages[0] = Page::Sky;
    s.pages[1] = Page::Gps;
    s.pages[2] = Page::Safety;
    TEST_ASSERT_TRUE(c.tick(s, at(0)).page == Page::Sky);
    const uint8_t noGps = mask({Page::Sky, Page::Safety, Page::Network});
    TEST_ASSERT_TRUE(c.tick(s, at(5000, day(), noGps)).page == Page::Safety);
    TEST_ASSERT_TRUE(c.tick(s, at(10000, day(), noGps)).page == Page::Sky);
}

// --- US4: wake, blank and the button ---

void test_wake_request_turns_on_for_the_timeout(void)
{
    Controller c;
    Settings s = on();
    s.wakeOnBoot = false;
    TEST_ASSERT_FALSE(c.tick(s, at(0)).on);
    Inputs wake = at(1000);
    wake.wake = true;
    TEST_ASSERT_TRUE(c.tick(s, wake).on);
    TEST_ASSERT_TRUE(c.tick(s, at(60999)).on);
    TEST_ASSERT_FALSE(c.tick(s, at(61000)).on);
}

void test_blank_turns_off_until_the_next_wake(void)
{
    Controller c;
    Settings s = on();
    s.timeoutSeconds = 0;
    TEST_ASSERT_TRUE(c.tick(s, at(0)).on);
    Inputs blank = at(100);
    blank.blank = true;
    TEST_ASSERT_FALSE(c.tick(s, blank).on);
    TEST_ASSERT_FALSE(c.tick(s, at(100000)).on);
    Inputs wake = at(200000);
    wake.wake = true;
    TEST_ASSERT_TRUE(c.tick(s, wake).on);
}

void test_button_wakes_then_advances_and_restarts_the_timeout(void)
{
    Controller c;
    Settings s = on();
    s.wakeOnBoot = false;
    s.pageCount = 2;
    s.pages[0] = Page::Sky;
    s.pages[1] = Page::Safety;
    s.cycleSeconds = 60;
    TEST_ASSERT_FALSE(c.tick(s, at(0)).on);
    Inputs press = at(1000);
    press.button = true;
    Output out = c.tick(s, press);
    TEST_ASSERT_TRUE(out.on);
    TEST_ASSERT_TRUE(out.page == Page::Sky);
    press.nowMs = 50000;
    out = c.tick(s, press);
    TEST_ASSERT_TRUE(out.page == Page::Safety);
    TEST_ASSERT_EQUAL(1, out.shift);
    // The timeout restarted at 50 s.
    TEST_ASSERT_TRUE(c.tick(s, at(109999)).on);
    TEST_ASSERT_FALSE(c.tick(s, at(110000)).on);
}

void test_requests_ignored_while_unavailable(void)
{
    Controller c;
    Settings off;
    Inputs wake = at(0);
    wake.wake = true;
    wake.button = true;
    TEST_ASSERT_FALSE(c.tick(off, wake).on);
    Inputs lost = wake;
    lost.detected = false;
    TEST_ASSERT_FALSE(c.tick(on(), lost).on);
}

// Unplugged (or three failed writes on a glitchy bus) while lit: lit again
// when found, without a wake (spec 031 edge cases, quickstart 4.5).
void test_resumes_when_detected_again(void)
{
    Controller c;
    Settings s = on();
    s.timeoutSeconds = 0; // never by day
    TEST_ASSERT_TRUE(c.tick(s, at(0)).on);
    Inputs lost = at(1000);
    lost.detected = false;
    TEST_ASSERT_FALSE(c.tick(s, lost).on);
    lost.nowMs = 6000;
    TEST_ASSERT_FALSE(c.tick(s, lost).on);
    TEST_ASSERT_TRUE(c.tick(s, at(11000)).on);

    // Off when it went away: stays off when it's back.
    Controller dark;
    Settings quiet = on();
    quiet.wakeOnBoot = false;
    TEST_ASSERT_FALSE(dark.tick(quiet, at(0)).on);
    Inputs gone = at(1000);
    gone.detected = false;
    dark.tick(quiet, gone);
    TEST_ASSERT_FALSE(dark.tick(quiet, at(2000)).on);
}

void run_controller_tests(void)
{
    RUN_TEST(test_resumes_when_detected_again);
    RUN_TEST(test_wakes_on_boot_for_the_timeout);
    RUN_TEST(test_no_wake_on_boot_when_turned_off);
    RUN_TEST(test_timeout_never_by_day_but_sixty_seconds_at_night);
    RUN_TEST(test_unknown_night_behaves_as_night);
    RUN_TEST(test_stay_on_at_night_only_when_night_is_known);
    RUN_TEST(test_contrast_by_brightness_and_night);
    RUN_TEST(test_off_when_disabled_or_not_detected_or_forced_off);
    RUN_TEST(test_rotation_passes_through);
    RUN_TEST(test_millis_wrap);
    RUN_TEST(test_cycles_selected_available_pages_in_order);
    RUN_TEST(test_one_visible_page_does_not_cycle);
    RUN_TEST(test_no_visible_page_falls_back_to_network);
    RUN_TEST(test_page_becoming_unavailable_is_skipped);
    RUN_TEST(test_wake_request_turns_on_for_the_timeout);
    RUN_TEST(test_blank_turns_off_until_the_next_wake);
    RUN_TEST(test_button_wakes_then_advances_and_restarts_the_timeout);
    RUN_TEST(test_requests_ignored_while_unavailable);
}
