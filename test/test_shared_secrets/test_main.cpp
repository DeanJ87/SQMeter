#include <unity.h>

#include <ArduinoJson.h>

#include "Config.h"
#include "SharedSecrets.h"

#include <string>

// Passwords handed to a paired device (specs/037-share-secrets): which
// refusal comes first, the answer's exact shape, and the failed-login lock.

using namespace SQM;
using namespace SQM::SharedSecrets;

void setUp(void) {}
void tearDown(void) {}

namespace
{
    // Every kind of secret set, sharing on, a device password set.
    Config sharing()
    {
        Config cfg = Config::createDefault();
        cfg.wifi.password = "wifi-secret";
        cfg.mqtt.password = "mqtt-secret";
        cfg.ota.enabled = true;
        cfg.ota.password = "ota-secret";
        cfg.auth.enabled = true;
        cfg.auth.password = "web-secret";
        cfg.alerts.ntfyToken = "tk_ntfy";
        cfg.alerts.pushoverUserKey = "uQiRzpo4DXghDmr9QzzfQu27cmVRsG";
        cfg.alerts.pushoverAppToken = "azGDORePK8gMaC0QOYAMyEEuzJnyUi";
        cfg.alerts.webhookAuthHeader = "Bearer webhook-secret";
        cfg.ble.passkey = "123456";
        cfg.sharing.secrets = true;
        return cfg;
    }
} // namespace

void test_sharing_is_off_by_default()
{
    const Config cfg = Config::createDefault();
    TEST_ASSERT_FALSE(cfg.sharing.secrets);
    TEST_ASSERT_EQUAL(static_cast<int>(Refusal::SharingOff), static_cast<int>(refusal(cfg)));
}

void test_sharing_off_is_refused_first()
{
    Config cfg = sharing();
    cfg.sharing.secrets = false;
    TEST_ASSERT_EQUAL(static_cast<int>(Refusal::SharingOff), static_cast<int>(refusal(cfg)));
    // Off wins over a missing device password too.
    cfg.auth.enabled = false;
    TEST_ASSERT_EQUAL(static_cast<int>(Refusal::SharingOff), static_cast<int>(refusal(cfg)));
    TEST_ASSERT_EQUAL_STRING("Password sharing is off in SQMeter", refusalMessage(Refusal::SharingOff));
    TEST_ASSERT_EQUAL(403, refusalStatus(Refusal::SharingOff));
}

void test_no_device_password_is_refused()
{
    Config cfg = sharing();
    cfg.auth.enabled = false;
    TEST_ASSERT_FALSE(devicePasswordSet(cfg));
    TEST_ASSERT_EQUAL(static_cast<int>(Refusal::NoDevicePassword), static_cast<int>(refusal(cfg)));

    cfg.auth.enabled = true;
    cfg.auth.password = "";
    TEST_ASSERT_FALSE(devicePasswordSet(cfg));
    TEST_ASSERT_EQUAL(static_cast<int>(Refusal::NoDevicePassword), static_cast<int>(refusal(cfg)));

    TEST_ASSERT_EQUAL_STRING("Set a web password in SQMeter before sharing passwords", refusalMessage(Refusal::NoDevicePassword));
    TEST_ASSERT_EQUAL(403, refusalStatus(Refusal::NoDevicePassword));
}

void test_sharing_on_with_a_device_password_goes_on_to_the_login()
{
    const Config cfg = sharing();
    TEST_ASSERT_TRUE(devicePasswordSet(cfg));
    TEST_ASSERT_EQUAL(static_cast<int>(Refusal::None), static_cast<int>(refusal(cfg)));
    TEST_ASSERT_NULL(refusalMessage(Refusal::None));
    TEST_ASSERT_EQUAL(200, refusalStatus(Refusal::None));
}

