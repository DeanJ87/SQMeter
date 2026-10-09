#include "WebServer.h"
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
#include <Preferences.h>

extern uint32_t bootCount;

namespace SQM
{
    namespace
    {
        constexpr size_t CONFIG_JSON_BUFFER_SIZE = 12288; // full config incl. custom alert texts

        constexpr const char *ARMED_NVS_NAMESPACE = "sqm-alerts";

        // Where captive-portal probes land: the WiFi setup screen on the hotspot.
        std::string setupScreenUrl()
        {
            return std::string("http://") + WiFi.softAPIP().toString().c_str() + "/wifi";
        }

        // "1"/"0", "on"/"off", "true"/"false", "arm"/"disarm" (any case).
        bool parseArmPayload(std::string text, bool &armed)
        {
            while (!text.empty() && isspace(static_cast<unsigned char>(text.back())))
                text.pop_back();
            for (char &c : text)
                c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            if (text == "1" || text == "on" || text == "true" || text == "arm" || text == "armed")
                armed = true;
            else if (text == "0" || text == "off" || text == "false" || text == "disarm" || text == "disarmed")
                armed = false;
            else
                return false;
            return true;
        }

        // Test sends that only ring paired phones (no push channel enabled).
        constexpr uint8_t BLE_ONLY_TEST = 0x80;

        esp_timer_handle_t restartTimer = nullptr;

        void restartTimerCallback(void *)
        {
            ESP.restart();
        }

    } // namespace

    WebServer::WebServer(
        TSL2591Sensor &tsl,
        BME280Sensor &bme,
        MLX90614Sensor &mlx,
        GPSSensor &gps,
        RG15Sensor &rg15,
        WindSensor &wind,
        TimeManager *timeMgr,
        MQTTClient *mqtt,
        GetConfigCallback getConfig,
        SaveConfigCallback saveConfig)
        : server(PORT),
          wsSensors("/ws/sensors"),
          wsStatus("/ws/status"),
          tslSensor(tsl),
          bmeSensor(bme),
          mlxSensor(mlx),
          gpsSensor(gps),
          rg15Sensor(rg15),
          windSensor(wind),
          timeManager(timeMgr),
          mqttClient(mqtt),
          getConfigCallback(getConfig),
          saveConfigCallback(saveConfig),
          lastSensorBroadcast(0),
          lastStatusBroadcast(0),
          sensorSnapshotMutex(xSemaphoreCreateMutex()),
          wifiConnectActive(false),
          wifiConnectConfigSaved(false),
          wifiConnectStartedAt(0)
    {
        refreshSensorSnapshot(0);

        alertDispatcher = std::make_unique<AlertDispatcher>(
            mqttClient,
            [this]
            {
                const OtaUpdater::Phase phase = otaUpdater ? otaUpdater->phase() : OtaUpdater::Phase::Idle;
                return phase == OtaUpdater::Phase::Downloading || phase == OtaUpdater::Phase::Writing;
            });

        otaUpdater = std::make_unique<OtaUpdater>(
            [this](int percent) { setOTAProgress(percent); },
            [this](const char *message) { setOTAError(message); },
            [] { WebServer::scheduleRestart(1000); });

        languagePack = std::make_unique<LanguagePack>(*this);
    }

    WebServer::~WebServer()
    {
        wsSensors.closeAll();
        wsStatus.closeAll();
        if (sensorSnapshotMutex)
        {
            vSemaphoreDelete(sensorSnapshotMutex);
            sensorSnapshotMutex = nullptr;
        }
        if (safetyMutex)
        {
            vSemaphoreDelete(safetyMutex);
            safetyMutex = nullptr;
        }
    }

    void WebServer::begin()
    {
        loopTaskHandle = xTaskGetCurrentTaskHandle();

        {
            Preferences prefs;
            if (prefs.begin(ARMED_NVS_NAMESPACE, true))
            {
                // No "reason" key: saved by firmware from before reasons.
                alertSchedule.restore(
                    prefs.getBool("armed", true),
                    prefs.isKey("reason"),
                    prefs.getUChar("reason", 0),
                    static_cast<int64_t>(prefs.getUInt("since", 0)));
                prefs.end();
            }
            shareSchedule();
        }
        if (mqttClient != nullptr)
        {
            // Home Assistant MQTT switch: command <topic>/alerts/armed/set,
            // state <topic>/alerts/armed.
            mqttClient->onCommand(
                "alerts/armed/set",
                [this](const std::string &payload)
                {
                    bool armed = false;
                    if (parseArmPayload(payload, armed))
                        pendingArm = encodeArm(armed, Alerts::ScheduleReason::UserMqtt);
                });
        }
        Logger::info(TAG, "Starting web server on port %d", PORT);

        // CRITICAL: Register API routes BEFORE static file serving
        // Otherwise /api/* requests get treated as filesystem paths
        setupAPIRoutes();
        setupWebSocket();
        setupOTA();
        setupGithubUpdates();
        setupAlpacaRoutes();
        setupAlertRoutes();
        languagePack->registerRoutes(server);
        setupStaticRoutes(); // Must be last - has catch-all serveStatic

        HeapTrace::mark("web server routes");
        alertDispatcher->begin();
        HeapTrace::mark("alert dispatcher");

        if (BleService::available() && !getConfigCallback().ble.enabled)
        {
            BleService::releaseControllerMemory();
            HeapTrace::mark("bluetooth memory released");
        }
        if (BleService::available() && getConfigCallback().ble.enabled)
        {
            ble.begin(getConfigCallback().deviceName, getConfigCallback().ble.passkey);
            HeapTrace::mark("bluetooth");
        }

        if (getConfigCallback().alpaca.enabled)
        {
            // With IPv6 on, one dual-stack socket takes IPv4 broadcasts and
            // the IPv6 discovery group (spec 015, FR-005).
            const bool ipv6 = WiFiManager::ipv6Running();
            const bool listening = ipv6 ? alpacaDiscoveryUdp.listen(IP_ANY_TYPE, Alpaca::DISCOVERY_UDP_PORT)
                                        : alpacaDiscoveryUdp.listen(Alpaca::DISCOVERY_UDP_PORT);
            alpacaIpv6Pending = listening && ipv6;
            if (listening)
            {
                // Replies straight from the UDP task: discovery answers
                // within milliseconds instead of waiting for a main-loop pass.
                alpacaDiscoveryUdp.onPacket(
                    [](AsyncUDPPacket &packet)
                    {
                        if (!Alpaca::isValidDiscoveryRequest(packet.data(), packet.length()) || !Ipv6Network::allowedDiscoveryPeer(packet))
                            return;
                        const std::string response = Alpaca::buildDiscoveryResponse(PORT);
                        packet.write(reinterpret_cast<const uint8_t *>(response.data()), response.size());
                    });
                Logger::info(TAG, "Alpaca UDP discovery listening on port %u", Alpaca::DISCOVERY_UDP_PORT);
            }
            else
            {
                Logger::error(TAG, "Failed to start Alpaca UDP discovery listener");
            }
        }

        // SPA fallback - serve index.html for any non-API routes
        server.onNotFound(
            [](AsyncWebServerRequest *request)
            {
                String path = request->url();
                // Alpaca device API: the spec requires HTTP 400 with a plain-text
                // body for an unknown device type/number, method, or HTTP verb.
                if (path.startsWith("/api/v1/"))
                {
                    Logger::debug(TAG, "400 Invalid Alpaca request: %s %s", request->methodToString(), path.c_str());
                    request->send(400, "text/plain", "Invalid Alpaca device type, device number, method or HTTP verb");
                    return;
                }
                // In setup mode every hostname resolves here; send other sites'
                // pages to the setup screen.
                if ((WiFi.getMode() & WIFI_AP) && !path.startsWith("/api/") && request->host() != WiFi.softAPIP().toString())
                {
                    request->redirect(setupScreenUrl().c_str());
                    return;
                }
                // If it's an API route, return 404 JSON
                if (path.startsWith("/api/"))
                {
                    Logger::debug(TAG, "404 Not Found (API): %s", path.c_str());
                    request->send(404, "application/json", "{\"error\":\"Not found\"}");
                }
                else
                {
                    // For all other routes, serve index.html (SPA routing)
                    Logger::debug(TAG, "SPA fallback for: %s", path.c_str());
                    request->send(LittleFS, "/index.html", "text/html");
                }
            });

        server.begin();
        if (WiFiManager::ipv6Running())
            Ipv6Network::listenIpv6(server, PORT);
        Logger::info(TAG, "Web server started");
    }

    void WebServer::retryAlpacaIpv6Discovery(uint32_t now)
    {
        if (!alpacaIpv6Pending || now - lastAlpacaIpv6Attempt < 5000)
            return;
        lastAlpacaIpv6Attempt = now;
        alpacaIpv6Pending = !Ipv6Network::joinAlpacaDiscoveryGroup();
    }

