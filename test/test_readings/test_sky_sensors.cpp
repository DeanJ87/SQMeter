#include <unity.h>

#include "Readings.h"

#include <cstdio>
#include <cstring>
#include <map>
#include <string>

// Two sky sensors in the readings document (spec 030, contracts/readings.md).

using namespace SQM;
using namespace SQM::Readings;

namespace
{
    Snapshot skyHealthy()
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
        s.infrared.status = Status::Ok;
        s.clouds.condition = "clear";
        s.clouds.description = "Clear";
        return s;
    }

    SkySensorEntry entry(double sqm)
    {
        SkySensorEntry e;
        e.present = true;
        e.status = Status::Ok;
        e.ageMs = 412;
        e.sqm = sqm;
        e.rawSqm = sqm - 0.004;
        e.nelm = 6.54;
        e.bortle = 3;
        e.description = "Rural sky";
        e.calibrated = false;
        e.averagingWindowSeconds = 90;
        e.lux = 0.000412345678;
        e.gain = "MAX";
        e.integrationMs = 600;
        e.saturated = false;
        e.nightMode = true;
        return e;
    }

    Snapshot bothSensors()
    {
        Snapshot s = skyHealthy();
        s.skySensors[0] = entry(21.234);
        s.skySensors[1] = entry(21.4849);
        return s;
    }

    DynamicJsonDocument renderSky(const Snapshot &s, const Groups &groups = Groups{})
    {
        DynamicJsonDocument doc(DOC_CAPACITY);
        write(doc.to<JsonObject>(), s, groups);
        return doc;
    }

    // Every group ok and every optional sensor fitted, both sky sensors, long strings.
    Snapshot worstCase()
    {
        Snapshot s = bothSensors();
        s.source = SkyRole::Sqml;
        s.clouds.coverPercent = 100;
        s.clouds.description = "Overcast, thick cloud";
        s.gps.present = true;
        s.gps.status = Status::Ok;
        s.gps.fix = true;
        s.gps.satellites = 12;
        s.gps.latitude = -33.865143;
        s.gps.longitude = 151.209900;
        s.gps.altitude = 1234.5;
        s.gps.hdop = 0.9;
        s.rain.present = true;
        s.rain.status = Status::Ok;
        s.rain.raining = true;
        s.rain.rainingNow = false;
        s.rain.clearInMs = 899000;
        s.rain.intensity = 12.34;
        s.rain.eventAccumulation = 123.4;
        s.rain.sensorEventAccumulation = 123.4;
        s.rain.totalAccumulation = 12345.6;
        s.wind.present = true;
        s.wind.status = Status::Ok;
        s.wind.speed = 12.3;
        s.wind.gust = 23.4;
        s.wind.direction = 359;
        s.wind.directionValid = true;
        for (SkySensorEntry &e : s.skySensors)
            e.description = "Bright suburban sky";
        return s;
    }
} // namespace

void test_sky_source_always_written_with_the_sky_group()
{
    Snapshot s = skyHealthy();
    DynamicJsonDocument doc = renderSky(s);
    TEST_ASSERT_EQUAL_STRING("sqm", doc["sky"]["source"]);

    s.source = SkyRole::Sqml;
    s.light.status = Status::Missing;
    doc = renderSky(s);
    TEST_ASSERT_EQUAL_STRING("missing", doc["sky"]["status"]);
    TEST_ASSERT_EQUAL_STRING("sqml", doc["sky"]["source"]);
    TEST_ASSERT_FALSE(doc["sky"].containsKey("sqm"));

    Groups noSky;
    noSky.sky = false;
    doc = renderSky(bothSensors(), noSky);
    TEST_ASSERT_FALSE(doc.containsKey("sky"));
    TEST_ASSERT_FALSE(doc.containsKey("skySensors"));
}

// SC-001: the sky group keeps every field, name and value and only gains
// sky.source. What a whole one-sensor device emits (its enabled SQM adds
// skySensors.sqm) is pinned by test_one_sensor_device_readings_document in
// test/test_device_core.
void test_sky_group_unchanged_but_source()
{
    const Snapshot s = skyHealthy();
    std::string json;
    serializeJson(renderSky(s), json);
    TEST_ASSERT_NULL(strstr(json.c_str(), "skySensors"));
    const std::string expected = "\"sky\":{\"status\":\"ok\",\"source\":\"sqm\",\"sqm\":21.48,\"rawSqm\":21.4,\"nelm\":6.2,\"bortle\":2,"
                                 "\"description\":\"Typical truly dark site\",\"calibrated\":false,\"averagingWindowSeconds\":0}";
    TEST_ASSERT_NOT_NULL_MESSAGE(strstr(json.c_str(), expected.c_str()), json.c_str());

    // With the SQM listed (enabled) but SQM-L off: only the SQM entry, no delta.
    Snapshot one = skyHealthy();
    one.skySensors[0] = entry(21.2);
    DynamicJsonDocument doc = renderSky(one);
    TEST_ASSERT_TRUE(doc["skySensors"].containsKey("sqm"));
    TEST_ASSERT_FALSE(doc["skySensors"].containsKey("sqml"));
    TEST_ASSERT_FALSE(doc["skySensors"].containsKey("deltaSqm"));
}

