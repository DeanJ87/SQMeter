#include <unity.h>

#include <ArduinoJson.h>

#include "AlertDelivery.h"
#include "DeviceCore.h" // pulls in the libraries DeviceCore's sources need (PlatformIO's dependency finder)

using namespace SQM;

namespace
{
    Alerts::Alert rainAlert(Alerts::AlertLevel level = Alerts::AlertLevel::Urgent)
    {
        Alerts::Alert alert;
        alert.type = Alerts::AlertType::RainStarted;
        alert.level = level;
        alert.title = "Rain detected";
        alert.message = "Rain & wind: 2.4 mm/h";
        return alert;
    }

    std::string header(const Delivery::HttpRequest &request, const std::string &name)
    {
        for (const auto &h : request.headers)
            if (h.first == name)
                return h.second;
        return "";
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_url_encode_keeps_unreserved_and_escapes_the_rest()
{
    TEST_ASSERT_EQUAL_STRING("a-Z_0.~", Delivery::urlEncode("a-Z_0.~").c_str());
    TEST_ASSERT_EQUAL_STRING("a%20b%26c%2F%C3%A9", Delivery::urlEncode("a b&c/\xC3\xA9").c_str());
}

void test_full_title_prefixes_the_device_name()
{
    TEST_ASSERT_EQUAL_STRING("Obs: Rain", Delivery::fullTitle("Obs", "Rain").c_str());
    TEST_ASSERT_EQUAL_STRING("Rain", Delivery::fullTitle("", "Rain").c_str());
}

void test_pushover_form_body()
{
    const Delivery::HttpRequest r = Delivery::pushoverRequest(rainAlert(), {"user1", "tok1", "siren"}, "Obs");
    TEST_ASSERT_EQUAL_STRING("https://api.pushover.net/1/messages.json", r.url.c_str());
    TEST_ASSERT_EQUAL_STRING("application/x-www-form-urlencoded", r.contentType.c_str());
    TEST_ASSERT_EQUAL_STRING(
        "token=tok1&user=user1&title=Obs%3A%20Rain%20detected&message=Rain%20%26%20wind%3A%202.4%20mm%2Fh&priority=1&sound=siren",
        r.body.c_str());
}

void test_pushover_alert_sound_wins_and_emergency_repeats()
{
    Alerts::Alert alert = rainAlert(Alerts::AlertLevel::Wake);
    alert.sound = "alien";
    const Delivery::HttpRequest r = Delivery::pushoverRequest(alert, {"u", "t", "siren"}, "");
    TEST_ASSERT_NOT_EQUAL(std::string::npos, r.body.find("&priority=2&retry=60&expire=3600&sound=alien"));
}

void test_ntfy_request_headers_and_token()
{
    const Delivery::HttpRequest r = Delivery::ntfyRequest(rainAlert(), {"https://ntfy.sh//", "my topic", "tk"}, "Obs");
    TEST_ASSERT_EQUAL_STRING("https://ntfy.sh/my%20topic", r.url.c_str());
    TEST_ASSERT_EQUAL_STRING("text/plain; charset=utf-8", r.contentType.c_str());
    TEST_ASSERT_EQUAL_STRING("Obs: Rain detected", header(r, "Title").c_str());
    TEST_ASSERT_EQUAL_STRING("high", header(r, "Priority").c_str());
    TEST_ASSERT_EQUAL_STRING("cloud_with_rain", header(r, "Tags").c_str());
    TEST_ASSERT_EQUAL_STRING("Bearer tk", header(r, "Authorization").c_str());
    TEST_ASSERT_EQUAL_STRING("Rain & wind: 2.4 mm/h", r.body.c_str());
}

void test_ntfy_without_token_has_no_authorization()
{
    const Delivery::HttpRequest r = Delivery::ntfyRequest(rainAlert(), {"https://ntfy.sh", "t", ""}, "");
    TEST_ASSERT_EQUAL_STRING("", header(r, "Authorization").c_str());
    TEST_ASSERT_EQUAL_STRING("Rain detected", header(r, "Title").c_str());
}

void test_every_alert_type_has_a_tag()
{
    for (size_t i = 0; i < Alerts::ALERT_TYPE_COUNT; ++i)
        TEST_ASSERT_TRUE(std::string(Delivery::ntfyTags(static_cast<Alerts::AlertType>(i))).size() > 0);
}

void test_alert_json_lists_stacked_events_and_timestamp()
{
    Alerts::Alert alert = rainAlert();
    alert.stacked = {Alerts::AlertType::Unsafe};
    StaticJsonDocument<512> doc;
    deserializeJson(doc, Delivery::alertJson(alert, "Obs", 1791417600));
    TEST_ASSERT_EQUAL_STRING("rain_started", doc["event"] | "");
    TEST_ASSERT_EQUAL_STRING("rain_started", doc["events"][0] | "");
    TEST_ASSERT_EQUAL_STRING("unsafe", doc["events"][1] | "");
    TEST_ASSERT_EQUAL_STRING("urgent", doc["level"] | "");
    TEST_ASSERT_EQUAL_STRING("Obs", doc["device"] | "");
    TEST_ASSERT_EQUAL(1791417600, doc["timestamp"].as<int64_t>());
}

void test_alert_json_without_clock_or_stack()
{
    StaticJsonDocument<512> doc;
    deserializeJson(doc, Delivery::alertJson(rainAlert(), "Obs", 0));
    TEST_ASSERT_FALSE(doc.containsKey("timestamp"));
    TEST_ASSERT_FALSE(doc.containsKey("events"));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_url_encode_keeps_unreserved_and_escapes_the_rest);
    RUN_TEST(test_full_title_prefixes_the_device_name);
    RUN_TEST(test_pushover_form_body);
    RUN_TEST(test_pushover_alert_sound_wins_and_emergency_repeats);
    RUN_TEST(test_ntfy_request_headers_and_token);
    RUN_TEST(test_ntfy_without_token_has_no_authorization);
    RUN_TEST(test_every_alert_type_has_a_tag);
    RUN_TEST(test_alert_json_lists_stacked_events_and_timestamp);
    RUN_TEST(test_alert_json_without_clock_or_stack);
    return UNITY_END();
}
