#include <unity.h>

#include <ArduinoJson.h>

#include "DeviceCore.h"

#include <cstdio>
#include <string>

// /api/status fits Core::STATUS_DOC_CAPACITY in the worst case (spec 030
// FR-008, constitution IV): a full pool silently drops the trailing sections
// (diagnostics, MQTT, connections). The sections this library writes are
// written for real; the device's own (src/WebServerStatus.cpp) are modelled
// member for member with their longest strings. Measured on the host, where
// ArduinoJson's slots are bigger than on the ESP32, so the device has more room.

using namespace SQM;

namespace
{
    const std::string LONG(64, 'x'); // the longest saved settings strings (test_config_model)

    void okSky(SkySensorSnapshot &sky, uint32_t now)
    {
        sky.initialized = true;
        sky.reading.status = SensorStatus::Ok;
        sky.reading.timestamp = now - 400;
        sky.reading.lux = 0.0004F;
        sky.lastUpdate = now - 400;
        sky.diagnostics.gainName = "max";
        sky.diagnostics.integrationMs = 600;
        sky.diagnostics.averagingWindowSeconds = 90;
        sky.diagnostics.sampleCount = 150;
        sky.diagnostics.nightMode = true;
    }

    // Every sensor on and answering, both sky sensors.
    SensorSnapshot everything(uint32_t now)
    {
        SensorSnapshot s;
        okSky(s.skySensors[0], now);
        okSky(s.skySensors[1], now);
        s.bmeInitialized = s.mlxInitialized = s.gpsInitialized = s.rg15Initialized = true;
        s.bme.status = s.mlx.status = s.gps.status = s.rg15.status = SensorStatus::Ok;
        s.bme.timestamp = s.mlx.timestamp = s.gps.timestamp = s.rg15.timestamp = now - 400;
        s.bmeLastUpdate = s.mlxLastUpdate = s.gpsLastUpdate = s.rg15LastUpdate = now - 400;
        s.rg15.online = true;
        s.gps.hasFix = true;
        s.dataTimestamp = now - 400;
        return s;
    }

    Config everythingOn()
    {
        Config c = Config::createDefault();
        c.sensor.i2c2SDA = 25;
        c.sensor.i2c2SCL = 26;
        c.sky.sqmlEnabled = true;
        c.gps.enabled = c.rain.enabled = c.wind.enabled = c.wind.directionEnabled = true;
        c.alpaca.enabled = c.alerts.enabled = c.mqtt.enabled = true;
        c.location.set = true;
        c.location.latitude = 51.48;
        c.ntp.timezone = LONG;
        c.wifi.hostname = std::string(32, 'h');
        return c;
    }