void test_sky_sensor_entries_and_rounding()
{
    DynamicJsonDocument doc = renderSky(bothSensors());
    JsonObject sqm = doc["skySensors"]["sqm"];
    TEST_ASSERT_EQUAL_STRING("ok", sqm["status"]);
    TEST_ASSERT_EQUAL(412, sqm["ageMs"].as<int>());
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 21.23F, (float)sqm["sqm"].as<double>());
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 21.23F, (float)sqm["rawSqm"].as<double>());
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 6.5F, (float)sqm["nelm"].as<double>());
    TEST_ASSERT_EQUAL(3, sqm["bortle"].as<int>());
    TEST_ASSERT_EQUAL_STRING("Rural sky", sqm["description"]);
    TEST_ASSERT_FALSE(sqm["calibrated"].as<bool>());
    TEST_ASSERT_EQUAL(90, sqm["averagingWindowSeconds"].as<int>());
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.000412F, (float)sqm["lux"].as<double>());
    TEST_ASSERT_EQUAL_STRING("MAX", sqm["gain"]);
    TEST_ASSERT_EQUAL(600, sqm["integrationMs"].as<int>());
    TEST_ASSERT_FALSE(sqm["saturated"].as<bool>());
    TEST_ASSERT_TRUE(sqm["nightMode"].as<bool>());
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 21.48F, (float)doc["skySensors"]["sqml"]["sqm"].as<double>());
    // SQM-L - SQM from the unrounded values, 2 decimals.
    TEST_ASSERT_FLOAT_WITHIN(1e-6F, 0.25F, (float)doc["skySensors"]["deltaSqm"].as<double>());
}

void test_sky_sensor_not_ok_carries_status_only()
{
    Snapshot s = bothSensors();
    s.skySensors[1].status = Status::Error;
    s.skySensors[1].ageMs = 5120;
    DynamicJsonDocument doc = renderSky(s);
    JsonObject sqml = doc["skySensors"]["sqml"];
    TEST_ASSERT_EQUAL_STRING("error", sqml["status"]);
    TEST_ASSERT_EQUAL(5120, sqml["ageMs"].as<int>());
    TEST_ASSERT_EQUAL(2, sqml.size());
    TEST_ASSERT_FALSE(doc["skySensors"].containsKey("deltaSqm"));
    TEST_ASSERT_TRUE(doc["skySensors"]["sqm"].containsKey("sqm"));

    s.skySensors[1].status = Status::Missing;
    doc = renderSky(s);
    TEST_ASSERT_EQUAL(1, doc["skySensors"]["sqml"].as<JsonObject>().size());

    s.skySensors[1].status = Status::Stale;
    doc = renderSky(s);
    TEST_ASSERT_EQUAL_STRING("stale", doc["skySensors"]["sqml"]["status"]);
    TEST_ASSERT_FALSE(doc["skySensors"].containsKey("deltaSqm"));

    // Neither enabled: no skySensors at all.
    Snapshot none = skyHealthy();
    doc = renderSky(none);
    TEST_ASSERT_FALSE(doc.containsKey("skySensors"));
}

// Research D6: the worst case fits DOC_CAPACITY with 20% headroom and the
// MQTT payload fits the 3072-byte client buffer.
void test_worst_case_document_fits()
{
    DynamicJsonDocument doc(DOC_CAPACITY * 2);
    write(doc.to<JsonObject>(), worstCase());
    TEST_ASSERT_FALSE(doc.overflowed());
    const size_t used = doc.memoryUsage();
    char detail[96];
    snprintf(detail, sizeof detail, "pool %u bytes of %u", static_cast<unsigned>(used), static_cast<unsigned>(DOC_CAPACITY));
    TEST_ASSERT_TRUE_MESSAGE(used * 12 / 10 <= DOC_CAPACITY, detail);

    std::string payload;
    serializeJson(doc, payload);
    snprintf(detail, sizeof detail, "payload %u bytes", static_cast<unsigned>(payload.size()));
    TEST_ASSERT_LESS_THAN_MESSAGE(MQTT_PAYLOAD_LIMIT, payload.size(), detail);
}