    void WebServer::handle()
    {
        wsSensors.cleanupClients();
        wsStatus.cleanupClients();
        pollWiFiConnect();
        languagePack->loop();

        const uint32_t now = millis();

        applyPendingArm();
        retryAlpacaIpv6Discovery(now);
        if (mqttClient != nullptr && mqttClient->connectionCount() != mqttArmedConnection)
            publishArmedState();
        publishMqttReadings(now);

        if (now - lastSafetyEvaluation >= SAFETY_EVALUATION_INTERVAL_MS)
        {
            updateSafetyStatus();
            lastSafetyEvaluation = now;
        }

        PendingAlertTest pendingTest;
        portENTER_CRITICAL(&pendingAlertTestLock);
        pendingTest = pendingAlertTest;
        pendingAlertTest.mask = 0;
        portEXIT_CRITICAL(&pendingAlertTestLock);
        if (pendingTest.mask != 0)
        {
            const Config &cfg = getConfigCallback();
            const Core::SampleAlert *sample = pendingTest.sample;
            std::string localTime, localDate;
            localClock(localTime, localDate);
            const Alerts::Alert test = Core::buildTestAlert(
                sample,
                pendingTest.level,
                pendingTest.sound,
                pendingTest.title,
                pendingTest.message,
                getSafetyStatus(),
                cfg,
                buildAlpacaObservingConditionsSnapshot(),
                computeNight(getSensorSnapshot(), cfg),
                localTime,
                localDate);
            if (sample != nullptr && test.level == Alerts::AlertLevel::Wake)
            {
                const time_t wallClock = time(nullptr);
                ble.raiseAlarm(sample->bleFlags, wallClock >= Core::CLOCK_VALID_EPOCH ? static_cast<uint32_t>(wallClock) : 0);
            }
            if (pendingTest.mask != BLE_ONLY_TEST)
                alertDispatcher->dispatch(test, cfg.alerts, cfg.deviceName, pendingTest.mask, channelBlocks(getSensorSnapshot()));
        }

        // Broadcast sensor data every 1 second (for Dashboard)
        if (now - lastSensorBroadcast >= WS_SENSOR_BROADCAST_INTERVAL_MS)
        {
            broadcastSensorData();
            lastSensorBroadcast = now;
        }

        // Broadcast status data every 2 seconds (for System page)
        if (now - lastStatusBroadcast >= WS_STATUS_BROADCAST_INTERVAL_MS)
        {
            broadcastStatusData();
            lastStatusBroadcast = now;
        }
    }

    void WebServer::refreshSensorSnapshot(uint32_t dataTimestampMs)
    {
        SensorSnapshot next;
        next.tsl = tslSensor.getReading();
        next.tslDiagnostics = tslSensor.getDiagnostics();
        next.bme = bmeSensor.getReading();
        next.mlx = mlxSensor.getReading();
        next.gps = gpsSensor.getReading();
        next.rg15 = rg15Sensor.copyReading();
        next.rg15Diagnostics = rg15Sensor.getDiagnostics();
        next.tslInitialized = tslSensor.isInitialized();
        next.bmeInitialized = bmeSensor.isInitialized();
        next.mlxInitialized = mlxSensor.isInitialized();
        next.gpsInitialized = gpsSensor.isInitialized();
        next.rg15Initialized = rg15Sensor.isInitialized();
        next.tslLastUpdate = tslSensor.getLastUpdateTime();
        next.bmeLastUpdate = bmeSensor.getLastUpdateTime();
        next.mlxLastUpdate = mlxSensor.getLastUpdateTime();
        next.gpsLastUpdate = gpsSensor.getLastUpdateTime();
        next.rg15LastUpdate = rg15Sensor.getLastUpdateTime();
        next.wind = windSensor.getReading();
        next.dataTimestamp = dataTimestampMs;
        next.capturedAt = millis();

        Core::derive(next, getConfigCallback());

        if (!sensorSnapshotMutex)
        {
            sensorSnapshot = next;
        }
        else if (xSemaphoreTake(sensorSnapshotMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            sensorSnapshot = next;
            xSemaphoreGive(sensorSnapshotMutex);
        }
        else
        {
            Logger::warn(TAG, "Sensor snapshot lock unavailable");
        }
    }

    void WebServer::setupStaticRoutes()
    {
        // Captive portal detection (iOS/macOS, Android, Windows, Firefox):
        // answer every probe with a redirect so the OS opens its sign-in
        // window on the WiFi setup screen.
        for (const char *probe :
             {"/hotspot-detect.html",
              "/library/test/success.html",
              "/generate_204",
              "/gen_204",
              "/success.txt",
              "/connecttest.txt",
              "/ncsi.txt",
              "/redirect",
              "/canonical.html"})
        {
            server.on(
                probe,
                HTTP_GET,
                [](AsyncWebServerRequest *request)
                {
                    Logger::info(TAG, "Captive check %s%s -> setup screen", request->host().c_str(), request->url().c_str());
                    request->redirect(setupScreenUrl().c_str());
                });
        }

        // Serve files from LittleFS
        server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("no-cache, no-store, must-revalidate");
    }

    void WebServer::setupAPIRoutes()
    {
        // Status endpoint
        server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetStatus(request); });