    // src/WebServerStatus.cpp appendFirmware, appendRuntime, appendMemory,
    // appendPartitions, appendTime, appendWifi (with Ipv6Network::appendStatus),
    // appendMqttStatus and appendConnections. A std::string or String value is
    // copied into the pool; a const char * is not.
    void deviceSections(JsonDocument &doc, const Config &cfg)
    {
        JsonObject firmware = doc.createNestedObject("firmware");
        for (const char *key : {"name", "version", "buildDate", "buildTime", "variant", "layout"})
            firmware[key] = "literal";
        JsonObject ble = doc.createNestedObject("ble");
        ble["available"] = ble["active"] = true;
        ble["clients"] = 3;
        JsonObject alarm = ble.createNestedObject("alarm");
        for (const char *key : {"serviceActive", "active", "sequence", "acknowledgedSequence", "bondedPhones"})
            alarm[key] = 1;

        for (const char *key : {"uptime", "configRevision", "freeHeap", "sensorSnapshotBytes"})
            doc[key] = 4294967295U;
        JsonObject stacks = doc.createNestedObject("stackFree");
        stacks["asyncTcp"] = stacks["loop"] = 4096;

        JsonArray heapStages = doc.createNestedArray("heapStages");
        for (int i = 0; i < 16; ++i) // HeapTrace::MAX_CHECKPOINTS
        {
            JsonObject stage = heapStages.createNestedObject();
            stage["stage"] = "literal";
            stage["free"] = stage["largest"] = 4294967295U;
        }
        for (const char *key :
             {"minFreeHeap",
              "maxAllocHeap",
              "heapSize",
              "cpuFreqMHz",
              "flashSize",
              "sketchSize",
              "freeSketchSpace",
              "resetReason",
              "bootCount",
              "fsTotal",
              "fsUsed"})
            doc[key] = 4294967295U;

        JsonObject partitions = doc.createNestedObject("partitions");
        for (const char *key : {"runningSlot", "bootSlot", "nextSlot"})
            partitions[key] = "app0"; // esp_partition_t::label: a char array, referenced
        for (const char *key : {"runningAddress", "runningSize", "nextSize", "fsAddress", "fsSize"})
            partitions[key] = 4294967295U;
        JsonObject nvs = partitions.createNestedObject("nvs");
        for (const char *key : {"usedEntries", "freeEntries", "totalEntries", "namespaceCount"})
            nvs[key] = 4294967295U;

        JsonObject time = doc.createNestedObject("time");
        time["epoch"] = 4294967295U;
        time["iso"] = std::string("2026-10-17T23:59:59+14:00");
        time["timezone"] = cfg.ntp.timezone;
        JsonObject ntp = doc.createNestedObject("ntp");
        for (const char *key :
             {"enabled", "synced", "status", "lastSync", "nextSync", "drift", "activeSource", "gpsEnabled", "gpsHasFix", "gpsSatellites"})
            ntp[key] = 4294967295U;
        ntp["server"] = LONG;
        ntp["gpsTimeUTC"] = std::string("2026-10-17T23:59:59Z");

        JsonObject wifi = doc.createNestedObject("wifi");
        for (const char *key : {"connected", "rssi", "connectPending", "apMode", "mdns"})
            wifi[key] = -100;
        wifi["ssid"] = std::string(32, 's');
        wifi["ip"] = std::string("255.255.255.255");
        wifi["mac"] = std::string("AA:BB:CC:DD:EE:FF");
        wifi["hostname"] = cfg.wifi.hostname;
        JsonObject ipv6 = wifi.createNestedObject("ipv6");
        ipv6["enabled"] = true;
        JsonArray addresses = ipv6.createNestedArray("addresses");
        for (int i = 0; i < 3; ++i) // LWIP_IPV6_NUM_ADDRESSES
        {
            JsonObject entry = addresses.createNestedObject();
            entry["address"] = std::string("ffff:ffff:ffff:ffff:ffff:ffff:ffff:ffff");
            entry["scope"] = "global";
        }

        JsonObject mqtt = doc.createNestedObject("mqtt");
        for (const char *key : {"enabled", "connected", "state", "lastPublish", "lastReconnectAttempt", "port"})
            mqtt[key] = 4294967295U;
        mqtt["broker"] = LONG;
        mqtt["topic"] = LONG;
        mqtt["availabilityTopic"] = LONG + "/availability";
        mqtt["clientId"] = std::string("SQMeter-AABBCC");

        JsonObject connections = doc.createNestedObject("connections");
        connections["tcpLimit"] = 16;
        JsonObject live = connections.createNestedObject("liveUpdates");
        for (const char *key : {"sensors", "status", "limitPerEndpoint", "replaced", "stalledClosed"})
            live[key] = 4294967295U;
        connections["alpacaRestoredAfterRestart"] = true;
    }

    // Every member and element takes one slot.
    size_t slots(JsonVariantConst value)
    {
        size_t count = 0;
        if (value.is<JsonObjectConst>())
            for (JsonPairConst member : value.as<JsonObjectConst>())
                count += 1 + slots(member.value());
        else if (value.is<JsonArrayConst>())
            for (JsonVariantConst element : value.as<JsonArrayConst>())
                count += 1 + slots(element);
        return count;
    }

    // ArduinoJson's slot is 16 bytes on the ESP32 (32-bit pointers) and bigger
    // on the host: what the same document takes on the device.
    constexpr size_t DEVICE_SLOT_BYTES = 16;
    size_t onDevice(const JsonDocument &doc)
    {
        const size_t hostSlot = JSON_OBJECT_SIZE(1);
        return doc.memoryUsage() - slots(doc.as<JsonVariantConst>()) * (hostSlot - DEVICE_SLOT_BYTES);
    }

    // The library's sections, as createStatusJson writes them.
    void coreSections(JsonDocument &doc, const Config &cfg, uint32_t now)
    {
        SensorSnapshot s = everything(now);
        Core::derive(s, cfg);
        const Readings::Snapshot readings = Core::buildReadings(s, cfg, now, 1791417600);
        Core::writeSky(doc.createNestedObject("sky"), Core::night(s, cfg, 1791417600));
        JsonObject alerts = doc.createNestedObject("alerts");
        Alerts::ScheduleState schedule;
        schedule.sending = false;
        schedule.reason = Alerts::ScheduleReason::WaitingForClient;
        schedule.sinceKnown = true;
        schedule.sinceEpoch = 1791417600;
        Core::writeAlertSchedule(alerts, schedule, cfg, now);
        alerts["recentRevision"] = 4294967295U;
        Core::writeClientWatch(doc.createNestedObject("alpaca"), Alpaca::ClientWatch{}, cfg, now);
        Core::writeSensorHealth(doc.createNestedObject("sensors"), readings, cfg);
        Core::writeSkySensorStatus(doc.as<JsonObject>(), s, readings);
        Core::writeDiagnostics(doc.createNestedObject("diagnostics"), s, cfg, now);
        Config withDisplay = cfg;
        withDisplay.display.enabled = true;
        Core::DisplayStatus display;
        display.detected = display.on = display.nightMode = true;
        display.page = Display::Page::Environment;
        Core::writeDisplayStatus(doc.as<JsonObject>(), withDisplay, display);
    }
} // namespace

