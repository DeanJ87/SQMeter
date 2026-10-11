#include "WebServer.h"
#include "ConnectionBudget.h"
#include "WebServerShared.h"
#include "Logger.h"
#include "version.h"
#include <WiFi.h>
#include <Update.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <AsyncJson.h>
#include <PubSubClient.h>
#include <esp_partition.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include <esp_timer.h>
#include <cstring>
#include <nvs.h>
#include <nvs_flash.h>
#include <ctime>
#include "calculations/CloudDetection.h"
#include "sensors/RG15Sensor.h"
#include "AlpacaDiscovery.h"
#include "Ipv6Network.h"
#include "DualStackClient.h"
#include "NetAddress.h"
#include "WiFiManager.h"
#include "HeapTrace.h"
#include "SunPosition.h"
#include "SafetyHistory.h"
#include "FirmwareMarker.h"
#include <Preferences.h>

extern uint32_t bootCount;

// Documents the device publishes: readings, /api/status, MQTT readings and Home Assistant discovery.

namespace SQM
{
    using namespace WebShared;

    void WebServer::appendDiagnostics(JsonObject root, const SensorSnapshot &snapshot) const
    {
        Core::writeDiagnostics(root, snapshot, getConfigCallback(), millis());
    }

    // MQTT <base>/state (+ /diagnostics) every publish interval, and Home
    // Assistant discovery whenever the connection or the settings change.
    void WebServer::publishMqttReadings(uint32_t now)
    {
        if (mqttClient == nullptr || !mqttClient->isConnected())
            return;
        const MQTTConfig &mqtt = getConfigCallback().mqtt;
        Readings::Groups groups;
        groups.sky = mqtt.publish.sky;
        groups.environment = mqtt.publish.environment;
        groups.clouds = mqtt.publish.clouds;
        groups.gps = mqtt.publish.gps;
        groups.rain = mqtt.publish.rain;
        groups.wind = mqtt.publish.wind;
        const Config &cfg = getConfigCallback();
        const BleAlarmStatus alarm = ble.alarmStatus();
        groups.alertsSwitch = cfg.alerts.enabled || (cfg.ble.enabled && alarm.serviceActive && alarm.bondedPhones > 0); // dep: D-13

        publishDiscovery(mqtt, groups);

        const bool reconnected = mqttClient->connectionCount() != mqttStateConnection;
        if (!reconnected && mqttStatePublishedAt != 0 && now - mqttStatePublishedAt < mqtt.publishIntervalMs)
            return;
        DynamicJsonDocument doc(3072);
        Readings::write(doc.to<JsonObject>(), buildReadings(), groups);
        std::string payload;
        serializeJson(doc, payload);
        if (!mqttClient->publishSubtopic("state", payload, true))
        {
            Logger::warn(TAG, "MQTT state publish failed (%u bytes)", static_cast<unsigned>(payload.size()));
            return;
        }
        mqttStatePublishedAt = now == 0 ? 1 : now;
        mqttStateConnection = mqttClient->connectionCount();

        if (mqtt.publish.diagnostics)
        {
            DynamicJsonDocument diag(1536);
            appendDiagnostics(diag.to<JsonObject>(), getSensorSnapshot());
            std::string diagPayload;
            serializeJson(diag, diagPayload);
            mqttClient->publishSubtopic("diagnostics", diagPayload, false);
        }
    }

    void WebServer::publishDiscovery(const MQTTConfig &mqtt, const Readings::Groups &groups)
    {
        // Rebuilt from scratch when anything it depends on changes.
        char mac[13];
        snprintf(mac, sizeof(mac), "%012llx", static_cast<unsigned long long>(ESP.getEfuseMac()));
        Readings::DiscoveryDevice device{
            std::string("sqmeter_") + mac, getConfigCallback().deviceName, FIRMWARE_VERSION, mqtt.topic, mqtt.discoveryPrefix};
        std::string key = std::to_string(mqtt.homeAssistant) + device.name + device.baseTopic + device.prefix;
        for (bool on : {groups.sky, groups.environment, groups.clouds, groups.rain, groups.wind, mqtt.publish.safety, groups.alertsSwitch})
            key += on ? '1' : '0';
        if (key == discoveryKey && mqttClient->connectionCount() == discoveryConnection)
            return;

        auto publish = [this](const std::string &topic, const std::string &payload) { mqttClient->publishTopic(topic, payload, true); };
        if (!discoveryKey.empty() && discoveryWasOn &&
            (!mqtt.homeAssistant || discoveryDevice.prefix != device.prefix || discoveryDevice.baseTopic != device.baseTopic))
        {
            // Remove what was announced under the old settings.
            Readings::forEachDiscovery(
                discoveryDevice, groups, true, [&](const std::string &topic, const std::string &) { publish(topic, ""); });
        }
        if (mqtt.homeAssistant)
            Readings::forEachDiscovery(device, groups, mqtt.publish.safety, publish);

        discoveryKey = key;
        discoveryConnection = mqttClient->connectionCount();
        discoveryDevice = device;
        discoveryWasOn = mqtt.homeAssistant;
    }