        // Sensors endpoint
        server.on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetSensors(request); });

        // Plain-text "1" (safe) or "0" (unsafe) - the SafetyMonitor verdict
        // for scripts and loggers.
        server.on(
            "/api/safe",
            HTTP_GET,
            [this](AsyncWebServerRequest *request) { request->send(200, "text/plain", getSafetyStatus().isSafe ? "1" : "0"); });

        // Before /api/safety, which would otherwise match this path too.
        server.on(
            "/api/safety/history",
            HTTP_GET,
            [](AsyncWebServerRequest *request)
            {
                static SafetyHistory::Entry entries[SafetyHistory::CAPACITY];
                const size_t n = SafetyHistory::entries(entries, SafetyHistory::CAPACITY);
                static const char *const KIND[] = {"boot", "change", "alert", "armed"};
                DynamicJsonDocument doc(256 + n * 160);
                doc["boot"] = SafetyHistory::currentBoot();
                doc["uptime"] = millis() / 1000;
                JsonArray list = doc.createNestedArray("entries");
                for (size_t i = n; i-- > 0;) // newest first
                {
                    const SafetyHistory::Entry &e = entries[i];
                    JsonObject item = list.createNestedObject();
                    item["kind"] = KIND[static_cast<uint8_t>(e.kind) <= 3 ? static_cast<uint8_t>(e.kind) : 1];
                    item["boot"] = e.boot;
                    item["uptime"] = e.uptimeS;
                    if (e.epoch != 0)
                        item["timestamp"] = e.epoch;
                    if (e.kind == SafetyHistory::Kind::Boot)
                        item["resetReason"] = e.resetReason;
                    else
                        item["safe"] = static_cast<bool>(e.safe);
                    if (e.kind == SafetyHistory::Kind::Change)
                    {
                        item["held"] = static_cast<bool>(e.held);
                        item["reasonFlags"] = e.flags;
                    }
                }
                std::string json;
                serializeJson(doc, json);
                request->send(200, "application/json", json.c_str());
            });

        // What's actually in effect (specs/020-settings-dependencies).
        server.on(
            "/api/settings/effective",
            HTTP_GET,
            [this](AsyncWebServerRequest *request)
            {
                const Deps::Facts facts = settingsFacts(getSensorSnapshot());
                const std::vector<Deps::Entry> entries = Deps::evaluate(getConfigCallback(), facts);
                DynamicJsonDocument doc(Deps::reportCapacity(entries));
                Deps::writeReport(doc.to<JsonObject>(), entries, facts);
                std::string json;
                serializeJson(doc, json);
                request->send(200, "application/json", json.c_str());
            });

        server.on(
            "/api/safety",
            HTTP_GET,
            [this](AsyncWebServerRequest *request)
            {
                DynamicJsonDocument doc(1536);
                appendSafetyStatus(doc.to<JsonObject>());
                std::string json;
                serializeJson(doc, json);
                request->send(200, "application/json", json.c_str());
            });

        server.on(
            "/api/sensors/tsl2591/calibrate-dark",
            HTTP_POST,
            [this](AsyncWebServerRequest *request) { handleTSL2591DarkCalibration(request); });

        // Config endpoints
        server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetConfig(request); });

        AsyncCallbackJsonWebHandler *configHandler = new AsyncCallbackJsonWebHandler(
            "/api/config",
            [this](AsyncWebServerRequest *request, JsonVariant &json)
            {
                if (!requireAuth(request))
                    return;

                String jsonStr;
                serializeJson(json, jsonStr);
                const Config &currentConfig = getConfigCallback();
                std::string reason;
                auto configOpt = Config::fromJson(jsonStr.c_str(), &currentConfig, &reason);

                if (!configOpt)
                {
                    Logger::warn(TAG, "Config rejected: %s", reason.c_str());
                    request->send(
                        400, "application/json", createErrorJson(reason.empty() ? "Invalid configuration" : reason.c_str()).c_str());
                    return;
                }

                const std::string previousLanguage = currentConfig.language;
                if (saveConfigCallback(*configOpt))
                {
                    tslSensor.configureSkyMeasurement(configOpt->skyAveraging, configOpt->skyCalibration);
                    if (previousLanguage != configOpt->language)
                        languagePack->onLanguageChanged(configOpt->language);
                    request->send(200, "application/json", "{\"success\":true}");
                }
                else
                {
                    request->send(500, "application/json", createErrorJson("Failed to save configuration").c_str());
                }
            },
            CONFIG_JSON_BUFFER_SIZE);
        configHandler->setMethod(HTTP_POST | HTTP_PUT);
        server.addHandler(configHandler);

        // System endpoints
        server.on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest *request) { handleRestart(request); });

        // WiFi endpoints
        server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request) { handleWiFiScan(request); });

        server.on("/api/sensors/rg15/test", HTTP_POST, [this](AsyncWebServerRequest *request) { handleRG15Test(request); });
        server.on("/api/sensors/rg15/reset-total", HTTP_POST, [this](AsyncWebServerRequest *request) { handleRG15ResetTotal(request); });
        server.on("/api/sensors/rg15/reboot", HTTP_POST, [this](AsyncWebServerRequest *request) { handleRG15Reboot(request); });

        // MQTT test endpoint
        AsyncCallbackJsonWebHandler *mqttTestHandler = new AsyncCallbackJsonWebHandler(
            "/api/mqtt/test", [this](AsyncWebServerRequest *request, JsonVariant &json) { handleMQTTTest(request, json); });
        server.addHandler(mqttTestHandler);

        // Use AsyncCallbackJsonWebHandler for POST with JSON body
        AsyncCallbackJsonWebHandler *wifiConnectHandler = new AsyncCallbackJsonWebHandler(
            "/api/wifi/connect",
            [this](AsyncWebServerRequest *request, JsonVariant &json)
            {
                if (!requireAuth(request))
                    return;

                JsonObject jsonObj = json.as<JsonObject>();

                if (!jsonObj.containsKey("ssid") || !jsonObj.containsKey("password"))
                {
                    request->send(400, "application/json", "{\"error\":\"Missing SSID or password\"}");
                    return;
                }

                const char *ssid = jsonObj["ssid"];
                const char *password = jsonObj["password"];

                Logger::info(TAG, "Starting nonblocking connection to SSID: '%s'", ssid);

                pendingWifiSSID = ssid;
                pendingWifiPassword = password;
                wifiConnectActive = true;
                wifiConnectConfigSaved = false;
                wifiConnectStartedAt = millis();
                WiFi.disconnect(false);
                WiFi.begin(pendingWifiSSID.c_str(), pendingWifiPassword.c_str());

                StaticJsonDocument<256> doc;
                doc["success"] = true;
                doc["pending"] = true;
                doc["message"] = "Connection started";

                String responseStr;
                serializeJson(doc, responseStr);
                request->send(202, "application/json", responseStr.c_str());
            });
        server.addHandler(wifiConnectHandler);
    }

    void WebServer::setupWebSocket()
    {
        // Sensor WebSocket for Dashboard (/ws/sensors)
        wsSensors.onEvent(
            [this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
            { onSensorWebSocketEvent(server, client, type, arg, data, len); });

        // Status WebSocket for System page (/ws/status)
        wsStatus.onEvent(
            [this](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
            { onStatusWebSocketEvent(server, client, type, arg, data, len); });

        server.addHandler(&wsSensors);
        server.addHandler(&wsStatus);
    }

    void WebServer::setupOTA()
    {
        // Filesystem OTA update (LittleFS partition)
        // Static variables to track filesystem update progress
        static const esp_partition_t *fs_partition = nullptr;
        static size_t fs_bytes_written = 0;
        static bool fs_update_error = false;
        static String fs_error_msg = "";

        server.on(
            "/api/update/fs",
            HTTP_POST,
            [this](AsyncWebServerRequest *request)
            {
                if (!requireAuth(request))
                    return;
                String response_json;

                const bool fsFailed = fs_update_error;
                if (fsFailed)
                {
                    response_json = createErrorJson(fs_error_msg.c_str()).c_str();
                }
                else
                {
                    response_json = "{\"success\":true}";
                }

                // Reset state
                fs_partition = nullptr;
                fs_bytes_written = 0;
                fs_update_error = false;
                fs_error_msg = "";

                AsyncWebServerResponse *response = request->beginResponse(fsFailed ? 500 : 200, "application/json", response_json);
                response->addHeader("Connection", "close");
                request->send(response);

                if (!fs_update_error)
                {
                    WebServer::scheduleRestart(1000);
                }
            },
            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
            {
                if (!index)
                {
                    Logger::info("OTA", "Filesystem update started: %s", filename.c_str());
                    fs_bytes_written = 0;
                    fs_update_error = false;
                    fs_error_msg = "";

                    // Find the LittleFS partition (labeled as "spiffs" in partition table)
                    fs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);

                    if (!fs_partition)
                    {
                        Logger::error("OTA", "Filesystem partition not found!");
                        fs_update_error = true;
                        fs_error_msg = "Filesystem partition not found";
                        return;
                    }

                    Logger::info(
                        "OTA",
                        "Found filesystem partition at 0x%x, size %u bytes",
                        fs_partition->address,
                        static_cast<unsigned>(fs_partition->size));

                    // Unmount LittleFS before writing
                    LittleFS.end();

                    // Erase the partition
                    Logger::info("OTA", "Erasing filesystem partition...");
                    esp_err_t err = esp_partition_erase_range(fs_partition, 0, fs_partition->size);
                    if (err != ESP_OK)
                    {
                        Logger::error("OTA", "Partition erase failed: %d", err);
                        fs_update_error = true;
                        fs_error_msg = "Failed to erase partition";
                        return;
                    }
                    Logger::info("OTA", "Partition erased successfully");
                }

                if (!fs_update_error && fs_partition)
                {
                    // Write data directly to partition (no magic byte validation)
                    esp_err_t err = esp_partition_write(fs_partition, fs_bytes_written, data, len);
                    if (err != ESP_OK)
                    {
                        Logger::error("OTA", "Partition write failed at offset %u: %d", static_cast<unsigned>(fs_bytes_written), err);
                        fs_update_error = true;
                        fs_error_msg = "Failed to write to partition";
                        return;
                    }
                    fs_bytes_written += len;

                    if (index % 10240 == 0)
                    { // Log every ~10KB
                        Logger::info("OTA", "Written %u bytes", static_cast<unsigned>(fs_bytes_written));
                    }
                }

                if (final)
                {
                    if (!fs_update_error)
                    {
                        Logger::info("OTA", "Filesystem update success: %u bytes written", static_cast<unsigned>(fs_bytes_written));
                    }
                    else
                    {
                        Logger::error("OTA", "Filesystem update failed: %s", fs_error_msg.c_str());
                    }
                }
            });

        // Firmware OTA update (app partition). Registered after /api/update/fs:
        // this server also matches "/api/update" as a prefix of
        // "/api/update/fs", so registered first it took filesystem uploads too.
        // Set when the updater's own activation failed but a second
        // esp_ota_set_boot_partition() (which re-verifies the image) succeeded.
        static bool activatedOnRetry = false;
        server.on(
            "/api/update",
            HTTP_POST,
            [this](AsyncWebServerRequest *request)
            {
                if (!requireAuth(request))
                    return;
                bool success = !Update.hasError() || activatedOnRetry;
                String response_json;

                if (success)
                {
                    // `retried`: the boot switch only took on the second try
                    // (see the upload handler), so the success rate of the
                    // first attempt can be measured (spec 012 SC-001).
                    response_json = activatedOnRetry ? "{\"success\":true,\"retried\":true}" : "{\"success\":true}";
                }
                else
                {
                    // Get detailed error message
                    String error_msg = "Unknown error";
                    uint8_t error = Update.getError();
                    switch (error)
                    {
                    case UPDATE_ERROR_OK:
                        error_msg = "No error";
                        break;
                    case UPDATE_ERROR_WRITE:
                        error_msg = "Flash write failed";
                        break;
                    case UPDATE_ERROR_ERASE:
                        error_msg = "Flash erase failed";
                        break;
                    case UPDATE_ERROR_READ:
                        error_msg = "Flash read failed";
                        break;
                    case UPDATE_ERROR_SPACE:
                        error_msg = "Not enough space";
                        break;
                    case UPDATE_ERROR_SIZE:
                        error_msg = "Bad size given";
                        break;
                    case UPDATE_ERROR_STREAM:
                        error_msg = "Stream read timeout";
                        break;
                    case UPDATE_ERROR_MD5:
                        error_msg = "MD5 check failed";
                        break;
                    case UPDATE_ERROR_MAGIC_BYTE:
                        error_msg = "Wrong magic byte";
                        break;
                    case UPDATE_ERROR_ACTIVATE:
                        error_msg = "Could not activate partition";
                        break;
                    case UPDATE_ERROR_NO_PARTITION:
                        error_msg = "Partition not found";
                        break;
                    case UPDATE_ERROR_BAD_ARGUMENT:
                        error_msg = "Bad argument";
                        break;
                    case UPDATE_ERROR_ABORT:
                        error_msg = "Update aborted";
                        break;
                    default:
                        error_msg = "Error code: " + String(error);
                        break;
                    }
                    response_json = createErrorJson(error_msg.c_str()).c_str();
                }

                AsyncWebServerResponse *response = request->beginResponse(success ? 200 : 500, "application/json", response_json);
                response->addHeader("Connection", "close");
                request->send(response);

                if (success)
                {
                    WebServer::scheduleRestart(1000);
                }
            },
            [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
            {
                if (!index)
                {
                    Logger::info("OTA", "Firmware update started: %s", filename.c_str());
                    activatedOnRetry = false;
                    if (Update.isRunning())
                    {
                        // An earlier upload was cut off; start clean.
                        Logger::warn("OTA", "Aborting an unfinished update");
                        Update.abort();
                    }
                    if (!Update.begin(UPDATE_SIZE_UNKNOWN))
                    {
                        Logger::error("OTA", "Update.begin failed: %d", Update.getError());
                        Update.printError(Serial);
                    }
                }

                if (Update.write(data, len) != len)
                {
                    Logger::error("OTA", "Update.write failed: %d", Update.getError());
                    Update.printError(Serial);
                }

                if (final)
                {
                    if (Update.end(true))
                    {
                        Logger::info("OTA", "Firmware update success, rebooting...");
                    }
                    else if (Update.getError() == UPDATE_ERROR_ACTIVATE)
                    {
                        // The image is fully written and its first block restored;
                        // only switching the boot partition failed. Uploads used to
                        // fail like this on the first attempt and succeed on a
                        // retry, so retry the switch here. It verifies the image
                        // again, so a bad image still can't be booted.
                        const esp_partition_t *target = esp_ota_get_next_update_partition(nullptr);
                        const esp_err_t first = esp_ota_set_boot_partition(target);
                        Logger::warn("OTA", "Activating %s failed; retry: %s", target ? target->label : "?", esp_err_to_name(first));
                        activatedOnRetry = first == ESP_OK;
                        if (activatedOnRetry)
                            Logger::info("OTA", "Firmware update success on retry, rebooting...");
                    }
                    else
                    {
                        Logger::error("OTA", "Update.end failed: %d", Update.getError());
                        Update.printError(Serial);
                    }
                }
            });
    }

    void WebServer::setupGithubUpdates()
    {
        // Check for available releases on the given track (?track=stable|beta,
        // defaults to stable). Returns the filtered release list; staleness
        // relative to the running firmware is computed client-side.
        server.on(
            "/api/updates/check",
            HTTP_GET,
            [this](AsyncWebServerRequest *request)
            {
                if (!requireAuth(request))
                    return;

                std::string track = "stable";
                if (request->hasParam("track"))
                {
                    String t = request->getParam("track")->value();
                    if (t == "beta")
                        track = "beta";
                }

                std::string error;
                std::vector<GithubRelease> releases = otaUpdater->checkForUpdate(track, error);

                if (!error.empty())
                {
                    AsyncWebServerResponse *response =
                        request->beginResponse(502, "application/json", createErrorJson(error.c_str()).c_str());
                    request->send(response);
                    return;
                }

                DynamicJsonDocument doc(8192);
                JsonArray arr = doc.to<JsonArray>();
                for (const GithubRelease &r : releases)
                {
                    JsonObject o = arr.createNestedObject();
                    o["tag"] = r.tag;
                    o["name"] = r.name;
                    o["prerelease"] = r.prerelease;
                    o["publishedAt"] = r.publishedAt;
                    o["firmwareAssetUrl"] = r.firmwareAssetUrl;
                    o["firmwareAssetSize"] = r.firmwareAssetSize;
                    o["fsAssetUrl"] = r.fsAssetUrl;
                    o["fsAssetSize"] = r.fsAssetSize;
                }

                std::string json;
                serializeJson(doc, json);
                request->send(200, "application/json", json.c_str());
            });

        // Starts a self-download+flash of the given release's firmware AND
        // filesystem assets as one atomic update (never just one, to avoid
        // frontend/backend drift). Body: {"firmwareAssetUrl", "firmwareAssetSize",
        // "fsAssetUrl", "fsAssetSize"}. Progress/errors are pushed over
        // /ws/status ("ota_progress" messages), same channel the manual
        // upload OTA flow already uses.
        AsyncCallbackJsonWebHandler *applyHandler = new AsyncCallbackJsonWebHandler(
            "/api/updates/apply",
            [this](AsyncWebServerRequest *request, JsonVariant &json)
            {
                if (!requireAuth(request))
                    return;

                JsonObject body = json.as<JsonObject>();

                GithubRelease release;
                release.firmwareAssetUrl = std::string(body["firmwareAssetUrl"] | "");
                release.firmwareAssetSize = body["firmwareAssetSize"] | 0;
                release.fsAssetUrl = std::string(body["fsAssetUrl"] | "");
                release.fsAssetSize = body["fsAssetSize"] | 0;

                if (release.firmwareAssetUrl.empty() || release.fsAssetUrl.empty())
                {
                    request->send(400, "application/json", createErrorJson("firmwareAssetUrl and fsAssetUrl are required").c_str());
                    return;
                }

                if (!otaUpdater->applyUpdate(release))
                {
                    request->send(409, "application/json", createErrorJson("Update already in progress").c_str());
                    return;
                }

                request->send(200, "application/json", "{\"success\":true,\"message\":\"Update started\"}");
            });
        applyHandler->setMethod(HTTP_POST);
        server.addHandler(applyHandler);
    }

    Alpaca::SafetyInputs WebServer::buildAlpacaSafetyInputs() const
    {
        return Core::safetyInputs(getSensorSnapshot(), getConfigCallback(), millis());
    }

    Alpaca::SafetyThresholds WebServer::buildAlpacaSafetyThresholds(const Config &cfg)
    {
        return Core::safetyThresholds(cfg);
    }

    Alpaca::SafetyResult WebServer::evaluateAlpacaSafety() const
    {
        return Alpaca::evaluateSafety(buildAlpacaSafetyInputs(), buildAlpacaSafetyThresholds(getConfigCallback()));
    }

    void WebServer::updateSafetyStatus()
    {
        const Alpaca::SafetyResult result = evaluateAlpacaSafety();
        const uint32_t now = millis();

        if (safetyMutex && xSemaphoreTake(safetyMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            const bool first = safetyStatus.evaluatedAtMs == 0;
            if (Core::updateSafety(safetyStatus, safeDelayFilter, result, getConfigCallback(), now))
            {
                if (!first)
                    Logger::info(TAG, "SafetyMonitor now %s", safetyStatus.isSafe ? "SAFE" : "UNSAFE");
                SafetyHistory::recordChange(safetyStatus.isSafe, !safetyStatus.isSafe && result.isSafe, result.reasonFlags);
            }
            xSemaphoreGive(safetyMutex);
        }

        processAlerts(getSafetyStatus());
    }

    Core::NightState WebServer::computeNight(const SensorSnapshot &snapshot, const Config &cfg)
    {
        return Core::night(snapshot, cfg, static_cast<int64_t>(time(nullptr)));
    }

    void WebServer::localClock(std::string &timeText, std::string &dateText)
    {
        char clock[8] = "--:--";
        char date[12] = "--";
        const time_t now = time(nullptr);
        if (now >= Core::CLOCK_VALID_EPOCH)
        {
            struct tm local;
            localtime_r(&now, &local);
            strftime(clock, sizeof(clock), "%H:%M", &local);
            strftime(date, sizeof(date), "%Y-%m-%d", &local);
        }
        timeText = clock;
        dateText = date;
    }

    void WebServer::processAlerts(const SafetyStatus &status)
    {
        const Config &cfg = getConfigCallback();
        if (mqttClient != nullptr && mqttClient->isConnected() && cfg.mqtt.publish.safety && status.evaluatedAtMs != 0)
            publishMqttSafety(status);

        const SensorSnapshot snapshot = getSensorSnapshot();
        const Alpaca::ObservingConditionsSnapshot obs = buildAlpacaObservingConditionsSnapshot();

        if (!alertEngineSeeded)
        {
            alertEngineSeeded = true;
            bool toldSafe = false;
            if (SafetyHistory::lastAlert(toldSafe))
                alertEngine.seedSafety(!toldSafe);
        }
        const Core::NightState night = computeNight(snapshot, cfg);
        Alerts::AlertInputs in = Core::alertInputs(status, snapshot, obs, cfg, night, millis());
        std::string localTime, localDate;
        localClock(localTime, localDate);

        // The imaging app: is each Alpaca device still being checked?
        {
            const Alpaca::DeviceActivity activity[Alpaca::DEVICE_COUNT] = {
                alpacaRouter.activity(Alpaca::Device::SafetyMonitor), alpacaRouter.activity(Alpaca::Device::ObservingConditions)};
            uint32_t silenceMs[Alpaca::DEVICE_COUNT];
            Core::clientSilenceMs(cfg, silenceMs);
            clientWatch.update(activity, silenceMs, cfg.alpaca.enabled, millis());
            Core::addClientInputs(in, clientWatch, cfg, millis(), localTime);
        }
        const Alerts::AlertRules rules = Core::alertRules(cfg);

        if (ble.isActive())
        {
            Ble::State bleState;
            bleState.safetyKnown = in.safetyKnown;
            bleState.isSafe = status.isSafe;
            bleState.rawSafe = status.rawSafe;
            bleState.reasonFlags = status.reasonFlags;
            bleState.rainEnabled = cfg.rain.enabled;
            bleState.rainHealthy = obs.rain.valid;
            bleState.raining = in.raining;
            bleState.rainRateMmPerHour = obs.rainRateMmPerHour;
            bleState.sqmValid = obs.skyLight.valid;
            bleState.sqm = obs.skyQualityMagArcsec2;

            StaticJsonDocument<256> summary;
            auto put = [&summary](const char *key, bool valid, float value)
            {
                if (valid)
                    summary[key] = serialized(String(value, 2));
            };
            put("sqm", obs.skyLight.valid, obs.skyQualityMagArcsec2);
            put("cloud", obs.irSky.valid, obs.cloudCoverPercent);
            put("skyT", obs.irSky.valid, obs.skyTemperatureC);
            put("temp", obs.environment.valid, obs.temperatureC);
            put("hum", obs.environment.valid, obs.humidityPercent);
            put("dew", obs.environment.valid, obs.dewpointC);
            put("press", obs.environment.valid, obs.pressureHPa);
            put("wind", obs.wind.valid, obs.windSpeedMs);
            put("gust", obs.wind.valid, obs.windGustMs);
            std::string summaryJson;
            serializeJson(summary, summaryJson);
            ble.update(bleState, summaryJson);
        }

        // Always run the engine so its state tracks reality while alerts are
        // off. Each alert gets its configured level, sound and wording;
        // everything raised in the same pass goes out as one notification.
        // Push channels need the master switch, paired phones only Bluetooth.
        const time_t wallClock = time(nullptr);
        const uint32_t epoch = wallClock >= 1704067200 ? static_cast<uint32_t>(wallClock) : 0;
        Core::AlertStep step = Core::runAlerts(alertEngine, in, rules, cfg, obs, night, status, localTime, localDate);
        std::vector<Alerts::Alert> &outgoing = step.outgoing;
        const uint32_t alarmFlags = step.alarmFlags;
        // Paused: state is still tracked above, nothing goes out.
        if (!outgoing.empty() && alertSchedule.state().sending)
        {
            const Alerts::Alert notification = Alerts::stackAlerts(outgoing);
            if (cfg.alerts.enabled)
            {
                alertDispatcher->dispatch(notification, cfg.alerts, cfg.deviceName, ALERT_CHANNELS_ALL, channelBlocks(snapshot));
                ble.publishAlert(notification);
                for (const Alerts::Alert &sent : outgoing)
                    if (sent.type == Alerts::AlertType::Unsafe || sent.type == Alerts::AlertType::Safe)
                        SafetyHistory::recordAlert(sent.type == Alerts::AlertType::Safe);
            }
            if (notification.level == Alerts::AlertLevel::Wake)
                ble.raiseAlarm(alarmFlags, epoch);
            else
                ble.raiseInfo(status.reasonFlags, epoch);
        }

        // Only after this pass's alerts went out: a clean disconnect's
        // "imaging app disconnected" is sent before it pauses alerts.
        {
            const bool wasSending = alertSchedule.state().sending;
            if (alertSchedule.update(Core::effectiveSendMode(cfg), alpacaRouter.anyConnected(), millis(), epoch))
                scheduleChanged(wasSending);
        }
        portENTER_CRITICAL(&scheduleLock);
        clientWatchShared = clientWatch;
        portEXIT_CRITICAL(&scheduleLock);

        uint32_t acknowledged = 0;
        bool fromPhone = false;
        if (ble.processAcks(acknowledged, fromPhone) && cfg.alerts.enabled)
        {
            Alerts::Alert ack;
            ack.type = Alerts::AlertType::Acknowledged;
            ack.level = Alerts::AlertLevel::Quiet;
            ack.title = "Alarm acknowledged";
            ack.message = std::string("Phone alarm #") + std::to_string(acknowledged) + " was acknowledged " +
                          (fromPhone ? "on a phone." : "in the web UI.");
            alertDispatcher->dispatch(ack, cfg.alerts, cfg.deviceName, ALERT_CHANNELS_ALL, channelBlocks(snapshot));
        }
    }

    void WebServer::applyPendingArm()
    {
        const int8_t requested = pendingArm.exchange(-1);
        if (requested < 0)
            return;
        const bool wasSending = alertSchedule.state().sending;
        const time_t wallClock = time(nullptr);
        const int64_t epoch = wallClock >= Core::CLOCK_VALID_EPOCH ? static_cast<int64_t>(wallClock) : 0;
        if (alertSchedule.command((requested & 1) != 0, static_cast<Alerts::ScheduleReason>(requested / 2), millis(), epoch))
            scheduleChanged(wasSending);
    }

    void WebServer::shareSchedule()
    {
        portENTER_CRITICAL(&scheduleLock);
        scheduleShared = alertSchedule.state();
        portEXIT_CRITICAL(&scheduleLock);
    }

    Alerts::ScheduleState WebServer::sharedSchedule() const
    {
        portENTER_CRITICAL(&scheduleLock);
        Alerts::ScheduleState state = scheduleShared;
        portEXIT_CRITICAL(&scheduleLock);
        // A pause/resume the loop hasn't applied yet already counts.
        const int8_t pending = pendingArm.load();
        if (pending >= 0)
        {
            state.sending = (pending & 1) != 0;
            state.reason = static_cast<Alerts::ScheduleReason>(pending / 2);
        }
        return state;
    }

    Alpaca::ClientWatch WebServer::sharedClientWatch() const
    {
        portENTER_CRITICAL(&scheduleLock);
        const Alpaca::ClientWatch copy = clientWatchShared;
        portEXIT_CRITICAL(&scheduleLock);
        return copy;
    }

    void WebServer::scheduleChanged(bool wasSending)
    {
        const Alerts::ScheduleState &state = alertSchedule.state();
        shareSchedule();
        Preferences prefs;
        if (prefs.begin(ARMED_NVS_NAMESPACE, false))
        {
            prefs.putBool("armed", state.sending);
            prefs.putUChar("reason", static_cast<uint8_t>(state.reason));
            prefs.putUInt("since", static_cast<uint32_t>(state.sinceEpoch > 0 ? state.sinceEpoch : 0));
            prefs.end();
        }
        if (state.sending == wasSending)
            return;
        Logger::info(TAG, "Alerts %s (%s)", state.sending ? "resumed" : "paused", Alerts::scheduleReasonName(state.reason));
        SafetyHistory::recordArmed(state.sending);
        publishArmedState();

        const Config &cfg = getConfigCallback();
        if (state.sending && cfg.alerts.enabled)
        {
            // One quiet line so you know where things stand as you start.
            const SafetyStatus safety = getSafetyStatus();
            Alerts::Alert on;
            on.type = Alerts::AlertType::AlertsOn;
            on.level = Alerts::AlertLevel::Quiet;
            on.title = "Alerts resumed";
            if (safety.evaluatedAtMs == 0)
                on.message = "Safety not evaluated yet.";
            else if (safety.isSafe)
                on.message = "Observatory safe.";
            else
                on.message =
                    "Observatory UNSAFE" + (safety.reasons.empty() ? std::string(".") : ":\n" + Alerts::joinReasons(safety.reasons));
            alertDispatcher->dispatch(on, cfg.alerts, cfg.deviceName, ALERT_CHANNELS_ALL, channelBlocks(getSensorSnapshot()));
        }
    }

    void WebServer::publishArmedState()
    {
        if (mqttClient == nullptr || !mqttClient->isConnected())
            return;
        if (mqttClient->publishSubtopic("alerts/armed", alertSchedule.state().sending ? "1" : "0", true))
            mqttArmedConnection = mqttClient->connectionCount();
    }

    void WebServer::publishMqttSafety(const SafetyStatus &status)
    {
        const uint32_t now = millis();
        const bool changed =
            !mqttSafetyPublished || status.isSafe != mqttLastPublishedSafe || mqttClient->connectionCount() != mqttSafetyConnection;
        if (!changed && now - mqttSafetyPublishedAt < MQTT_SAFETY_REPUBLISH_MS)
            return;

        // Same object as GET /api/safety; <base>/safe is the bare 1/0 for
        // loggers, simple automations and the Home Assistant binary sensor.
        DynamicJsonDocument doc(1024);
        appendSafetyStatus(doc.to<JsonObject>());
        std::string payload;
        serializeJson(doc, payload);
        if (mqttClient->publishSubtopic("safety", payload, true) && mqttClient->publishSubtopic("safe", status.isSafe ? "1" : "0", true))
        {
            mqttSafetyPublished = true;
            mqttLastPublishedSafe = status.isSafe;
            mqttSafetyPublishedAt = now;
            mqttSafetyConnection = mqttClient->connectionCount();
        }
    }

    void WebServer::setupAlertRoutes()
    {
        server.on(
            "/api/alerts/test",
            HTTP_POST,
            [this](AsyncWebServerRequest *request)
            {
                if (!requireAuth(request))
                    return;

                const String channel = request->hasParam("channel") ? request->getParam("channel")->value() : String("all");
                const AlertsConfig &alerts = getConfigCallback().alerts;
                uint8_t mask = 0;
                if (channel == "all")
                    mask = ALERT_CHANNELS_ALL;
                else if (channel == "mqtt")
                    mask = alertChannelBit(AlertChannel::Mqtt);
                else if (channel == "pushover")
                    mask = alertChannelBit(AlertChannel::Pushover);
                else if (channel == "ntfy")
                    mask = alertChannelBit(AlertChannel::Ntfy);
                else if (channel == "webhook")
                    mask = alertChannelBit(AlertChannel::Webhook);
                else
                {
                    request->send(
                        400,
                        "application/json",
                        createErrorJson("Unknown channel (expected mqtt, pushover, ntfy, webhook or all)").c_str());
                    return;
                }

                const uint8_t enabledMask = (alerts.mqttEnabled ? alertChannelBit(AlertChannel::Mqtt) : 0) |
                                            (alerts.pushoverEnabled ? alertChannelBit(AlertChannel::Pushover) : 0) |
                                            (alerts.ntfyEnabled ? alertChannelBit(AlertChannel::Ntfy) : 0) |
                                            (alerts.webhookEnabled ? alertChannelBit(AlertChannel::Webhook) : 0);
                // ?event=rain_started&level=4&sound=siren sends a sample of that
                // event at the given level, so unsaved choices can be tried out.
                const Core::SampleAlert *eventSample = nullptr;
                uint8_t level = 2;
                String sound;
                String title;
                String message;
                if (request->hasParam("event"))
                {
                    eventSample = Core::sampleAlert(request->getParam("event")->value().c_str());
                    if (eventSample == nullptr)
                    {
                        request->send(400, "application/json", createErrorJson("Unknown event").c_str());
                        return;
                    }
                    level = request->hasParam("level") ? static_cast<uint8_t>(request->getParam("level")->value().toInt()) : 2;
                    if (level < 1 || level > 4)
                    {
                        request->send(400, "application/json", createErrorJson("Level must be 1-4").c_str());
                        return;
                    }
                    sound = request->hasParam("sound") ? request->getParam("sound")->value() : String();
                    if (sound.length() > 32)
                    {
                        request->send(400, "application/json", createErrorJson("Sound name too long").c_str());
                        return;
                    }
                    title = request->hasParam("title") ? request->getParam("title")->value() : String();
                    message = request->hasParam("message") ? request->getParam("message")->value() : String();
                    if (title.length() > AlertsConfig::MAX_TEMPLATE_TITLE || message.length() > AlertsConfig::MAX_TEMPLATE_MESSAGE)
                    {
                        request->send(
                            400, "application/json", createErrorJson("Title is up to 80 characters and message up to 240").c_str());
                        return;
                    }
                }

                uint8_t sendMask = mask & enabledMask;
                const bool ringsPhone = eventSample != nullptr && level == 4 && ble.alarmStatus().serviceActive;
                if (sendMask == 0 && ringsPhone)
                    sendMask = BLE_ONLY_TEST;
                if (sendMask == 0)
                {
                    request->send(
                        400, "application/json", createErrorJson("That channel isn't enabled - enable it and save settings first").c_str());
                    return;
                }

                portENTER_CRITICAL(&pendingAlertTestLock);
                pendingAlertTest.mask |= sendMask;
                pendingAlertTest.sample = eventSample;
                pendingAlertTest.level = level;
                strlcpy(pendingAlertTest.sound, sound.c_str(), sizeof(pendingAlertTest.sound));
                strlcpy(pendingAlertTest.title, title.c_str(), sizeof(pendingAlertTest.title));
                strlcpy(pendingAlertTest.message, message.c_str(), sizeof(pendingAlertTest.message));
                portEXIT_CRITICAL(&pendingAlertTestLock);
                request->send(202, "application/json", "{\"success\":true,\"message\":\"Test notification queued\"}");
            });

        server.on(
            "/api/alerts/clear",
            HTTP_POST,
            [this](AsyncWebServerRequest *request)
            {
                if (!requireAuth(request))
                    return;
                alertDispatcher->clearRecent();
                request->send(200, "application/json", "{\"success\":true}");
            });

        server.on(
            "/api/ble/ack",
            HTTP_POST,
            [this](AsyncWebServerRequest *request)
            {
                if (!requireAuth(request))
                    return;
                if (!ble.alarmStatus().alarmActive)
                {
                    request->send(409, "application/json", createErrorJson("No phone alarm is active").c_str());
                    return;
                }
                ble.requestLocalAck();
                request->send(202, "application/json", "{\"success\":true}");
            });

        server.on(
            "/api/ble/forget-bonds",
            HTTP_POST,
            [this](AsyncWebServerRequest *request)
            {
                if (!requireAuth(request))
                    return;
                if (!ble.isActive())
                {
                    request->send(409, "application/json", createErrorJson("Bluetooth is off").c_str());
                    return;
                }
                ble.requestForgetBonds();
                request->send(202, "application/json", "{\"success\":true}");
            });

        // Resume ("arm") and pause ("disarm") for automations (Home Assistant
        // rest_command, N.I.N.A. sequence scripts) and the web UI, which adds
        // ?source=ui so the status line can say who paused.
        auto armRoute = [this](bool armed)
        {
            return [this, armed](AsyncWebServerRequest *request)
            {
                if (!requireAuth(request))
                    return;
                const AsyncWebParameter *source = request->getParam("source");
                const bool fromUi = source != nullptr && source->value() == "ui";
                pendingArm = encodeArm(armed, fromUi ? Alerts::ScheduleReason::UserUi : Alerts::ScheduleReason::UserRest);
                request->send(202, "application/json", armed ? "{\"success\":true,\"armed\":true}" : "{\"success\":true,\"armed\":false}");
            };
        };
        server.on("/api/alerts/arm", HTTP_POST, armRoute(true));
        server.on("/api/alerts/disarm", HTTP_POST, armRoute(false));
        server.on(
            "/api/alerts/armed",
            HTTP_GET,
            [this](AsyncWebServerRequest *request)
            {
                StaticJsonDocument<384> doc;
                Core::writeAlertSchedule(doc.to<JsonObject>(), sharedSchedule(), getConfigCallback(), millis());
                std::string json;
                serializeJson(doc, json);
                request->send(200, "application/json", json.c_str());
            });

        server.on(
            "/api/alerts/recent",
            HTTP_GET,
            [this](AsyncWebServerRequest *request)
            {
                const std::vector<AlertRecord> records = alertDispatcher->recent();
                DynamicJsonDocument doc(8192);
                doc["enabled"] = getConfigCallback().alerts.enabled;
                doc["armed"] = sharedSchedule().sending;
                JsonArray arr = doc.createNestedArray("alerts");
                const uint32_t nowSeconds = millis() / 1000;
                // Newest first
                for (auto it = records.rbegin(); it != records.rend(); ++it)
                {
                    JsonObject item = arr.createNestedObject();
                    item["id"] = it->id;
                    item["event"] = Alerts::alertTypeName(it->alert.type);
                    item["title"] = it->alert.title;
                    item["message"] = it->alert.message;
                    item["level"] = Alerts::alertLevelName(it->alert.level);
                    item["ageSeconds"] = nowSeconds - it->uptimeSeconds;
                    if (it->epochSeconds != 0)
                        item["timestamp"] = it->epochSeconds;
                    JsonObject channels = item.createNestedObject("channels");
                    for (size_t i = 0; i < ALERT_CHANNEL_COUNT; ++i)
                    {
                        if (it->status[i] == DeliveryStatus::NotSent)
                            continue;
                        JsonObject ch = channels.createNestedObject(alertChannelName(static_cast<AlertChannel>(i)));
                        ch["status"] = deliveryStatusName(it->status[i]);
                        ch["detail"] = it->detail[i];
                    }
                }
                std::string json;
                serializeJson(doc, json);
                request->send(200, "application/json", json.c_str());
            });
    }

    SafetyStatus WebServer::getSafetyStatus() const
    {
        SafetyStatus copy;
        if (safetyMutex && xSemaphoreTake(safetyMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            copy = safetyStatus;
            xSemaphoreGive(safetyMutex);
        }
        return copy;
    }

    Alpaca::ObservingConditionsSnapshot WebServer::buildAlpacaObservingConditionsSnapshot() const
    {
        return Core::observingConditions(getSensorSnapshot(), getConfigCallback(), millis());
    }

    void WebServer::setupAlpacaRoutes()
    {
        // --- Setup pages ---
        // NINA's (and other clients') "Setup" button opens
        // /setup/v1/<devicetype>/<n>/setup in a browser. All of the device's
        // settings live on the SPA's Settings page, so send every setup URL
        // there. server.on() also matches "/setup/..." as a prefix in this
        // ESPAsyncWebServer version, so this single route covers them all.
        server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/settings?section=alpaca"); });

        // --- Management + device API ---
        // Everything is handled by Alpaca::Router (lib/AlpacaLogic), which the
        // native simulator ConformU tests in CI also runs. One handler per
        // prefix: registering ~50 routes separately cost ~10 KB of heap.
        server.on("/management", HTTP_GET, [this](AsyncWebServerRequest *request) { handleAlpacaRequest(request); });
        server.on("/api/v1", HTTP_ANY, [this](AsyncWebServerRequest *request) { handleAlpacaRequest(request); });
    }

    Alpaca::ServerIdentity WebServer::alpacaIdentity()
    {
        return {FIRMWARE_NAME, "SQMeter", FIRMWARE_VERSION, ESP.getEfuseMac()};
    }

    bool WebServer::AlpacaBackend::alpacaEnabled() const
    {
        return owner.getConfigCallback().alpaca.enabled;
    }
    bool WebServer::AlpacaBackend::isSafe() const
    {
        return owner.getSafetyStatus().isSafe;
    }
    Alpaca::ObservingConditionsSnapshot WebServer::AlpacaBackend::observingConditions() const
    {
        return owner.buildAlpacaObservingConditionsSnapshot();
    }
    std::string WebServer::AlpacaBackend::location() const
    {
        return owner.getConfigCallback().deviceName;
    }

    // ISO 8601 UTC for DeviceState, or empty if the clock has never been set
    // (NTP/GPS) - a 1970 timestamp would be worse than none.
    std::string WebServer::AlpacaBackend::timestampUtc() const
    {
        return Core::isoUtc(static_cast<int64_t>(time(nullptr)));
    }

    void WebServer::handleAlpacaRequest(AsyncWebServerRequest *request)
    {
        Alpaca::Request alpaca;
        alpaca.get = request->method() == HTTP_GET;
        alpaca.put = request->method() == HTTP_PUT;
        alpaca.path = request->url().c_str();
        const size_t count = request->params();
        alpaca.params.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            const AsyncWebParameter *param = request->getParam(i);
            if (param != nullptr && !param->isFile())
                alpaca.params.emplace_back(param->name().c_str(), param->value().c_str());
        }
        Alpaca::Response response;
        if (!alpacaRouter.handle(alpaca, response))
        {
            response.status = 400;
            response.contentType = "text/plain";
            response.body = "Invalid Alpaca device type, device number, method or HTTP verb";
        }
        request->send(response.status, response.contentType, response.body.c_str());
    }

    void WebServer::handleGetStatus(AsyncWebServerRequest *request)
    {
        std::string json = createStatusJson();
        request->send(200, "application/json", json.c_str());
    }

    void WebServer::handleGetSensors(AsyncWebServerRequest *request)
    {
        std::string json = createSensorDataJson();
        request->send(200, "application/json", json.c_str());
    }

    void WebServer::handleGetConfig(AsyncWebServerRequest *request)
    {
        const Config &cfg = getConfigCallback();
        std::string json = cfg.toJson(true);
        request->send(200, "application/json", json.c_str());
    }

    void WebServer::handleRestart(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Restarting...\"}");
        scheduleRestart(500);
    }

    void WebServer::handleWiFiScan(AsyncWebServerRequest *request)
    {
        // Starting a radio scan is an action, like joining a network
        // (/api/wifi/connect): both need the password when protection is on.
        if (!requireAuth(request))
            return;

        int n = WiFi.scanComplete();

        if (n == WIFI_SCAN_RUNNING)
        {
            request->send(202, "application/json", "{\"success\":true,\"scanning\":true,\"networks\":[]}");
            return;
        }

        if (n < 0)
        {
            WiFi.scanDelete();
            if (WiFi.scanNetworks(true) == WIFI_SCAN_RUNNING)
            {
                request->send(202, "application/json", "{\"success\":true,\"scanning\":true,\"networks\":[]}");
            }
            else
            {
                request->send(500, "application/json", createErrorJson("Failed to start WiFi scan").c_str());
            }
            return;
        }

        StaticJsonDocument<2048> doc;
        doc["success"] = true;
        doc["scanning"] = false;
        JsonArray networks = doc.createNestedArray("networks");

        for (int i = 0; i < n; i++)
        {
            JsonObject net = networks.createNestedObject();
            net["ssid"] = WiFi.SSID(i);
            net["rssi"] = WiFi.RSSI(i);
            net["encryption"] = (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? "open" : "secured";
        }

        std::string json;
        serializeJson(doc, json);
        request->send(200, "application/json", json.c_str());
        WiFi.scanDelete();
    }

    void WebServer::handleRG15Test(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;

        if (!getConfigCallback().rain.enabled)
        {
            request->send(
                409, "application/json", createErrorJson("The rain sensor is switched off (Settings → Sensors → Rain sensor)").c_str());
            return;
        }

        const uint32_t startedAt = millis();
        const bool ok = rg15Sensor.testCommunication();
        const RG15Reading reading = rg15Sensor.copyReading();
        const RG15Diagnostics diagnostics = rg15Sensor.getDiagnostics();
        const uint32_t now = millis();
        const bool success = ok && reading.status == SensorStatus::OK;

        // Success: 200 {"success": true, ...}; failure: 502 {"error": ..., ...}.
        // Both carry what was sent and received, for bring-up.
        StaticJsonDocument<1024> response;
        if (success)
            response["success"] = true;
        else
            response["error"] = diagnostics.lastError ? diagnostics.lastError->c_str() : "No valid response from the RG-15";
        response["command"] = diagnostics.lastCommand ? diagnostics.lastCommand->c_str() : "R";
        response["bytesWritten"] = diagnostics.lastBytesWritten;
        response["elapsedMs"] = now - startedAt;
        if (diagnostics.lastRawResponse)
            response["rawResponse"] = diagnostics.lastRawResponse->c_str();
        if (diagnostics.lastAck)
            response["ack"] = diagnostics.lastAck->c_str();
        response["online"] = reading.online;
        if (diagnostics.lastSuccessfulReadMs != 0)
            response["lastSuccessfulReadAgeMs"] = now - diagnostics.lastSuccessfulReadMs;
        if (!success)
            response["hint"] = "Check RG-15 Serial OUT -> ESP32 RX, Serial IN -> ESP32 TX, common ground, baud rate, and voltage level.";

        String responseStr;
        serializeJson(response, responseStr);
        request->send(success ? 200 : 502, "application/json", responseStr.c_str());
    }

    void WebServer::handleTSL2591DarkCalibration(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;

        const TSL2591Diagnostics diagnostics = tslSensor.getDiagnostics();
        if (!tslSensor.isInitialized() || diagnostics.sampleCount == 0)
        {
            request->send(409, "application/json", createErrorJson("No light-sensor readings to calibrate from").c_str());
            return;
        }
        // A covered sensor reads near zero at maximum gain. Out of night mode
        // it's seeing light, and the offset would wipe out real readings.
        if (!diagnostics.nightMode)
        {
            request->send(
                409,
                "application/json",
                createErrorJson("The sensor is seeing light. Cover it completely and wait for the averaging window to fill.").c_str());
            return;
        }
        // The offset is the window's average: wait until the window holds only
        // covered readings, not a mix from before it was covered.
        const uint16_t needed = Core::windowSamples(diagnostics);
        if (diagnostics.sampleCount < needed)
        {
            DynamicJsonDocument error(256);
            error["error"] = "The averaging window isn't full yet (" + std::to_string(diagnostics.sampleCount) + " of " +
                             std::to_string(needed) + " samples). Keep the sensor covered and try again.";
            error["sampleCount"] = diagnostics.sampleCount;
            error["windowSamples"] = needed;
            std::string body;
            serializeJson(error, body);
            request->send(409, "application/json", body.c_str());
            return;
        }

        Config updated = getConfigCallback();
        updated.skyCalibration.darkVisibleOffset = diagnostics.rollingVisible;
        updated.skyCalibration.darkFullOffset = 0.0F;
        updated.skyCalibration.darkIrOffset = 0.0F;
        updated.skyCalibration.darkSampleCount = diagnostics.sampleCount;
        const time_t epochSeconds = time(nullptr);
        updated.skyCalibration.darkCalibratedAt =
            epochSeconds >= 1704067200 ? static_cast<int64_t>(epochSeconds) : static_cast<int64_t>(millis());

        if (!saveConfigCallback(updated))
        {
            request->send(500, "application/json", createErrorJson("Failed to save dark calibration").c_str());
            return;
        }

        tslSensor.configureSkyMeasurement(updated.skyAveraging, updated.skyCalibration);

        StaticJsonDocument<384> response;
        response["success"] = true;
        response["darkVisibleOffset"] = updated.skyCalibration.darkVisibleOffset;
        response["sampleCount"] = updated.skyCalibration.darkSampleCount;
        response["darkCalibratedAt"] = updated.skyCalibration.darkCalibratedAt;

        String responseStr;
        serializeJson(response, responseStr);
        request->send(200, "application/json", responseStr.c_str());
    }

    void WebServer::handleRG15ResetTotal(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;

        const bool ok = rg15Sensor.resetTotalAccumulation();
        if (!ok)
        {
            request->send(502, "application/json", createErrorJson("RG-15 total accumulation reset failed").c_str());
            return;
        }
        request->send(
            200, "application/json", "{\"success\":true,\"command\":\"O\",\"message\":\"RG-15 total accumulation reset command sent\"}");
    }

    void WebServer::handleRG15Reboot(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;

        const bool ok = rg15Sensor.rebootSensor();
        if (!ok)
        {
            request->send(502, "application/json", createErrorJson("RG-15 reboot command failed").c_str());
            return;
        }
        request->send(200, "application/json", "{\"success\":true,\"command\":\"K\",\"message\":\"RG-15 reboot command sent\"}");
    }

    void WebServer::pollWiFiConnect()
    {
        if (!wifiConnectActive)
        {
            return;
        }

        const wl_status_t status = WiFi.status();
        if (status == WL_CONNECTED)
        {
            if (!wifiConnectConfigSaved)
            {
                Config config = getConfigCallback();
                config.wifi.ssid = pendingWifiSSID;
                config.wifi.password = pendingWifiPassword;
                wifiConnectConfigSaved = saveConfigCallback(config);
                if (!wifiConnectConfigSaved)
                {
                    Logger::error(TAG, "Connected to WiFi but failed to save config");
                }
            }

            Logger::info(TAG, "WiFi connected. IP: %s", WiFi.localIP().toString().c_str());
            // Restart onto the new network once the setup screen has had time
            // to show the new address.
            if (wifiConnectConfigSaved)
                scheduleRestart(15000);
            wifiConnectActive = false;
            pendingWifiSSID.clear();
            pendingWifiPassword.clear();
            return;
        }

        if (millis() - wifiConnectStartedAt >= WIFI_CONNECT_TIMEOUT_MS)
        {
            Logger::error(TAG, "WiFi connection timed out. Status: %d", status);
            WiFi.disconnect(false);
            wifiConnectActive = false;
            pendingWifiSSID.clear();
            pendingWifiPassword.clear();
        }
    }

    void WebServer::handleMQTTTest(AsyncWebServerRequest *request, JsonVariant &json)
    {
        if (!requireAuth(request))
            return;

        JsonObject jsonObj = json.as<JsonObject>();

        if (!jsonObj.containsKey("broker") || !jsonObj.containsKey("port"))
        {
            request->send(400, "application/json", "{\"error\":\"Missing broker or port\"}");
            return;
        }

        const char *broker = jsonObj["broker"];
        uint16_t port = jsonObj["port"];
        const char *username = jsonObj["username"] | "";
        const char *password = jsonObj["password"] | "";
        const char *clientId = jsonObj["clientId"] | "SQM-Test";

        // "[fd00::10]:1883" and bare IPv6 work as in the saved settings (spec 015).
        Net::Host host;
        const Net::HostError hostError = Net::parseHost(broker, host);
        if (hostError != Net::HostError::None)
        {
            StaticJsonDocument<192> reply;
            reply["success"] = false;
            reply["error"] = std::string("MQTT broker: ") + Net::hostErrorText(hostError);
            std::string body;
            serializeJson(reply, body);
            request->send(400, "application/json", body.c_str());
            return;
        }
        if (host.port != 0)
            port = host.port;
        Logger::info(TAG, "Testing MQTT connection to %s port %d", host.name.c_str(), port);

        DualStackClient testWifiClient;
        PubSubClient testMqtt(host.name.c_str(), port, testWifiClient);

        bool connected = false;
        String errorMsg = "";

        // Try to connect with or without auth
        if (strlen(username) > 0)
        {
            connected = testMqtt.connect(clientId, username, password);
        }
        else
        {
            connected = testMqtt.connect(clientId);
        }

        StaticJsonDocument<256> response;

        if (connected)
        {
            Logger::info(TAG, "MQTT test connection successful");
            response["success"] = true;
            response["message"] = "Connection successful";
            testMqtt.disconnect();
        }
        else
        {
            int state = testMqtt.state();
            Logger::error(TAG, "MQTT test connection failed with state: %d", state);

            // Provide detailed error messages based on state
            switch (state)
            {
            case -4:
                response["error"] = "Connection timeout";
                break;
            case -3:
                response["error"] = "Connection lost";
                break;
            case -2:
                response["error"] = "Connect failed";
                break;
            case -1:
                response["error"] = "Disconnected";
                break;
            case 1:
                response["error"] = "Bad protocol";
                break;
            case 2:
                response["error"] = "Bad client ID";
                break;
            case 3:
                response["error"] = "Unavailable";
                break;
            case 4:
                response["error"] = "Bad credentials - check username/password";
                break;
            case 5:
                response["error"] = "Unauthorized";
                break;
            default:
                response["error"] = "Unknown error";
            }
            response["state"] = state;
        }

        String responseStr;
        serializeJson(response, responseStr);
        request->send(connected ? 200 : 502, "application/json", responseStr.c_str());
    }

    void WebServer::broadcastSensorData()
    {
        // Skip a beat rather than queue more behind a slow client.
        if (wsSensors.count() == 0 || !wsSensors.availableForWriteAll())
            return;

        // Send only sensor data to Dashboard clients
        std::string json = createSensorDataJson();
        wsSensors.textAll(json.c_str());
    }

    void WebServer::broadcastStatusData()
    {
        if (wsStatus.count() == 0 || !wsStatus.availableForWriteAll())
            return;

        // Send only status data to System page clients
        std::string json = createStatusJson();
        wsStatus.textAll(json.c_str());
    }

    void WebServer::onSensorWebSocketEvent(
        AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
    {
        switch (type)
        {
        case WS_EVT_CONNECT:
            Logger::info(TAG, "Sensor WebSocket client connected: %u", client->id());
            // Send initial sensor data
            client->text(createSensorDataJson().c_str());
            break;

        case WS_EVT_DISCONNECT:
            Logger::info(TAG, "Sensor WebSocket client disconnected: %u", client->id());
            break;

        default:
            break;
        }
    }

    void WebServer::onStatusWebSocketEvent(
        AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
    {
        switch (type)
        {
        case WS_EVT_CONNECT:
            Logger::info(TAG, "Status WebSocket client connected: %u", client->id());
            // Send initial status data
            client->text(createStatusJson().c_str());
            break;

        case WS_EVT_DISCONNECT:
            Logger::info(TAG, "Status WebSocket client disconnected: %u", client->id());
            break;

        case WS_EVT_ERROR:
            Logger::error(TAG, "WebSocket error: %u", client->id());
            break;

        case WS_EVT_DATA:
            // Handle incoming WebSocket messages if needed
            break;

        default:
            break;
        }
    }

    SensorSnapshot WebServer::getSensorSnapshot() const
    {
        SensorSnapshot snapshot;
        if (!sensorSnapshotMutex)
        {
            return sensorSnapshot;
        }

        if (xSemaphoreTake(sensorSnapshotMutex, pdMS_TO_TICKS(20)) == pdTRUE)
        {
            snapshot = sensorSnapshot;
            xSemaphoreGive(sensorSnapshotMutex);
        }
        else
        {
            Logger::warn(TAG, "Sensor snapshot read lock unavailable");
        }
        return snapshot;
    }

    uint32_t WebServer::ageMs(uint32_t now, uint32_t timestamp)
    {
        return timestamp == 0 ? 0 : now - timestamp;
    }

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

    std::string WebServer::createStatusJson() const
    {
        DynamicJsonDocument doc(7168); // Includes MQTT, partition, boot, sensor, BLE and alert-schedule diagnostics
        const SensorSnapshot snapshot = getSensorSnapshot();

        // Firmware version
        JsonObject firmware = doc.createNestedObject("firmware");
        firmware["name"] = FIRMWARE_NAME;
        firmware["version"] = FIRMWARE_VERSION;
        firmware["buildDate"] = FIRMWARE_BUILD_DATE;
        firmware["buildTime"] = FIRMWARE_BUILD_TIME;
        firmware["variant"] = BleService::available() ? "ble" : "standard";

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

        // System stats
        doc["uptime"] = millis() / 1000;
        doc["freeHeap"] = ESP.getFreeHeap();
        // Stack headroom (bytes never used). This handler runs on the
        // AsyncTCP task, so "asyncTcp" is that task's own high-water mark.
        JsonObject stacks = doc.createNestedObject("stackFree");
        stacks["asyncTcp"] = uxTaskGetStackHighWaterMark(nullptr);
        stacks["loop"] = loopTaskHandle != nullptr ? uxTaskGetStackHighWaterMark(loopTaskHandle) : 0;
        doc["sensorSnapshotBytes"] = sizeof(SensorSnapshot);

        Core::writeSky(doc.createNestedObject("sky"), computeNight(snapshot, getConfigCallback()));
        Core::writeAlertSchedule(doc.createNestedObject("alerts"), sharedSchedule(), getConfigCallback(), millis());
        Core::writeClientWatch(doc.createNestedObject("alpaca"), sharedClientWatch(), getConfigCallback(), millis());

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
        const esp_partition_t *fs_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
        if (fs_partition)
        {
            partitions["fsAddress"] = fs_partition->address;
            partitions["fsSize"] = fs_partition->size;
        }

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

        // Per-sensor health for present hardware, and bring-up diagnostics.
        // Readings themselves are in /api/sensors.
        const Readings::Snapshot readings = buildReadings();
        Core::writeSensorHealth(doc.createNestedObject("sensors"), readings, getConfigCallback());

        JsonObject diagnostics = doc.createNestedObject("diagnostics");
        appendDiagnostics(diagnostics, snapshot);

        // MQTT status
        if (mqttClient)
        {
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

        std::string json;
        serializeJson(doc, json);
        return json;
    }

    bool WebServer::scheduleRestart(uint32_t delayMs)
    {
        if (!restartTimer)
        {
            esp_timer_create_args_t restartTimerArgs = {};
            restartTimerArgs.callback = &restartTimerCallback;
            restartTimerArgs.arg = nullptr;
            restartTimerArgs.dispatch_method = ESP_TIMER_TASK;
            restartTimerArgs.name = "sqm_restart";
            restartTimerArgs.skip_unhandled_events = false;

            if (esp_timer_create(&restartTimerArgs, &restartTimer) != ESP_OK)
            {
                Logger::error(TAG, "Failed to create restart timer");
                return false;
            }
        }

        esp_timer_stop(restartTimer);
        if (esp_timer_start_once(restartTimer, static_cast<uint64_t>(delayMs) * 1000ULL) != ESP_OK)
        {
            Logger::error(TAG, "Failed to schedule restart");
            return false;
        }

        return true;
    }

    bool WebServer::requireAuth(AsyncWebServerRequest *request) const
    {
        const Config &cfg = getConfigCallback();
        if (!cfg.auth.enabled || cfg.auth.password.empty())
        {
            return true;
        }
        if (!request->authenticate(cfg.auth.username.c_str(), cfg.auth.password.c_str()))
        {
            request->requestAuthentication("SQMeter", false);
            return false;
        }
        return true;
    }

    std::string WebServer::createErrorJson(const char *error)
    {
        StaticJsonDocument<128> doc;
        doc["error"] = error;

        std::string json;
        serializeJson(doc, json);
        return json;
    }

    void WebServer::setOTAProgress(int progress)
    {
        StaticJsonDocument<128> doc;
        doc["type"] = "ota_progress";
        doc["progress"] = progress;

        std::string json;
        serializeJson(doc, json);
        // Send OTA progress to status WebSocket (System page handles OTA)
        wsStatus.textAll(json.c_str());
    }

    void WebServer::setOTAError(const char *error)
    {
        // Send OTA errors to status WebSocket (System page handles OTA)
        wsStatus.textAll(createErrorJson(error).c_str());
    }

} // namespace SQM
