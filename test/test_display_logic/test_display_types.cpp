#include <unity.h>

#include "DisplayTypes.h"

using namespace SQM::Display;

void test_page_names_round_trip(void)
{
    const char *expected[] = {"sky", "environment", "cloud", "safety", "gps", "battery", "network"};
    TEST_ASSERT_EQUAL(7, PAGE_COUNT);
    for (size_t i = 0; i < PAGE_COUNT; ++i)
    {
        const Page page = static_cast<Page>(i);
        TEST_ASSERT_EQUAL_STRING(expected[i], pageName(page));
        Page parsed = Page::Network;
        TEST_ASSERT_TRUE(parsePage(expected[i], parsed));
        TEST_ASSERT_EQUAL(static_cast<int>(page), static_cast<int>(parsed));
    }
}

void test_parse_page_exact_names_only(void)
{
    Page page = Page::Gps;
    TEST_ASSERT_FALSE(parsePage("Sky", page));
    TEST_ASSERT_FALSE(parsePage("sky ", page));
    TEST_ASSERT_FALSE(parsePage("", page));
    TEST_ASSERT_FALSE(parsePage(nullptr, page));
    TEST_ASSERT_FALSE(parsePage("wind", page));
    TEST_ASSERT_EQUAL(static_cast<int>(Page::Gps), static_cast<int>(page));
}

void test_brightness_names(void)
{
    TEST_ASSERT_EQUAL_STRING("low", brightnessName(Brightness::Low));
    TEST_ASSERT_EQUAL_STRING("medium", brightnessName(Brightness::Medium));
    TEST_ASSERT_EQUAL_STRING("high", brightnessName(Brightness::High));
    Brightness b = Brightness::Low;
    TEST_ASSERT_TRUE(parseBrightness("high", b));
    TEST_ASSERT_EQUAL(static_cast<int>(Brightness::High), static_cast<int>(b));
    TEST_ASSERT_FALSE(parseBrightness("max", b));
    TEST_ASSERT_FALSE(parseBrightness("High", b));
    TEST_ASSERT_EQUAL(static_cast<int>(Brightness::High), static_cast<int>(b));
}

void run_display_types_tests(void)
{
    RUN_TEST(test_page_names_round_trip);
    RUN_TEST(test_parse_page_exact_names_only);
    RUN_TEST(test_brightness_names);
}