void test_answer_is_exactly_the_contract()
{
    TEST_ASSERT_EQUAL_STRING(
        "{\"mqtt\":{\"password\":\"mqtt-secret\"},\"alerts\":{\"ntfy\":{\"token\":\"tk_ntfy\"},"
        "\"pushover\":{\"userKey\":\"uQiRzpo4DXghDmr9QzzfQu27cmVRsG\",\"appToken\":\"azGDORePK8gMaC0QOYAMyEEuzJnyUi\"}}}",
        toJson(sharing()).c_str());
}

void test_unset_values_are_empty_strings()
{
    Config cfg = Config::createDefault();
    cfg.sharing.secrets = true;
    TEST_ASSERT_EQUAL_STRING(
        "{\"mqtt\":{\"password\":\"\"},\"alerts\":{\"ntfy\":{\"token\":\"\"},\"pushover\":{\"userKey\":\"\",\"appToken\":\"\"}}}",
        toJson(cfg).c_str());
}

void test_answer_never_holds_another_secret()
{
    const std::string json = toJson(sharing());
    for (const char *other : {"wifi-secret", "ota-secret", "web-secret", "webhook-secret", "123456"})
        TEST_ASSERT_EQUAL_MESSAGE(std::string::npos, json.find(other), other);
}

void test_values_with_quotes_are_escaped()
{
    Config cfg = sharing();
    cfg.mqtt.password = "a\"b\\c";
    StaticJsonDocument<512> doc;
    TEST_ASSERT_FALSE(deserializeJson(doc, toJson(cfg)));
    TEST_ASSERT_EQUAL_STRING("a\"b\\c", doc["mqtt"]["password"].as<const char *>());
}

void test_channels_that_are_off_still_share_what_is_stored()
{
    Config cfg = sharing();
    cfg.alerts.ntfyEnabled = false;
    cfg.alerts.pushoverEnabled = false;
    cfg.mqtt.enabled = false;
    const std::string json = toJson(cfg);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("tk_ntfy"));
    TEST_ASSERT_NOT_EQUAL(std::string::npos, json.find("mqtt-secret"));
}

void test_settings_answer_still_masks_the_shared_secrets()
{
    const std::string masked = sharing().toJson(true);
    for (const char *secret : {"mqtt-secret", "tk_ntfy", "uQiRzpo4DXghDmr9QzzfQu27cmVRsG", "azGDORePK8gMaC0QOYAMyEEuzJnyUi"})
        TEST_ASSERT_EQUAL_MESSAGE(std::string::npos, masked.find(secret), secret);
    TEST_ASSERT_NOT_EQUAL(std::string::npos, masked.find("\"sharing\":{\"secrets\":true}"));
}

void test_lock_starts_on_the_fifth_failure()
{
    LoginLock lock;
    for (int i = 0; i < 4; ++i)
    {
        TEST_ASSERT_FALSE(lock.failed(1000));
        TEST_ASSERT_EQUAL_UINT32(0, lock.retryAfterSeconds(1000));
    }
    TEST_ASSERT_TRUE(lock.failed(1000));
    TEST_ASSERT_EQUAL_UINT32(60, lock.retryAfterSeconds(1000));
    TEST_ASSERT_EQUAL_UINT32(30, lock.retryAfterSeconds(31000));
    TEST_ASSERT_EQUAL_UINT32(1, lock.retryAfterSeconds(60999));
    TEST_ASSERT_EQUAL_UINT32(0, lock.retryAfterSeconds(61000));
}

void test_success_resets_the_count()
{
    LoginLock lock;
    for (int i = 0; i < 4; ++i)
        lock.failed(0);
    lock.succeeded();
    for (int i = 0; i < 4; ++i)
        TEST_ASSERT_FALSE(lock.failed(0));
    TEST_ASSERT_EQUAL_UINT32(0, lock.retryAfterSeconds(0));
}

void test_after_the_lock_five_more_failures_are_needed()
{
    LoginLock lock;
    for (int i = 0; i < 5; ++i)
        lock.failed(0);
    TEST_ASSERT_EQUAL_UINT32(0, lock.retryAfterSeconds(60000));
    for (int i = 0; i < 4; ++i)
        TEST_ASSERT_FALSE(lock.failed(60000));
    TEST_ASSERT_TRUE(lock.failed(60000));
}

