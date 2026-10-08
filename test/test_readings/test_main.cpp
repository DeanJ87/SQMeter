#include <unity.h>

#include "Readings.h"

#include <cstring>
#include <string>
#include <vector>

using namespace SQM::Readings;

void setUp(void) {}
void tearDown(void) {}

namespace
{
    Snapshot healthy()
    {
        Snapshot s;
        s.timestamp = 1791401772;
        s.timeValid = true;
        s.dataAgeMs = 400;
        s.dataStale = false;
        s.light.status = Status::Ok;
        s.light.lux = 0.000301234;
        s.light.gain = "MAX";
        s.light.gainFactor = 9876;
        s.light.integrationMs = 600;
        s.light.nightMode = true;
        s.sky.sqm = 21.47999954;
        s.sky.rawSqm = 21.4;
        s.sky.nelm = 6.21;
        s.sky.bortle = 2;
        s.sky.description = "Typical truly dark site";
        s.environment.status = Status::Ok;
        s.environment.temperature = 12.34;
        s.environment.humidity = 64.7;
        s.environment.pressure = 1013.4;
        s.environment.dewpoint = 6.1;
        s.infrared.status = Status::Ok;
        s.infrared.skyTemperature = -24.7;
        s.infrared.ambientTemperature = 12.4;
        s.clouds.coverPercent = 3.4;
        s.clouds.condition = "clear";
        s.clouds.description = "Clear";
        s.clouds.humidityMeasured = true;
        return s;
    }

    DynamicJsonDocument render(const Snapshot &s, const Groups &groups = Groups{})
    {
        DynamicJsonDocument doc(4096);
        write(doc.to<JsonObject>(), s, groups);
        return doc;
    }

    bool camelCase(const char *key)
    {
        if (key[0] < 'a' || key[0] > 'z')
            return false;
        for (const char *c = key; *c; ++c)
            if (*c == '_' || *c == '-' || *c == ' ')
                return false;
        return true;
    }

    void assertCamelCase(JsonVariantConst value)
    {
        if (!value.is<JsonObjectConst>())
            return;
        for (JsonPairConst pair : value.as<JsonObjectConst>())
        {
            TEST_ASSERT_TRUE_MESSAGE(camelCase(pair.key().c_str()), pair.key().c_str());
            assertCamelCase(pair.value());
        }
    }
} // namespace

void test_healthy_document_has_rounded_values(void)
{
    DynamicJsonDocument doc = render(healthy());
    TEST_ASSERT_EQUAL(1791401772, doc["timestamp"].as<long long>());
    TEST_ASSERT_TRUE(doc["timeValid"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("ok", doc["sky"]["status"]);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 21.48f, static_cast<float>(doc["sky"]["sqm"].as<double>()));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 12.3f, static_cast<float>(doc["environment"]["temperature"].as<double>()));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 6.1f, static_cast<float>(doc["environment"]["dewpoint"].as<double>()));
    TEST_ASSERT_EQUAL_STRING("clear", doc["clouds"]["condition"]);
    TEST_ASSERT_EQUAL_STRING("measured", doc["clouds"]["humiditySource"]);
    std::string json;
    serializeJson(doc, json);
    TEST_ASSERT_NULL(strstr(json.c_str(), "21.4799"));
}

void test_faulted_sensor_sends_status_not_zeros(void)
{
    Snapshot s = healthy();
    s.environment.status = Status::Error;
    s.infrared.status = Status::Missing;
    DynamicJsonDocument doc = render(s);
    TEST_ASSERT_EQUAL_STRING("error", doc["environment"]["status"]);
    TEST_ASSERT_FALSE(doc["environment"].containsKey("temperature"));
    TEST_ASSERT_FALSE(doc["environment"].containsKey("dewpoint"));
    TEST_ASSERT_EQUAL_STRING("missing", doc["clouds"]["status"]);
    TEST_ASSERT_FALSE(doc["clouds"].containsKey("coverPercent"));
    TEST_ASSERT_FALSE(doc["infrared"].containsKey("skyTemperature"));
    // An error has an age; a sensor that never answered doesn't.
    TEST_ASSERT_TRUE(doc["environment"].containsKey("ageMs"));
    TEST_ASSERT_FALSE(doc["infrared"].containsKey("ageMs"));
}

