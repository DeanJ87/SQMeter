#include <unity.h>

#include "Readings.h"

#include <map>
#include <string>

// Restart reason and boot time on MQTT <base>/device (specs/038-restart-reason-mqtt).

using namespace SQM::Readings;

void test_restart_reason_names(void)
{
    const char *expected[] = {
        "unknown",
        "poweron",
        "external",
        "software",
        "panic",
        "watchdog",
        "watchdog",
        "watchdog",
        "deepsleep",
        "brownout",
        "sdio",
        "usb",
        "jtag",
        "efuse",
        "powerglitch",
        "cpulockup"};
    for (int code = 0; code < 16; ++code)
        TEST_ASSERT_EQUAL_STRING(expected[code], restartReasonName(code));
    TEST_ASSERT_EQUAL_STRING("unknown", restartReasonName(16));
    TEST_ASSERT_EQUAL_STRING("unknown", restartReasonName(-1));
}

void test_device_message_with_and_without_clock(void)
{
    DeviceInfo info;
    info.resetReason = 9;
    info.bootCount = 3;
    info.bootEpoch = 1795550000; // 2026-11-24T19:53:20Z

    DynamicJsonDocument doc(256);
    writeDevice(doc.to<JsonObject>(), info);
    TEST_ASSERT_EQUAL_STRING("brownout", doc["resetReason"]);
    TEST_ASSERT_EQUAL(9, doc["resetReasonCode"].as<int>());
    TEST_ASSERT_EQUAL(3, doc["bootCount"].as<int>());
    TEST_ASSERT_EQUAL_STRING("2026-11-24T19:53:20Z", doc["bootTime"]);

    info.bootEpoch = 1200; // clock not set: seconds since boot only
    DynamicJsonDocument noClock(256);
    writeDevice(noClock.to<JsonObject>(), info);
    TEST_ASSERT_TRUE(noClock.containsKey("bootTime"));
    TEST_ASSERT_TRUE(noClock["bootTime"].isNull());
    TEST_ASSERT_FALSE(bootTimeKnown(1200));
    TEST_ASSERT_TRUE(bootTimeKnown(1795550000));
}

void test_device_discovery_entities(void)
{
    DiscoveryDevice device{"sqmeter_aabbccddeeff", "Roof SQM", "0.4.0", "sqmeter", "homeassistant"};
    Groups groups;
    groups.sky = groups.environment = groups.clouds = groups.rain = groups.wind = false; // always announced regardless
    std::map<std::string, std::string> out;
    forEachDiscovery(device, groups, false, [&](const std::string &topic, const std::string &payload) { out[topic] = payload; });

    const std::string reasonTopic = "homeassistant/sensor/sqmeter_aabbccddeeff/restart_reason/config";
    const std::string bootTopic = "homeassistant/sensor/sqmeter_aabbccddeeff/last_boot/config";
    TEST_ASSERT_FALSE(out[reasonTopic].empty());
    TEST_ASSERT_FALSE(out[bootTopic].empty());

    DynamicJsonDocument reason(2048);
    TEST_ASSERT_FALSE(deserializeJson(reason, out[reasonTopic]));
    TEST_ASSERT_EQUAL_STRING("Last restart reason", reason["name"]);
    TEST_ASSERT_EQUAL_STRING("sqmeter/device", reason["state_topic"]);
    TEST_ASSERT_EQUAL_STRING("diagnostic", reason["entity_category"]);
    TEST_ASSERT_TRUE(std::string(reason["value_template"].as<const char *>()).find("brownout or power loss") != std::string::npos);
    TEST_ASSERT_FALSE(reason.containsKey("state_class"));
    TEST_ASSERT_FALSE(reason.containsKey("device_class"));

    DynamicJsonDocument boot(2048);
    TEST_ASSERT_FALSE(deserializeJson(boot, out[bootTopic]));
    TEST_ASSERT_EQUAL_STRING("Last boot", boot["name"]);
    TEST_ASSERT_EQUAL_STRING("sqmeter/device", boot["state_topic"]);
    TEST_ASSERT_EQUAL_STRING("timestamp", boot["device_class"]);
    TEST_ASSERT_EQUAL_STRING("diagnostic", boot["entity_category"]);
    TEST_ASSERT_FALSE(boot.containsKey("state_class"));
    TEST_ASSERT_EQUAL(1, boot["availability"].size());
}
