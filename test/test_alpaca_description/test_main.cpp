#include <unity.h>

#include "AlpacaRouter.h"

#include <cstring>
#include <string>

// The management description (spec 007 FR-008): ServerName is the device's
// name, Manufacturer the product, Location where the device is.

namespace
{
    class Backend : public SQM::Alpaca::Backend
    {
    public:
        std::string name = "Observatory";
        std::string where = "51.4779, -0.0015";
        bool alpacaEnabled() const override { return true; }
        bool isSafe() const override { return true; }
        SQM::Alpaca::ObservingConditionsSnapshot observingConditions() const override { return {}; }
        std::string serverName() const override { return name; }
        std::string location() const override { return where; }
        std::string timestampUtc() const override { return ""; }
    };

    std::string description(SQM::Alpaca::Router &router)
    {
        SQM::Alpaca::Request request;
        request.get = true;
        request.path = "/management/v1/description";
        SQM::Alpaca::Response response;
        TEST_ASSERT_TRUE(router.handle(request, response));
        return response.body;
    }
} // namespace

void setUp(void) {}
void tearDown(void) {}

void test_names_the_device_and_its_location(void)
{
    Backend backend;
    SQM::Alpaca::Router router(backend, {"SQMeter", "SQMeter", "1.2.3", 0x1234});
    const std::string body = description(router);
    TEST_ASSERT_NOT_NULL(strstr(body.c_str(), "\"ServerName\":\"Observatory\""));
    TEST_ASSERT_NOT_NULL(strstr(body.c_str(), "\"Manufacturer\":\"SQMeter\""));
    TEST_ASSERT_NOT_NULL(strstr(body.c_str(), "\"Location\":\"51.4779, -0.0015\""));
}

void test_no_name_or_location(void)
{
    Backend backend;
    backend.name = "";
    backend.where = "";
    SQM::Alpaca::Router router(backend, {"SQMeter", "SQMeter", "1.2.3", 0x1234});
    const std::string body = description(router);
    TEST_ASSERT_NOT_NULL(strstr(body.c_str(), "\"ServerName\":\"SQMeter\"")); // no name: the product
    TEST_ASSERT_NOT_NULL(strstr(body.c_str(), "\"Location\":\"\""));
}

void test_format_location(void)
{
    TEST_ASSERT_EQUAL_STRING("51.4779, -0.0015", SQM::Alpaca::formatLocation(51.47789, -0.00149).c_str());
    TEST_ASSERT_EQUAL_STRING("-33.8600, 151.2100", SQM::Alpaca::formatLocation(-33.86, 151.21).c_str());
    TEST_ASSERT_EQUAL_STRING("0.0000, 0.0000", SQM::Alpaca::formatLocation(0, 0).c_str());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_names_the_device_and_its_location);
    RUN_TEST(test_no_name_or_location);
    RUN_TEST(test_format_location);
    return UNITY_END();
}