void test_lock_survives_the_millis_wrap()
{
    LoginLock lock;
    const uint32_t start = 0xFFFFFFFFu - 10000u; // 10 s before the wrap
    for (int i = 0; i < 5; ++i)
        lock.failed(start);
    TEST_ASSERT_EQUAL_UINT32(50, lock.retryAfterSeconds(start + 10000u)); // wrapped to 0
    TEST_ASSERT_EQUAL_UINT32(0, lock.retryAfterSeconds(start + 60000u));
    // Forgotten once lifted: a later wrap can't bring it back.
    TEST_ASSERT_EQUAL_UINT32(0, lock.retryAfterSeconds(start));
}

void test_only_basic_credentials_count_as_a_login_attempt()
{
    TEST_ASSERT_FALSE(isLoginAttempt(nullptr));
    TEST_ASSERT_FALSE(isLoginAttempt(""));
    TEST_ASSERT_FALSE(isLoginAttempt("Basic"));
    TEST_ASSERT_FALSE(isLoginAttempt("Basic "));
    TEST_ASSERT_FALSE(isLoginAttempt("Bearer abc"));
    TEST_ASSERT_FALSE(isLoginAttempt("Digest username=\"admin\""));
    TEST_ASSERT_TRUE(isLoginAttempt("Basic YWRtaW46d3Jvbmc="));
    TEST_ASSERT_TRUE(isLoginAttempt("basic YWRtaW46d3Jvbmc="));
}

void test_a_quiet_spell_forgets_old_failures()
{
    LoginLock lock;
    for (int i = 0; i < 4; ++i)
        lock.failed(0);
    // Ten quiet minutes: the next miss is the first again, so a host that
    // misses now and then never locks the paired device out.
    TEST_ASSERT_FALSE(lock.failed(LoginLock::QUIET_RESET_MS));
    for (int i = 0; i < 3; ++i)
        TEST_ASSERT_FALSE(lock.failed(LoginLock::QUIET_RESET_MS + 1000));
    TEST_ASSERT_TRUE(lock.failed(LoginLock::QUIET_RESET_MS + 2000));
}

void test_misses_closer_than_the_quiet_spell_still_add_up()
{
    LoginLock lock;
    for (uint32_t i = 0; i < 4; ++i)
        TEST_ASSERT_FALSE(lock.failed(i * (LoginLock::QUIET_RESET_MS - 1)));
    TEST_ASSERT_TRUE(lock.failed(4 * (LoginLock::QUIET_RESET_MS - 1)));
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_sharing_is_off_by_default);
    RUN_TEST(test_sharing_off_is_refused_first);
    RUN_TEST(test_no_device_password_is_refused);
    RUN_TEST(test_sharing_on_with_a_device_password_goes_on_to_the_login);
    RUN_TEST(test_answer_is_exactly_the_contract);
    RUN_TEST(test_unset_values_are_empty_strings);
    RUN_TEST(test_answer_never_holds_another_secret);
    RUN_TEST(test_values_with_quotes_are_escaped);
    RUN_TEST(test_channels_that_are_off_still_share_what_is_stored);
    RUN_TEST(test_settings_answer_still_masks_the_shared_secrets);
    RUN_TEST(test_lock_starts_on_the_fifth_failure);
    RUN_TEST(test_success_resets_the_count);
    RUN_TEST(test_after_the_lock_five_more_failures_are_needed);
    RUN_TEST(test_lock_survives_the_millis_wrap);
    RUN_TEST(test_only_basic_credentials_count_as_a_login_attempt);
    RUN_TEST(test_a_quiet_spell_forgets_old_failures);
    RUN_TEST(test_misses_closer_than_the_quiet_spell_still_add_up);
    return UNITY_END();
}