void test_status_document_fits()
{
    const uint32_t now = 100000;
    const Config cfg = everythingOn();
    DynamicJsonDocument doc(Core::STATUS_DOC_CAPACITY * 2);
    deviceSections(doc, cfg);
    coreSections(doc, cfg, now);
    TEST_ASSERT_FALSE(doc.overflowed());
    TEST_ASSERT_TRUE(doc["skySensors"].containsKey("sqml"));
    TEST_ASSERT_TRUE(doc["diagnostics"].size() > 0);
    TEST_ASSERT_EQUAL_STRING("environment", doc["display"]["page"]);

    // With 20% headroom, like the readings document.
    const size_t used = onDevice(doc);
    char detail[96];
    snprintf(detail, sizeof detail, "pool %u bytes of %u", static_cast<unsigned>(used), static_cast<unsigned>(Core::STATUS_DOC_CAPACITY));
    TEST_ASSERT_TRUE_MESSAGE(used * 12 / 10 <= Core::STATUS_DOC_CAPACITY, detail);
    printf("status document on the device: %s\n", detail);
}

// SC-001, FR-014: what a real one-sensor device emits. Its SQM is enabled by
// default, so the readings gain skySensors.sqm (no sqml, no deltaSqm) and
// sky.source; every existing field keeps its name, type and value, and the
// SQM's own entry repeats the authoritative values.
void test_one_sensor_device_readings_document()
{
    const uint32_t now = 100000;
    const Config cfg = Config::createDefault();
    SensorSnapshot s;
    okSky(s.skySensors[0], now);
    Core::derive(s, cfg);
    const Readings::Snapshot readings = Core::buildReadings(s, cfg, now, 1791417600);
    DynamicJsonDocument doc(Readings::DOC_CAPACITY);
    Readings::write(doc.to<JsonObject>(), readings);

    JsonObject sky = doc["sky"];
    TEST_ASSERT_EQUAL_STRING("ok", sky["status"]);
    TEST_ASSERT_EQUAL_STRING("sqm", sky["source"]);
    for (const char *key : {"sqm", "rawSqm", "nelm", "bortle", "description", "calibrated", "averagingWindowSeconds"})
        TEST_ASSERT_TRUE_MESSAGE(sky.containsKey(key), key);
    TEST_ASSERT_EQUAL_STRING("ok", doc["light"]["status"]);
    TEST_ASSERT_TRUE(doc["light"].containsKey("lux"));

    JsonObject skySensors = doc["skySensors"];
    TEST_ASSERT_EQUAL(1, skySensors.size());
    JsonObject sqm = skySensors["sqm"];
    TEST_ASSERT_EQUAL_STRING("ok", sqm["status"]);
    TEST_ASSERT_EQUAL_FLOAT((float)sky["sqm"].as<double>(), (float)sqm["sqm"].as<double>());
    TEST_ASSERT_EQUAL_FLOAT((float)sky["nelm"].as<double>(), (float)sqm["nelm"].as<double>());
    TEST_ASSERT_EQUAL(sky["bortle"].as<int>(), sqm["bortle"].as<int>());
    TEST_ASSERT_EQUAL_FLOAT((float)doc["light"]["lux"].as<double>(), (float)sqm["lux"].as<double>());

    // /api/status: the SQM's own block beside the unchanged sensors.light.
    DynamicJsonDocument status(Core::STATUS_DOC_CAPACITY);
    JsonObject root = status.to<JsonObject>();
    Core::writeSensorHealth(root.createNestedObject("sensors"), readings, cfg);
    Core::writeSkySensorStatus(root, s, readings);
    TEST_ASSERT_EQUAL_STRING("ok", root["sensors"]["light"]["status"]);
    TEST_ASSERT_EQUAL(1, root["skySensors"].size());
    TEST_ASSERT_TRUE(root["skySensors"]["sqm"]["detected"].as<bool>());
}
