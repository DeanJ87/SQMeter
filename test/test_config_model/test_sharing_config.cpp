#include <unity.h>

#include <string>

#include "Config.h"

// Share passwords setting (specs/037-share-secrets, contracts/api.md).

using namespace SQM;

void test_sharing_secrets_defaults_off()
{
    TEST_ASSERT_FALSE(Config::createDefault().sharing.secrets);
}

void test_sharing_secrets_round_trips_unmasked()
{
    Config c = Config::createDefault();
    c.sharing.secrets = true;
    // A setting, not a secret: the masked settings answer shows it as it is.
    TEST_ASSERT_NOT_EQUAL(std::string::npos, c.toJson(true).find("\"sharing\":{\"secrets\":true}"));
    std::string error;
    const auto loaded = Config::fromJson(c.toJson(), nullptr, &error);
    TEST_ASSERT_TRUE_MESSAGE(loaded.has_value(), error.c_str());
    TEST_ASSERT_TRUE(loaded->sharing.secrets);
}

void test_sharing_secrets_missing_from_older_config_loads_off()
{
    std::string error;
    const auto loaded = Config::fromJson("{\"auth\":{\"enabled\":true,\"username\":\"admin\",\"password\":\"pw\"}}", nullptr, &error);
    TEST_ASSERT_TRUE_MESSAGE(loaded.has_value(), error.c_str());
    TEST_ASSERT_FALSE(loaded->sharing.secrets);
}

void test_sharing_secrets_saves_without_a_device_password()
{
    // D-53 is a dependency, not a constraint: saved, reported not in effect.
    std::string error;
    const Config base = Config::createDefault();
    const auto cfg = Config::fromJson("{\"sharing\":{\"secrets\":true}}", &base, &error);
    TEST_ASSERT_TRUE_MESSAGE(cfg.has_value(), error.c_str());
    TEST_ASSERT_TRUE(cfg->sharing.secrets);
    TEST_ASSERT_FALSE(cfg->auth.enabled);
}

void test_sharing_secrets_must_be_a_switch()
{
    std::string error;
    const Config base = Config::createDefault();
    TEST_ASSERT_FALSE(Config::fromJson("{\"sharing\":{\"secrets\":\"yes\"}}", &base, &error).has_value());
    TEST_ASSERT_TRUE(error.find("sharing.secrets") != std::string::npos);
}