    Deps::Facts WebServer::settingsFacts(const SensorSnapshot &snapshot) const
    {
        Deps::Facts facts;
        Core::sensorFacts(
            facts, snapshot, Core::buildReadings(snapshot, getConfigCallback(), millis(), static_cast<int64_t>(time(nullptr))));
        facts.wifiConnected = WiFi.status() == WL_CONNECTED;
        facts.mqttConnected = mqttClient != nullptr && mqttClient->isConnected();
        facts.clockSet = static_cast<int64_t>(time(nullptr)) >= Core::CLOCK_VALID_EPOCH;
        facts.bluetoothBuild = BleService::available();
        facts.bluetoothRunning = ble.isActive();
        const uint32_t phones = ble.alarmStatus().bondedPhones;
        facts.pairedPhones = static_cast<uint8_t>(phones > 255 ? 255 : phones);
        return facts;
    }

    ChannelBlocks WebServer::channelBlocks(const SensorSnapshot &snapshot) const
    {
        const std::vector<Deps::Entry> entries = Deps::evaluate(getConfigCallback(), settingsFacts(snapshot));
        auto blocked = [&entries](const char *setting) -> const char *
        {
            const Deps::Reason *reason = Deps::reasonFor(entries, setting);
            return reason != nullptr && std::strcmp(reason->code, "alerts-off") != 0 ? reason->text : nullptr;
        };
        ChannelBlocks blocks{};
        blocks[static_cast<size_t>(AlertChannel::Mqtt)] = blocked("alerts.mqtt.enabled");         // dep: D-01 D-02
        blocks[static_cast<size_t>(AlertChannel::Pushover)] = blocked("alerts.pushover.enabled"); // dep: D-03
        blocks[static_cast<size_t>(AlertChannel::Ntfy)] = blocked("alerts.ntfy.enabled");
        blocks[static_cast<size_t>(AlertChannel::Webhook)] = blocked("alerts.webhook.enabled");
        return blocks;
    }

    void WebServer::appendSafetyStatus(JsonObject target) const
    {
        Core::writeSafety(target, getSafetyStatus(), getConfigCallback(), millis());
    }

    Readings::Snapshot WebServer::buildReadings() const
    {
        return Core::buildReadings(getSensorSnapshot(), getConfigCallback(), millis(), static_cast<int64_t>(time(nullptr)));
    }

    std::string WebServer::createSensorDataJson() const
    {
        DynamicJsonDocument doc(4096);
        Readings::write(doc.to<JsonObject>(), buildReadings());
        appendSafetyStatus(doc.createNestedObject("safety"));
        std::string json;
        serializeJson(doc, json);
        return json;
    }

    namespace
    {
        void appendMemory(JsonDocument &doc)
        {
            JsonArray heapStages = doc.createNestedArray("heapStages");
            for (size_t i = 0; i < HeapTrace::count(); ++i)
            {
                const HeapTrace::Checkpoint &checkpoint = HeapTrace::at(i);
                JsonObject stage = heapStages.createNestedObject();
                stage["stage"] = checkpoint.stage;
                stage["free"] = checkpoint.freeBytes;
                stage["largest"] = checkpoint.largestBlock;
            }
            doc["minFreeHeap"] = ESP.getMinFreeHeap();
            doc["maxAllocHeap"] = ESP.getMaxAllocHeap();
            doc["heapSize"] = ESP.getHeapSize();
            doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
            doc["flashSize"] = ESP.getFlashChipSize();
            doc["sketchSize"] = ESP.getSketchSize();
            doc["freeSketchSpace"] = ESP.getFreeSketchSpace();
            doc["resetReason"] = static_cast<int>(esp_reset_reason());
            doc["bootCount"] = ::bootCount;

            // Filesystem stats
            doc["fsTotal"] = LittleFS.totalBytes();
            doc["fsUsed"] = LittleFS.usedBytes();
        }

