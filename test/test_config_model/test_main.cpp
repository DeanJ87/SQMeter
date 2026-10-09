#include <unity.h>

#include <ArduinoJson.h>

#include <cstring>

#include "Config.h"

using namespace SQM;

namespace
{
    std::string rejectReason(const std::string &json)
    {
        const Config base = Config::createDefault();
        std::string error;
        const auto cfg = Config::fromJson(json, &base, &error);
        return cfg ? std::string() : error;
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_defaults_are_valid_and_round_trip()
{
    const Config defaults = Config::createDefault();
    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(defaults.validate(&error), error.c_str());
    TEST_ASSERT_EQUAL_STRING("sqmeter", defaults.wifi.hostname.c_str());
    TEST_ASSERT_TRUE(defaults.wifi.mdns);
    TEST_ASSERT_TRUE(defaults.wifi.ipv6);
    TEST_ASSERT_EQUAL_STRING("sqmeter", defaults.mqtt.topic.c_str());

    // Loading normalises (e.g. the backup time source when GPS is off);
    // after that, a save/load round trip changes nothing.
    const auto loaded = Config::fromJson(defaults.toJson());
    TEST_ASSERT_TRUE(loaded.has_value());
    const auto again = Config::fromJson(loaded->toJson());
    TEST_ASSERT_TRUE(again.has_value());
    TEST_ASSERT_EQUAL_STRING(loaded->toJson().c_str(), again->toJson().c_str());
}

void test_secrets_redacted_and_kept()
{
    Config cfg = Config::createDefault();
    cfg.wifi.ssid = "Home";
    cfg.wifi.password = "hunter22";
    const std::string redacted = cfg.toJson(true);
    TEST_ASSERT_TRUE(redacted.find("hunter22") == std::string::npos);
    TEST_ASSERT_TRUE(redacted.find("********") != std::string::npos);

    // Sending the mask back keeps the stored password.
    const auto updated = Config::fromJson(redacted, &cfg);
    TEST_ASSERT_TRUE(updated.has_value());
    TEST_ASSERT_EQUAL_STRING("hunter22", updated->wifi.password.c_str());
}

void test_partial_json_merges_onto_base()
{
    Config base = Config::createDefault();
    base.deviceName = "Observatory";
    const auto cfg = Config::fromJson("{\"gps\":{\"enabled\":true}}", &base);
    TEST_ASSERT_TRUE(cfg.has_value());
    TEST_ASSERT_TRUE(cfg->gps.enabled);
    TEST_ASSERT_EQUAL_STRING("Observatory", cfg->deviceName.c_str());
}

void test_rejections_say_why()
{
    TEST_ASSERT_EQUAL_STRING("Device name is required", rejectReason("{\"deviceName\":\"\"}").c_str());
    TEST_ASSERT_TRUE(rejectReason("{\"wifi\":{\"hostname\":\"-bad\"}}").find("Hostname") == 0);
    TEST_ASSERT_TRUE(rejectReason("{\"mqtt\":{\"enabled\":true,\"broker\":\"b\",\"topic\":\"a/#\"}}").find("MQTT topic") == 0);
    TEST_ASSERT_TRUE(rejectReason("{\"sensor\":{\"readIntervalMs\":50}}").size() > 0);
    TEST_ASSERT_TRUE(rejectReason("{not json").find("couldn't be read") != std::string::npos);
    TEST_ASSERT_EQUAL_STRING("", rejectReason("{\"sensor\":{\"readIntervalMs\":100}}").c_str());
}

// Constraints (specs/020-settings-dependencies): combinations that can never
// work stay rejected with the device's message - D-27 time sources, D-33
// MQTT broker and topic, D-34 HTTP auth password. The web UI shows the same
// messages before saving (web/src/__tests__/configSchema.test.ts).
void test_constraints_rejected_with_messages()
{
    struct
    {
        const char *json;
        const char *message;
    } cases[] = {
        {R"({"ntp":{"enabled":false},"gps":{"enabled":false}})", "At least one time source must be enabled"},                      // D-27
        {R"({"mqtt":{"enabled":true,"broker":"","topic":"sqmeter"}})", "MQTT broker and topic are required when MQTT is enabled"}, // D-33
        {R"({"auth":{"enabled":true,"username":"admin","password":""}})", "HTTP auth password is required when auth is enabled"},  // D-34
    };
    for (const auto &c : cases)
    {
        std::string error;
        TEST_ASSERT_FALSE_MESSAGE(Config::fromJson(c.json, nullptr, &error).has_value(), c.json);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(c.message, error.c_str(), c.json);
    }
}

// D-32 is a dependency, not a constraint: command-line uploads without a
// password are kept (devices in the field store this) and reported inactive.
void test_ota_without_password_is_kept()
{
    auto cfg = Config::fromJson(R"({"ota":{"enabled":true,"password":""}})");
    TEST_ASSERT_TRUE(cfg.has_value());
    TEST_ASSERT_TRUE(cfg->ota.enabled);
}

void test_read_interval_boundaries()
{
    TEST_ASSERT_EQUAL_STRING("", rejectReason("{\"sensor\":{\"readIntervalMs\":3600000}}").c_str());
    TEST_ASSERT_TRUE(rejectReason("{\"sensor\":{\"readIntervalMs\":99}}").size() > 0);
    TEST_ASSERT_TRUE(rejectReason("{\"sensor\":{\"readIntervalMs\":3600001}}").size() > 0);
}

void test_mqtt_interval_and_hostname_boundaries()
{
    TEST_ASSERT_EQUAL_STRING("", rejectReason("{\"mqtt\":{\"publishIntervalMs\":86400000}}").c_str());
    TEST_ASSERT_TRUE(rejectReason("{\"mqtt\":{\"publishIntervalMs\":86400001}}").size() > 0);
    TEST_ASSERT_EQUAL_STRING("", rejectReason("{\"wifi\":{\"hostname\":\"a-32-character-hostname-ok-12345\"}}").c_str());
    TEST_ASSERT_TRUE(rejectReason("{\"wifi\":{\"hostname\":\"a-33-character-hostname-ok-123456\"}}").size() > 0);
}

// --- When to send and the imaging-app events (specs/021) ---

void test_alert_schedule_defaults()
{
    const Config cfg = Config::createDefault();
    TEST_ASSERT_TRUE(cfg.alerts.sendMode == AlertsConfig::SendMode::Any);
    TEST_ASSERT_EQUAL_UINT32(120, cfg.alerts.clientSilentSafetySeconds);
    TEST_ASSERT_EQUAL_UINT32(600, cfg.alerts.clientSilentWeatherSeconds);
    TEST_ASSERT_EQUAL_UINT8(3, cfg.alerts.clientLost.level);
    TEST_ASSERT_EQUAL_UINT8(1, cfg.alerts.clientBack.level);
    TEST_ASSERT_EQUAL_UINT8(0, cfg.alerts.clientDisconnected.level);
    DynamicJsonDocument doc(8192);
    deserializeJson(doc, cfg.toJson());
    TEST_ASSERT_EQUAL_STRING("any", doc["alerts"]["sendMode"]);
    TEST_ASSERT_FALSE(doc["alerts"]["armWithAlpaca"].as<bool>());
    TEST_ASSERT_EQUAL(3, doc["alerts"]["events"]["client_lost"]["level"].as<int>());
}

void test_arm_with_alpaca_migrates_and_is_still_written()
{
    const Config base = Config::createDefault();
    auto cfg = Config::fromJson("{\"alerts\":{\"armWithAlpaca\":true}}", &base);
    TEST_ASSERT_TRUE(cfg.has_value());
    TEST_ASSERT_TRUE(cfg->alerts.sendMode == AlertsConfig::SendMode::WhileConnected);
    TEST_ASSERT_TRUE(cfg->alerts.armWithAlpaca);
    cfg = Config::fromJson("{\"alerts\":{\"armWithAlpaca\":false}}", &*cfg);
    TEST_ASSERT_TRUE(cfg->alerts.sendMode == AlertsConfig::SendMode::Any);

    // sendMode wins over a stale armWithAlpaca (the new UI sends both).
    cfg = Config::fromJson("{\"alerts\":{\"sendMode\":\"whileConnected\",\"armWithAlpaca\":false}}", &base);
    TEST_ASSERT_TRUE(cfg->alerts.sendMode == AlertsConfig::SendMode::WhileConnected);
    // Older firmware reads armWithAlpaca from the main stored part.
    DynamicJsonDocument main(8192);
    deserializeJson(main, cfg->alertsToJson(false, Config::AlertsPart::Main));
    TEST_ASSERT_TRUE(main["armWithAlpaca"].as<bool>());
    TEST_ASSERT_TRUE(main["sendMode"].isNull());
    TEST_ASSERT_TRUE(main["events"]["client_lost"].isNull());

    TEST_ASSERT_EQUAL_STRING(
        "Alerts: when to send must be any or whileConnected", rejectReason("{\"alerts\":{\"sendMode\":\"sometimes\"}}").c_str());
}

void test_client_silence_boundaries()
{
    TEST_ASSERT_EQUAL_STRING(
        "", rejectReason("{\"alerts\":{\"clientSilentSafetySeconds\":30,\"clientSilentWeatherSeconds\":3600}}").c_str());
    TEST_ASSERT_EQUAL_STRING(
        "Alerts: silence times must be between 30 and 3600 seconds",
        rejectReason("{\"alerts\":{\"clientSilentSafetySeconds\":29}}").c_str());
    TEST_ASSERT_TRUE(rejectReason("{\"alerts\":{\"clientSilentWeatherSeconds\":3601}}").size() > 0);
}

void test_stored_parts_round_trip()
{
    Config cfg = Config::createDefault();
    cfg.alerts.sendMode = AlertsConfig::SendMode::WhileConnected;
    cfg.alerts.clientSilentSafetySeconds = 90;
    cfg.alerts.clientLost = {4, "siren", "Lost {device}", "Gone for {silent_for}"};
    // As ConfigStore loads them: the main document with both parts spliced in.
    const std::string stored = std::string("{\"alerts\":") + cfg.alertsToJson(false, Config::AlertsPart::Main) +
                               ",\"alertsClient\":" + cfg.alertsToJson(false, Config::AlertsPart::Client) + "}";
    const auto loaded = Config::fromJson(stored);
    TEST_ASSERT_TRUE(loaded.has_value());
    TEST_ASSERT_TRUE(loaded->alerts.sendMode == AlertsConfig::SendMode::WhileConnected);
    TEST_ASSERT_EQUAL_UINT32(90, loaded->alerts.clientSilentSafetySeconds);
    TEST_ASSERT_EQUAL_UINT8(4, loaded->alerts.clientLost.level);
    TEST_ASSERT_EQUAL_STRING("Gone for {silent_for}", loaded->alerts.clientLost.message.c_str());
    // First boot after the update: no client part yet - armWithAlpaca decides.
    const auto upgraded = Config::fromJson(std::string("{\"alerts\":") + cfg.alertsToJson(false, Config::AlertsPart::Main) + "}");
    TEST_ASSERT_TRUE(upgraded->alerts.sendMode == AlertsConfig::SendMode::WhileConnected);
    TEST_ASSERT_EQUAL_UINT32(120, upgraded->alerts.clientSilentSafetySeconds);
}

void test_full_client_templates_fit_and_add_nothing_to_main()
{
    Config cfg = Config::createDefault();
    const std::string mainBefore = cfg.alertsToJson(false, Config::AlertsPart::Main);
    const std::string title(AlertsConfig::MAX_TEMPLATE_TITLE, 'T');
    const std::string message(AlertsConfig::MAX_TEMPLATE_MESSAGE, 'M');
    for (AlertsConfig::EventSetting *event : {&cfg.alerts.clientLost, &cfg.alerts.clientBack, &cfg.alerts.clientDisconnected})
        *event = {4, "persistent", title, message};
    cfg.alerts.sendMode = AlertsConfig::SendMode::WhileConnected;
    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(cfg.validate(&error), error.c_str());
    TEST_ASSERT_TRUE(cfg.alertsToJson(false, Config::AlertsPart::Client).size() <= Config::MAX_ALERTS_JSON_BYTES);
    // The main part (what was stored before this feature) only changes by
    // the armWithAlpaca value it already had.
    std::string mainAfter = cfg.alertsToJson(false, Config::AlertsPart::Main);
    const size_t at = mainAfter.find("\"armWithAlpaca\":true");
    TEST_ASSERT_TRUE(at != std::string::npos);
    mainAfter.replace(at, std::strlen("\"armWithAlpaca\":true"), "\"armWithAlpaca\":false");
    TEST_ASSERT_EQUAL_STRING(mainBefore.c_str(), mainAfter.c_str());
}

void test_ipv6_setting_defaults_on_and_round_trips()
{
    const Config base = Config::createDefault();
    // Older saved config has no "ipv6": on.
    const auto old = Config::fromJson(R"({"wifi":{"ssid":"Home","mdns":true}})");
    TEST_ASSERT_TRUE(old.has_value());
    TEST_ASSERT_TRUE(old->wifi.ipv6);
    const auto off = Config::fromJson(R"({"wifi":{"ipv6":false}})", &base);
    TEST_ASSERT_TRUE(off.has_value());
    TEST_ASSERT_FALSE(off->wifi.ipv6);
    const auto again = Config::fromJson(off->toJson());
    TEST_ASSERT_FALSE(again->wifi.ipv6);
}

void test_ipv6_broker_and_webhook_forms()
{
    // Accepted: bare and bracketed IPv6, bracketed with a port, http with a bracketed IPv6.
    TEST_ASSERT_EQUAL_STRING("", rejectReason(R"({"mqtt":{"enabled":true,"broker":"fd00::10","topic":"sqm"}})").c_str());
    TEST_ASSERT_EQUAL_STRING("", rejectReason(R"({"mqtt":{"enabled":true,"broker":"[fd00::10]:1883","topic":"sqm"}})").c_str());
    TEST_ASSERT_EQUAL_STRING(
        "", rejectReason(R"({"alerts":{"webhook":{"enabled":true,"url":"http://[fd00::10]:8080/hook"}}})").c_str());

    TEST_ASSERT_EQUAL_STRING(
        "MQTT broker: Put the port in the Port field",
        rejectReason(R"({"mqtt":{"enabled":true,"broker":"broker.local:1883","topic":"sqm"}})").c_str());
    TEST_ASSERT_EQUAL_STRING(
        "MQTT broker: Not a valid IPv6 address", rejectReason(R"({"mqtt":{"enabled":true,"broker":"fd00::zz","topic":"sqm"}})").c_str());
    TEST_ASSERT_EQUAL_STRING(
        "Alerts: webhook URL: https to an IPv6 address isn't supported yet - use a host name, or http",
        rejectReason(R"({"alerts":{"webhook":{"enabled":true,"url":"https://[fd00::10]/hook"}}})").c_str());

    // Saved config isn't held to the new forms, so it always loads.
    const auto stored = Config::fromJson(R"({"mqtt":{"enabled":true,"broker":"broker.local:1883","topic":"sqm"}})");
    TEST_ASSERT_TRUE(stored.has_value());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_defaults_are_valid_and_round_trip);
    RUN_TEST(test_secrets_redacted_and_kept);
    RUN_TEST(test_partial_json_merges_onto_base);
    RUN_TEST(test_rejections_say_why);
    RUN_TEST(test_read_interval_boundaries);
    RUN_TEST(test_constraints_rejected_with_messages);
    RUN_TEST(test_ota_without_password_is_kept);
    RUN_TEST(test_mqtt_interval_and_hostname_boundaries);
    RUN_TEST(test_alert_schedule_defaults);
    RUN_TEST(test_arm_with_alpaca_migrates_and_is_still_written);
    RUN_TEST(test_client_silence_boundaries);
    RUN_TEST(test_stored_parts_round_trip);
    RUN_TEST(test_full_client_templates_fit_and_add_nothing_to_main);
    RUN_TEST(test_ipv6_setting_defaults_on_and_round_trips);
    RUN_TEST(test_ipv6_broker_and_webhook_forms);
    return UNITY_END();
}