void test_optional_hardware_only_when_present(void)
{
    Snapshot s = healthy();
    DynamicJsonDocument doc = render(s);
    TEST_ASSERT_FALSE(doc.containsKey("gps"));
    TEST_ASSERT_FALSE(doc.containsKey("rain"));
    TEST_ASSERT_FALSE(doc.containsKey("wind"));

    s.rain.present = true;
    s.rain.status = Status::Ok;
    s.rain.intensity = 2.4;
    s.wind.present = true;
    s.wind.status = Status::Ok;
    s.wind.speed = 3.3;
    s.wind.directionValid = false; // calm
    doc = render(s);
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 2.4f, static_cast<float>(doc["rain"]["intensity"].as<double>()));
    TEST_ASSERT_FLOAT_WITHIN(0.0001f, 3.3f, static_cast<float>(doc["wind"]["speed"].as<double>()));
    TEST_ASSERT_FALSE(doc["wind"].containsKey("direction"));
}

void test_groups_filter_mqtt_payload(void)
{
    Groups groups;
    groups.environment = false;
    groups.clouds = false;
    DynamicJsonDocument doc = render(healthy(), groups);
    TEST_ASSERT_TRUE(doc.containsKey("sky"));
    TEST_ASSERT_FALSE(doc.containsKey("environment"));
    TEST_ASSERT_FALSE(doc.containsKey("infrared"));
    TEST_ASSERT_FALSE(doc.containsKey("clouds"));
}

void test_timestamp_is_zero_without_clock(void)
{
    Snapshot s = healthy();
    s.timeValid = false;
    s.timestamp = 123456; // e.g. millis - must never leak
    DynamicJsonDocument doc = render(s);
    TEST_ASSERT_EQUAL(0, doc["timestamp"].as<long long>());
    TEST_ASSERT_FALSE(doc["timeValid"].as<bool>());
}

void test_every_key_is_camel_case(void)
{
    Snapshot s = healthy();
    s.gps.present = true;
    s.gps.status = Status::Ok;
    s.gps.fix = true;
    s.rain.present = true;
    s.rain.status = Status::Ok;
    s.wind.present = true;
    s.wind.status = Status::Ok;
    s.wind.directionValid = true;
    DynamicJsonDocument doc = render(s);
    assertCamelCase(doc.as<JsonVariantConst>());
}

void test_discovery_messages(void)
{
    DiscoveryDevice device{"sqmeter_aabbccddeeff", "Roof SQM", "0.3.0", "sqmeter", "homeassistant"};
    Groups groups;
    groups.wind = false;
    std::vector<std::pair<std::string, std::string>> messages;
    forEachDiscovery(device, groups, true, [&](const std::string &topic, const std::string &payload)
                     { messages.emplace_back(topic, payload); });

    TEST_ASSERT_EQUAL(17, messages.size());
    bool sawSqm = false, sawWindRemoval = false, sawSafety = false, sawSwitch = false;
    for (const auto &m : messages)
    {
        if (m.first == "homeassistant/sensor/sqmeter_aabbccddeeff/sqm/config")
        {
            sawSqm = true;
            DynamicJsonDocument doc(2048);
            TEST_ASSERT_FALSE(deserializeJson(doc, m.second));
            TEST_ASSERT_EQUAL_STRING("sqmeter/state", doc["state_topic"]);
            TEST_ASSERT_EQUAL_STRING("sqmeter_aabbccddeeff_sqm", doc["unique_id"]);
            TEST_ASSERT_EQUAL_STRING("sqmeter/availability", doc["availability"][0]["topic"]);
        }
        if (m.first == "homeassistant/sensor/sqmeter_aabbccddeeff/wind_speed/config")
            sawWindRemoval = m.second.empty();
        if (m.first == "homeassistant/binary_sensor/sqmeter_aabbccddeeff/safety/config")
            sawSafety = m.second.find("\"payload_on\":\"0\"") != std::string::npos;
        if (m.first == "homeassistant/switch/sqmeter_aabbccddeeff/alerts/config")
            sawSwitch = m.second.find("sqmeter/alerts/armed/set") != std::string::npos;
    }
    TEST_ASSERT_TRUE(sawSqm);
    TEST_ASSERT_TRUE(sawWindRemoval);
    TEST_ASSERT_TRUE(sawSafety);
    TEST_ASSERT_TRUE(sawSwitch);
}

int main(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    UNITY_BEGIN();
    RUN_TEST(test_healthy_document_has_rounded_values);
    RUN_TEST(test_faulted_sensor_sends_status_not_zeros);
    RUN_TEST(test_optional_hardware_only_when_present);
    RUN_TEST(test_groups_filter_mqtt_payload);
    RUN_TEST(test_timestamp_is_zero_without_clock);
    RUN_TEST(test_every_key_is_camel_case);
    RUN_TEST(test_discovery_messages);
    return UNITY_END();
}