        void appendPartitions(JsonDocument &doc)
        {
            // Partition information
            JsonObject partitions = doc.createNestedObject("partitions");

            // Get running OTA partition
            const esp_partition_t *running = esp_ota_get_running_partition();
            const esp_partition_t *boot = esp_ota_get_boot_partition();

            if (running)
            {
                partitions["runningSlot"] = running->label;
                partitions["runningAddress"] = running->address;
                partitions["runningSize"] = running->size;
            }

            if (boot)
            {
                partitions["bootSlot"] = boot->label;
            }

            // Get next OTA partition info
            const esp_partition_t *next = esp_ota_get_next_update_partition(NULL);
            if (next)
            {
                partitions["nextSlot"] = next->label;
                partitions["nextSize"] = next->size;
            }

            // Get NVS partition stats
            nvs_stats_t nvs_stats;
            if (nvs_get_stats(NULL, &nvs_stats) == ESP_OK)
            {
                JsonObject nvs = partitions.createNestedObject("nvs");
                nvs["usedEntries"] = nvs_stats.used_entries;
                nvs["freeEntries"] = nvs_stats.free_entries;
                nvs["totalEntries"] = nvs_stats.total_entries;
                nvs["namespaceCount"] = nvs_stats.namespace_count;
            }

            // Get LittleFS partition info
            const esp_partition_t *fs_partition =
                esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
            if (fs_partition)
            {
                partitions["fsAddress"] = fs_partition->address;
                partitions["fsSize"] = fs_partition->size;
            }
        }
    } // namespace

    void WebServer::appendFirmware(JsonDocument &doc) const
    {
        // Firmware version
        JsonObject firmware = doc.createNestedObject("firmware");
        firmware["name"] = FIRMWARE_NAME;
        firmware["version"] = FIRMWARE_VERSION;
        firmware["buildDate"] = FIRMWARE_BUILD_DATE;
        firmware["buildTime"] = FIRMWARE_BUILD_TIME;
        firmware["variant"] = BleService::available() ? "ble" : "standard";
        // Spec 027: "l2" (the whole-chip layout), or "legacy" when the device
        // still needs the one-time USB flash.
        firmware["layout"] = FirmwareMarker::layout();

        JsonObject bleStatus = doc.createNestedObject("ble");
        bleStatus["available"] = BleService::available();
        bleStatus["active"] = ble.isActive();
        bleStatus["clients"] = ble.connectedClients();
        const BleAlarmStatus bleAlarm = ble.alarmStatus();
        JsonObject bleAlarmJson = bleStatus.createNestedObject("alarm");
        bleAlarmJson["serviceActive"] = bleAlarm.serviceActive;
        bleAlarmJson["active"] = bleAlarm.alarmActive;
        bleAlarmJson["sequence"] = bleAlarm.sequence;
        bleAlarmJson["acknowledgedSequence"] = bleAlarm.acknowledgedSequence;
        bleAlarmJson["bondedPhones"] = bleAlarm.bondedPhones;
    }

    void WebServer::appendRuntime(JsonDocument &doc) const
    {
        // System stats
        doc["uptime"] = millis() / 1000;
        doc["configRevision"] = configRevision.load();
        doc["freeHeap"] = ESP.getFreeHeap();
        // Stack headroom (bytes never used). This handler runs on the
        // AsyncTCP task, so "asyncTcp" is that task's own high-water mark.
        JsonObject stacks = doc.createNestedObject("stackFree");
        stacks["asyncTcp"] = uxTaskGetStackHighWaterMark(nullptr);
        stacks["loop"] = loopTaskHandle != nullptr ? uxTaskGetStackHighWaterMark(loopTaskHandle) : 0;
        doc["sensorSnapshotBytes"] = sizeof(SensorSnapshot);
    }

    void WebServer::appendTime(JsonDocument &doc) const
    {
        // Current time: `epoch` in Unix seconds like every other timestamp
        // (0 until the clock is set, spec 013 FR-003); `iso` is the local time
        // with its offset, for display.
        JsonObject timeObj = doc.createNestedObject("time");
        const time_t epochNow = time(nullptr);
        timeObj["epoch"] = epochNow >= Core::CLOCK_VALID_EPOCH ? static_cast<uint32_t>(epochNow) : 0U;
        if (timeManager)
        {
            timeObj["iso"] = timeManager->getCurrentTimeISO();
            timeObj["timezone"] = getConfigCallback().ntp.timezone;
        }
        else
        {
            time_t now;
            time(&now);
            struct tm timeinfo;
            localtime_r(&now, &timeinfo);

            char buffer[32];
            strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &timeinfo);
            timeObj["iso"] = buffer;
            timeObj["timezone"] = getConfigCallback().ntp.timezone;
        }

