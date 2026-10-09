#include <unity.h>

#include "CaptivePortal.h"

using namespace SQM;
using CaptivePortal::NotFound;

void setUp() {}
void tearDown() {}

void test_no_credentials_opens_hotspot_at_once()
{
    TEST_ASSERT_TRUE(CaptivePortal::shouldOpenHotspot(false, false, 0));
}

void test_saved_network_gets_time_before_hotspot()
{
    TEST_ASSERT_FALSE(CaptivePortal::shouldOpenHotspot(true, false, 10000));
    TEST_ASSERT_FALSE(CaptivePortal::shouldOpenHotspot(true, false, CaptivePortal::FALLBACK_AFTER_MS - 1));
    TEST_ASSERT_TRUE(CaptivePortal::shouldOpenHotspot(true, false, CaptivePortal::FALLBACK_AFTER_MS));
}

void test_connected_never_opens_hotspot()
{
    TEST_ASSERT_FALSE(CaptivePortal::shouldOpenHotspot(true, true, CaptivePortal::FALLBACK_AFTER_MS * 10));
}

void test_home_network_is_never_redirected()
{
    // The bug: while the hotspot was open, /lang.json fetched over the home
    // network went to http://192.168.4.1/wifi.
    TEST_ASSERT_EQUAL(static_cast<int>(NotFound::FileMissing), static_cast<int>(CaptivePortal::notFound("/lang.json", false, false)));
    TEST_ASSERT_EQUAL(static_cast<int>(NotFound::AppPage), static_cast<int>(CaptivePortal::notFound("/settings", false, false)));
    TEST_ASSERT_FALSE(CaptivePortal::probeOpensSetup(false));
}

void test_hotspot_other_sites_go_to_setup()
{
    TEST_ASSERT_EQUAL(static_cast<int>(NotFound::SetupScreen), static_cast<int>(CaptivePortal::notFound("/", true, false)));
    TEST_ASSERT_EQUAL(static_cast<int>(NotFound::SetupScreen), static_cast<int>(CaptivePortal::notFound("/news/x.html", true, false)));
    TEST_ASSERT_TRUE(CaptivePortal::probeOpensSetup(true));
}

void test_hotspot_device_address_serves_the_app()
{
    TEST_ASSERT_EQUAL(static_cast<int>(NotFound::AppPage), static_cast<int>(CaptivePortal::notFound("/wifi", true, true)));
    TEST_ASSERT_EQUAL(static_cast<int>(NotFound::FileMissing), static_cast<int>(CaptivePortal::notFound("/lang.json", true, true)));
}

void test_api_paths_keep_their_errors()
{
    TEST_ASSERT_EQUAL(static_cast<int>(NotFound::AlpacaError), static_cast<int>(CaptivePortal::notFound("/api/v1/x/0/y", true, false)));
    TEST_ASSERT_EQUAL(static_cast<int>(NotFound::ApiError), static_cast<int>(CaptivePortal::notFound("/api/nope", true, false)));
    TEST_ASSERT_EQUAL(static_cast<int>(NotFound::ApiError), static_cast<int>(CaptivePortal::notFound("/api/nope", false, false)));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_no_credentials_opens_hotspot_at_once);
    RUN_TEST(test_saved_network_gets_time_before_hotspot);
    RUN_TEST(test_connected_never_opens_hotspot);
    RUN_TEST(test_home_network_is_never_redirected);
    RUN_TEST(test_hotspot_other_sites_go_to_setup);
    RUN_TEST(test_hotspot_device_address_serves_the_app);
    RUN_TEST(test_api_paths_keep_their_errors);
    return UNITY_END();
}
