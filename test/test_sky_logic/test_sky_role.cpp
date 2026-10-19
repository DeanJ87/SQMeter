#include <unity.h>

#include <cstring>

#include "SensorTypes.h"

using namespace SQM;

void test_sky_role_names()
{
    TEST_ASSERT_EQUAL_UINT8(0, static_cast<uint8_t>(SkyRole::Sqm));
    TEST_ASSERT_EQUAL_UINT8(1, static_cast<uint8_t>(SkyRole::Sqml));
    TEST_ASSERT_EQUAL_size_t(2, SKY_ROLE_COUNT);
    TEST_ASSERT_EQUAL_STRING("sqm", skyRoleName(SkyRole::Sqm));
    TEST_ASSERT_EQUAL_STRING("sqml", skyRoleName(SkyRole::Sqml));
    TEST_ASSERT_EQUAL_STRING("SQM", skyRoleLabel(SkyRole::Sqm));
    TEST_ASSERT_EQUAL_STRING("SQM-L", skyRoleLabel(SkyRole::Sqml));
}

void test_sky_role_parse()
{
    SkyRole role = SkyRole::Sqml;
    TEST_ASSERT_TRUE(parseSkyRole("sqm", role));
    TEST_ASSERT_TRUE(role == SkyRole::Sqm);
    TEST_ASSERT_TRUE(parseSkyRole("sqml", role));
    TEST_ASSERT_TRUE(role == SkyRole::Sqml);

    // Anything else is rejected and leaves the role as it was.
    const char *bad[] = {"", "SQM", "sqm-l", "SQM-L", "sqml ", "wide", "1"};
    for (const char *s : bad)
    {
        role = SkyRole::Sqml;
        TEST_ASSERT_FALSE_MESSAGE(parseSkyRole(s, role), s);
        TEST_ASSERT_TRUE(role == SkyRole::Sqml);
    }
    TEST_ASSERT_FALSE(parseSkyRole(nullptr, role));
}