        // NTP/GPS Time status
        if (timeManager)
        {
            TimeStatus timeStatus = timeManager->getStatus();
            JsonObject ntp = doc.createNestedObject("ntp");
            ntp["enabled"] = timeStatus.ntpEnabled;
            ntp["synced"] = (timeStatus.syncStatus == NTPSyncStatus::SYNCED);
            ntp["status"] = static_cast<int>(timeStatus.syncStatus);
            ntp["lastSync"] = timeStatus.lastSyncMs;
            ntp["nextSync"] = timeStatus.nextSyncMs;
            ntp["drift"] = timeStatus.driftSeconds;
            ntp["server"] = timeStatus.server;
            ntp["activeSource"] = static_cast<int>(timeStatus.activeSource); // 0=None, 1=NTP, 2=GPS
            ntp["gpsEnabled"] = timeStatus.gpsEnabled;
            ntp["gpsHasFix"] = timeStatus.gpsHasFix;
            ntp["gpsTimeUTC"] = timeStatus.gpsTimeUTC;
            ntp["gpsSatellites"] = timeStatus.gpsSatellites;
        }
    }

    void WebServer::appendWifi(JsonDocument &doc) const
    {
        // WiFi status
        JsonObject wifi = doc.createNestedObject("wifi");
        wifi["connected"] = WiFi.isConnected();
        wifi["ssid"] = WiFi.SSID();
        wifi["ip"] = WiFi.localIP().toString();
        wifi["rssi"] = WiFi.RSSI();
        wifi["mac"] = WiFi.macAddress();
        wifi["connectPending"] = wifiConnectActive;
        wifi["apMode"] = (WiFi.getMode() & WIFI_AP) != 0;
        {
            const Config &cfg = getConfigCallback();
            wifi["hostname"] = cfg.wifi.hostname;
            wifi["mdns"] = cfg.wifi.mdns;
        }
        Ipv6Network::appendStatus(wifi);
    }

    void WebServer::appendMqttStatus(JsonDocument &doc) const
    {
        if (!mqttClient)
            return;
        MQTTStatus mqttStatus = mqttClient->getStatus();
        JsonObject mqtt = doc.createNestedObject("mqtt");
        mqtt["enabled"] = mqttStatus.enabled;
        mqtt["connected"] = mqttStatus.connected;
        mqtt["state"] = mqttStatus.state;
        mqtt["lastPublish"] = mqttStatus.lastPublishMs;
        mqtt["lastReconnectAttempt"] = mqttStatus.lastReconnectAttemptMs;
        mqtt["broker"] = mqttStatus.broker; // std::string: copied into the doc (mqttStatus dies before serializing)
        mqtt["port"] = mqttStatus.port;
        mqtt["topic"] = mqttStatus.topic;
        mqtt["availabilityTopic"] = mqttStatus.availabilityTopic;
        mqtt["clientId"] = mqttStatus.clientId;
    }

    void WebServer::appendConnections(JsonDocument &doc) const
    {
        // Who holds the device's connections (spec 011 FR-008, 013 FR-012).
        JsonObject connections = doc.createNestedObject("connections");
        connections["tcpLimit"] = ConnectionBudget::TCP_CONNECTIONS;
        JsonObject live = connections.createNestedObject("liveUpdates");
        live["sensors"] = wsSensors.count();
        live["status"] = wsStatus.count();
        live["limitPerEndpoint"] = ConnectionBudget::WEBSOCKETS_PER_ENDPOINT;
        live["replaced"] = wsReplaced;
        live["stalledClosed"] = wsStalled;
        connections["alpacaRestoredAfterRestart"] = alpacaConnectionsRestored;
    }

    std::string WebServer::createStatusJson() const
    {
        DynamicJsonDocument doc(7168); // Includes MQTT, partition, boot, sensor, BLE and alert-schedule diagnostics
        const SensorSnapshot snapshot = getSensorSnapshot();
        appendFirmware(doc);
        appendRuntime(doc);
        Core::writeSky(doc.createNestedObject("sky"), computeNight(snapshot, getConfigCallback()));
        JsonObject alerts = doc.createNestedObject("alerts");
        Core::writeAlertSchedule(alerts, sharedSchedule(), getConfigCallback(), millis());
        alerts["recentRevision"] = alertDispatcher ? alertDispatcher->recentRevision() : 0;
        Core::writeClientWatch(doc.createNestedObject("alpaca"), sharedClientWatch(), getConfigCallback(), millis());
        appendMemory(doc);
        appendPartitions(doc);
        appendTime(doc);
        appendWifi(doc);

        // Per-sensor health for present hardware, and bring-up diagnostics.
        // Readings themselves are in /api/sensors.
        const Readings::Snapshot readings = buildReadings();
        Core::writeSensorHealth(doc.createNestedObject("sensors"), readings, getConfigCallback());
        JsonObject diagnostics = doc.createNestedObject("diagnostics");
        appendDiagnostics(diagnostics, snapshot);
        appendMqttStatus(doc);
        appendConnections(doc);

        std::string json;
        serializeJson(doc, json);
        return json;
    }

} // namespace SQM