namespace
{
    std::map<std::string, std::string> discovery(bool sqm, bool sqml, bool sky = true)
    {
        DiscoveryDevice device{"sqmeter_aabbccddeeff", "Roof SQM", "0.4.0", "sqmeter", "homeassistant"};
        Groups groups;
        groups.sky = sky;
        groups.skySqm = sqm;
        groups.skySqml = sqml;
        std::map<std::string, std::string> out;
        forEachDiscovery(device, groups, true, [&](const std::string &topic, const std::string &payload) { out[topic] = payload; });
        return out;
    }

    const std::string TOPIC = "homeassistant/sensor/sqmeter_aabbccddeeff/";
} // namespace

// Spec 030 D7: an entity per sky sensor and their difference.
void test_discovery_sky_sensor_entities()
{
    auto both = discovery(true, true);
    struct
    {
        const char *object;
        const char *name;
        const char *value;
        const char *availability;
    } const expected[] = {
        {"sqm_wide",
         "SQM (wide)",
         "{{ value_json.skySensors.sqm.sqm if value_json.skySensors.sqm.sqm is defined else none }}",
         "{{ 'online' if value_json.skySensors.sqm.status == 'ok' else 'offline' }}"},
        {"sqm_l",
         "SQM-L (narrow)",
         "{{ value_json.skySensors.sqml.sqm if value_json.skySensors.sqml.sqm is defined else none }}",
         "{{ 'online' if value_json.skySensors.sqml.status == 'ok' else 'offline' }}"},
        {"sqm_delta",
         "SQM-L − SQM",
         "{{ value_json.skySensors.deltaSqm if value_json.skySensors.deltaSqm is defined else none }}",
         "{{ 'online' if value_json.skySensors.deltaSqm is defined else 'offline' }}"},
    };
    for (const auto &e : expected)
    {
        const std::string payload = both[TOPIC + e.object + "/config"];
        TEST_ASSERT_FALSE_MESSAGE(payload.empty(), e.object);
        DynamicJsonDocument doc(2048);
        TEST_ASSERT_FALSE(deserializeJson(doc, payload));
        TEST_ASSERT_EQUAL_STRING(e.name, doc["name"]);
        TEST_ASSERT_EQUAL_STRING((std::string("sqmeter_aabbccddeeff_") + e.object).c_str(), doc["unique_id"]);
        TEST_ASSERT_EQUAL_STRING("sqmeter/state", doc["state_topic"]);
        TEST_ASSERT_EQUAL_STRING(e.value, doc["value_template"]);
        TEST_ASSERT_EQUAL_STRING("mag/arcsec²", doc["unit_of_measurement"]);
        TEST_ASSERT_EQUAL_STRING("sqmeter/availability", doc["availability"][0]["topic"]);
        TEST_ASSERT_EQUAL_STRING(e.availability, doc["availability"][1]["value_template"]);
        TEST_ASSERT_EQUAL_STRING("all", doc["availability_mode"]);
    }

    // Only the SQM: its entity; the SQM-L's and the difference removed.
    auto sqmOnly = discovery(true, false);
    TEST_ASSERT_FALSE(sqmOnly[TOPIC + "sqm_wide/config"].empty());
    TEST_ASSERT_TRUE(sqmOnly[TOPIC + "sqm_l/config"].empty());
    TEST_ASSERT_TRUE(sqmOnly[TOPIC + "sqm_delta/config"].empty());
    TEST_ASSERT_EQUAL(1, sqmOnly.count(TOPIC + "sqm_l/config")); // published empty: removed
    auto sqmlOnly = discovery(false, true);
    TEST_ASSERT_TRUE(sqmlOnly[TOPIC + "sqm_wide/config"].empty());
    TEST_ASSERT_FALSE(sqmlOnly[TOPIC + "sqm_l/config"].empty());
    TEST_ASSERT_TRUE(sqmlOnly[TOPIC + "sqm_delta/config"].empty());
    // The sky group off removes all three.
    auto noSky = discovery(true, true, false);
    for (const char *object : {"sqm_wide", "sqm_l", "sqm_delta"})
        TEST_ASSERT_TRUE(noSky[TOPIC + object + "/config"].empty());

    // The existing sky entities are the same whatever the sky sensors.
    auto none = discovery(false, false);
    for (const char *object : {"sqm", "nelm", "bortle", "illuminance"})
    {
        const std::string topic = TOPIC + object + "/config";
        TEST_ASSERT_EQUAL_STRING(none[topic].c_str(), both[topic].c_str());
        TEST_ASSERT_TRUE(both[topic].find("skySensors") == std::string::npos);
    }
    TEST_ASSERT_EQUAL_size_t(none.size(), both.size());
}
