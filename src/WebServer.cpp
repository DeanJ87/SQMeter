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
#include <nvs.h>
#include <nvs_flash.h>
#include <ctime>
#include "calculations/CloudDetection.h"
#include "sensors/RG15Sensor.h"
#include "AlpacaDiscovery.h"
#include "HeapTrace.h"
#include "SunPosition.h"

extern uint32_t bootCount;

namespace SQM
{
    namespace
    {
        constexpr size_t CONFIG_JSON_BUFFER_SIZE = 8192;

        esp_timer_handle_t restartTimer = nullptr;

        void restartTimerCallback(void *)
        {
            ESP.restart();
        }

        const char *rg15StateToString(RG15State state)
        {
            switch (state)
            {
            case RG15State::RG15_DISABLED:
                return "disabled";
            case RG15State::RG15_CONFIGURED:
                return "configured";
            case RG15State::RG15_UART_OPENED:
                return "uart_opened";
            case RG15State::RG15_CONFIGURING:
                return "configuring";
            case RG15State::RG15_COMMAND_SENT:
                return "command_sent";
            case RG15State::RG15_AWAITING_RESPONSE:
                return "awaiting_response";
            case RG15State::RG15_ACKNOWLEDGED:
                return "acknowledged";
            case RG15State::RG15_READING_RECEIVED:
                return "reading_received";
            case RG15State::RG15_PARSE_ERROR:
                return "parse_error";
            case RG15State::RG15_TIMEOUT:
                return "timeout";
            case RG15State::RG15_STALE:
                return "stale";
            case RG15State::RG15_ONLINE:
                return "online";
            default:
                return "unknown";
            }
        }

        static void appendOptionalString(JsonObject obj, const char *key, const std::optional<std::string> &value)
        {
            if (value)
            {
                obj[key] = value->c_str();
            }
            else
            {
                obj[key] = nullptr;
            }
        }

        static void appendRG15Diagnostics(JsonObject root, const RG15Reading &reading, const RG15Diagnostics &diag, uint32_t now)
        {
            root["enabled"] = diag.enabled;
            root["sensor"] = "hydreon_rg15";
            root["initialized"] = diag.uartOpened;
            root["online"] = reading.online;
            root["stale"] = reading.stale;
            root["state"] = rg15StateToString(diag.state);
            root["timestamp"] = reading.timestamp;
            root["ageMs"] = reading.ageMs;
            root["status"] = static_cast<int>(reading.status);
            root["isRaining"] = reading.isRaining;
            root["raining"] = reading.rainLatched;
            root["acc"] = reading.acc;
            root["eventAcc"] = reading.eventAcc;
            root["totalAcc"] = reading.totalAcc;
            root["rInt"] = reading.rInt;
            root["accumulation_since_last_read"] = reading.acc;
            root["event_accumulation"] = reading.localEventAcc;
            root["local_event_accumulation"] = reading.localEventAcc;
            root["hydreon_event_accumulation"] = reading.eventAcc;
            root["total_accumulation"] = reading.totalAcc;
            root["rain_intensity"] = reading.rInt;
            root["lensBad"] = reading.lensBad;
            root["emSat"] = reading.emSat;

            JsonObject uart = root.createNestedObject("uart");
            uart["configured"] = diag.configured;
            uart["opened"] = diag.uartOpened;
            uart["rx_pin"] = diag.rxPin;
            uart["tx_pin"] = diag.txPin;
            uart["baud_rate"] = diag.baudRate;
            uart["uart_port"] = diag.uartPort;
            uart["mode"] = diag.mode;
            uart["resolution"] = diag.resolution;
            uart["units"] = diag.units;
            uart["debug_uart"] = diag.debugUart;
            uart["poll_interval_ms"] = diag.pollIntervalMs;
            uart["rain_clear_delay_ms"] = diag.rainClearDelayMs;
            uart["daily_reset_enabled"] = diag.dailyResetEnabled;
            uart["daily_reset_hour"] = diag.dailyResetHour;
            uart["daily_reset_minute"] = diag.dailyResetMinute;
            appendOptionalString(uart, "last_command", diag.lastCommand);
            if (diag.lastCommandMs != 0)
                uart["last_command_ms"] = static_cast<uint32_t>(diag.lastCommandMs);
            else
                uart["last_command_ms"] = nullptr;
            uart["last_bytes_written"] = diag.lastBytesWritten;
            appendOptionalString(uart, "expected_ack", diag.expectedAck);
            appendOptionalString(uart, "last_ack", diag.lastAck);
            if (diag.lastAckMs != 0)
                uart["last_ack_ms"] = static_cast<uint32_t>(diag.lastAckMs);
            else
                uart["last_ack_ms"] = nullptr;
            appendOptionalString(uart, "last_raw_response", diag.lastRawResponse);
            if (diag.lastResponseMs != 0)
                uart["last_response_ms"] = static_cast<uint32_t>(diag.lastResponseMs);
            else
                uart["last_response_ms"] = nullptr;
            appendOptionalString(uart, "last_error", diag.lastError);
            uart["timeouts"] = diag.timeouts;
            uart["parse_errors"] = diag.parseErrors;
            uart["successful_reads"] = diag.successfulReads;
            uart["response_timeout_ms"] = diag.responseTimeoutMs;
            uart["stale_timeout_ms"] = diag.staleTimeoutMs;
            if (diag.lastHealthCheckMs != 0)
                uart["last_health_check_ms"] = static_cast<uint32_t>(diag.lastHealthCheckMs);
            else
                uart["last_health_check_ms"] = nullptr;
            if (diag.lastHealthCheckMs != 0)
                uart["last_health_check_age_ms"] = static_cast<uint32_t>(now - diag.lastHealthCheckMs);
            else
                uart["last_health_check_age_ms"] = nullptr;
            if (diag.lastPollMs != 0)
                uart["last_poll_ms"] = static_cast<uint32_t>(diag.lastPollMs);
            else
                uart["last_poll_ms"] = nullptr;
            if (diag.lastPollMs != 0)
                uart["last_poll_age_ms"] = static_cast<uint32_t>(now - diag.lastPollMs);
            else
                uart["last_poll_age_ms"] = nullptr;
            if (diag.lastRainDetectedMs != 0)
                uart["last_rain_detected_ms"] = static_cast<uint32_t>(diag.lastRainDetectedMs);
            else
                uart["last_rain_detected_ms"] = nullptr;
            if (diag.lastRainDetectedMs != 0)
                uart["last_rain_detected_age_ms"] = static_cast<uint32_t>(now - diag.lastRainDetectedMs);
            else
                uart["last_rain_detected_age_ms"] = nullptr;
            if (diag.lastTotalResetMs != 0)
                uart["last_total_reset_ms"] = static_cast<uint32_t>(diag.lastTotalResetMs);
            else
                uart["last_total_reset_ms"] = nullptr;
            if (diag.lastTotalResetMs != 0)
                uart["last_total_reset_age_ms"] = static_cast<uint32_t>(now - diag.lastTotalResetMs);
            else
                uart["last_total_reset_age_ms"] = nullptr;
            if (diag.lastRebootCommandMs != 0)
                uart["last_reboot_command_ms"] = static_cast<uint32_t>(diag.lastRebootCommandMs);
            else
                uart["last_reboot_command_ms"] = nullptr;
            if (diag.lastRebootCommandMs != 0)
                uart["last_reboot_command_age_ms"] = static_cast<uint32_t>(now - diag.lastRebootCommandMs);
            else
                uart["last_reboot_command_age_ms"] = nullptr;
            appendOptionalString(uart, "last_status_line", diag.lastStatusLine);
            appendOptionalString(uart, "software_version", diag.softwareVersion);
            appendOptionalString(uart, "software_build_date", diag.softwareBuildDate);
            appendOptionalString(uart, "reset_reason", diag.resetReason);
            if (diag.powerOnDays)
                uart["power_on_days"] = *diag.powerOnDays;
            else
                uart["power_on_days"] = nullptr;
            if (diag.emitter1)
                uart["emitter_1"] = *diag.emitter1;
            else
                uart["emitter_1"] = nullptr;
            if (diag.emitter2)
                uart["emitter_2"] = *diag.emitter2;
            else
                uart["emitter_2"] = nullptr;
            if (diag.emitterTotal)
                uart["emitter_total"] = *diag.emitterTotal;
            else
                uart["emitter_total"] = nullptr;
            if (diag.lastResponseMs != 0)
                uart["last_response_age_ms"] = static_cast<uint32_t>(now - diag.lastResponseMs);
            else
                uart["last_response_age_ms"] = nullptr;
            if (diag.lastSuccessfulReadMs != 0)
                uart["last_successful_read_ms"] = static_cast<uint32_t>(diag.lastSuccessfulReadMs);
            else
                uart["last_successful_read_ms"] = nullptr;
            if (diag.lastSuccessfulReadMs != 0)
                uart["last_successful_read_age_ms"] = static_cast<uint32_t>(now - diag.lastSuccessfulReadMs);
            else
                uart["last_successful_read_age_ms"] = nullptr;
        }
    }

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
            [this](int percent)
            { setOTAProgress(percent); },
            [this](const char *message)
            { setOTAError(message); },
            []
            { WebServer::scheduleRestart(1000); });
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
        Logger::info(TAG, "Starting web server on port %d", PORT);

        // CRITICAL: Register API routes BEFORE static file serving
        // Otherwise /api/* requests get treated as filesystem paths
        setupAPIRoutes();
        setupWebSocket();
        setupOTA();
        setupGithubUpdates();
        setupAlpacaRoutes();
        setupAlertRoutes();
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
            if (alpacaDiscoveryUdp.begin(Alpaca::DISCOVERY_UDP_PORT))
            {
                alpacaDiscoveryStarted = true;
                Logger::info(TAG, "Alpaca UDP discovery listening on port %u", Alpaca::DISCOVERY_UDP_PORT);
            }
            else
            {
                Logger::error(TAG, "Failed to start Alpaca UDP discovery listener");
            }
        }

        // SPA fallback - serve index.html for any non-API routes
        server.onNotFound([](AsyncWebServerRequest *request)
                          { 
            String path = request->url();
            // Alpaca device API: the spec requires HTTP 400 with a plain-text
            // body for an unknown device type/number, method, or HTTP verb.
            if (path.startsWith("/api/v1/")) {
                Logger::debug(TAG, "400 Invalid Alpaca request: %s %s", request->methodToString(), path.c_str());
                request->send(400, "text/plain", "Invalid Alpaca device type, device number, method or HTTP verb");
                return;
            }
            // If it's an API route, return 404 JSON
            if (path.startsWith("/api/")) {
                Logger::debug(TAG, "404 Not Found (API): %s", path.c_str());
                request->send(404, "application/json", "{\"error\":\"Not found\"}");
            } else {
                // For all other routes, serve index.html (SPA routing)
                Logger::debug(TAG, "SPA fallback for: %s", path.c_str());
                request->send(LittleFS, "/index.html", "text/html");
            } });

        server.begin();
        Logger::info(TAG, "Web server started");
    }

    void WebServer::handle()
    {
        wsSensors.cleanupClients();
        wsStatus.cleanupClients();
        pollWiFiConnect();
        handleAlpacaDiscovery();

        const uint32_t now = millis();

        if (now - lastSafetyEvaluation >= SAFETY_EVALUATION_INTERVAL_MS)
        {
            updateSafetyStatus();
            lastSafetyEvaluation = now;
        }

        const uint8_t testMask = pendingAlertTestMask.exchange(0);
        if (testMask != 0)
        {
            const Config &cfg = getConfigCallback();
            Alerts::Alert test;
            test.type = Alerts::AlertType::Test;
            test.level = Alerts::AlertLevel::Normal;
            test.title = "Test notification";
            test.message = "Alerts from this SQMeter are working.";
            alertDispatcher->dispatch(test, cfg.alerts, cfg.deviceName, testMask);
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
        // Captive portal detection URLs for iOS, Android, etc.
        server.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->redirect("/"); });

        server.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->redirect("/"); });

        server.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->redirect("/"); });

        server.on("/gen_204", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->redirect("/"); });

        server.on("/success.txt", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send(200, "text/plain", "Success"); });

        server.on("/connecttest.txt", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->redirect("/"); });

        // Catch-all for Microsoft Windows captive portal detection
        server.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->send(200, "text/plain", "Microsoft NCSI"); });

        // Serve files from LittleFS
        server.serveStatic("/", LittleFS, "/")
            .setDefaultFile("index.html")
            .setCacheControl("no-cache, no-store, must-revalidate");
    }

    void WebServer::setupAPIRoutes()
    {
        // Status endpoint
        server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request)
                  { handleGetStatus(request); });

        // Sensors endpoint
        server.on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest *request)
                  { handleGetSensors(request); });

        server.on("/api/safety", HTTP_GET, [this](AsyncWebServerRequest *request)
                  {
            DynamicJsonDocument doc(1536);
            appendSafetyStatus(doc.to<JsonObject>());
            std::string json;
            serializeJson(doc, json);
            request->send(200, "application/json", json.c_str()); });

        server.on("/api/sensors/tsl2591/calibrate-dark", HTTP_POST, [this](AsyncWebServerRequest *request)
                  { handleTSL2591DarkCalibration(request); });

        // Config endpoints
        server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request)
                  { handleGetConfig(request); });

        AsyncCallbackJsonWebHandler *configHandler = new AsyncCallbackJsonWebHandler(
            "/api/config",
            [this](AsyncWebServerRequest *request, JsonVariant &json)
            {
                if (!requireAuth(request))
                    return;

                String jsonStr;
                serializeJson(json, jsonStr);
                const Config &currentConfig = getConfigCallback();
                auto configOpt = Config::fromJson(jsonStr.c_str(), &currentConfig);

                if (!configOpt)
                {
                    request->send(400, "application/json", createErrorJson("Invalid configuration").c_str());
                    return;
                }

                if (saveConfigCallback(*configOpt))
                {
                    tslSensor.configureSkyMeasurement(configOpt->skyAveraging, configOpt->skyCalibration);
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
        server.on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest *request)
                  { handleRestart(request); });

        // WiFi endpoints
        server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request)
                  { handleWiFiScan(request); });

        server.on("/api/sensors/rg15/test", HTTP_POST, [this](AsyncWebServerRequest *request)
                  { handleRG15Test(request); });
        server.on("/api/sensors/rg15/reset-total", HTTP_POST, [this](AsyncWebServerRequest *request)
                  { handleRG15ResetTotal(request); });
        server.on("/api/sensors/rg15/reboot", HTTP_POST, [this](AsyncWebServerRequest *request)
                  { handleRG15Reboot(request); });

        // MQTT test endpoint
        AsyncCallbackJsonWebHandler *mqttTestHandler = new AsyncCallbackJsonWebHandler(
            "/api/mqtt/test",
            [this](AsyncWebServerRequest *request, JsonVariant &json)
            {
                handleMQTTTest(request, json);
            });
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
        wsSensors.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                                 AwsEventType type, void *arg, uint8_t *data, size_t len)
                          { onSensorWebSocketEvent(server, client, type, arg, data, len); });

        // Status WebSocket for System page (/ws/status)
        wsStatus.onEvent([this](AsyncWebSocket *server, AsyncWebSocketClient *client,
                                AwsEventType type, void *arg, uint8_t *data, size_t len)
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

        server.on("/api/update/fs", HTTP_POST, [this](AsyncWebServerRequest *request)
                  {
            if (!requireAuth(request))
                return;
            String response_json;
            
            if (fs_update_error) {
                response_json = "{\"success\":false,\"error\":\"" + fs_error_msg + "\"}";
            } else {
                response_json = "{\"success\":true}";
            }
            
            // Reset state
            fs_partition = nullptr;
            fs_bytes_written = 0;
            fs_update_error = false;
            fs_error_msg = "";
            
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", response_json);
            response->addHeader("Connection", "close");
            request->send(response);
            
            if (!fs_update_error) {
                WebServer::scheduleRestart(1000);
            } }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
                  {
            if (!index) {
                Logger::info("OTA", "Filesystem update started: %s", filename.c_str());
                fs_bytes_written = 0;
                fs_update_error = false;
                fs_error_msg = "";
                
                // Find the LittleFS partition (labeled as "spiffs" in partition table)
                fs_partition = esp_partition_find_first(
                    ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
                
                if (!fs_partition) {
                    Logger::error("OTA", "Filesystem partition not found!");
                    fs_update_error = true;
                    fs_error_msg = "Filesystem partition not found";
                    return;
                }
                
                Logger::info("OTA", "Found filesystem partition at 0x%x, size %u bytes",
                    fs_partition->address, static_cast<unsigned>(fs_partition->size));
                
                // Unmount LittleFS before writing
                LittleFS.end();
                
                // Erase the partition
                Logger::info("OTA", "Erasing filesystem partition...");
                esp_err_t err = esp_partition_erase_range(fs_partition, 0, fs_partition->size);
                if (err != ESP_OK) {
                    Logger::error("OTA", "Partition erase failed: %d", err);
                    fs_update_error = true;
                    fs_error_msg = "Failed to erase partition";
                    return;
                }
                Logger::info("OTA", "Partition erased successfully");
            }
            
            if (!fs_update_error && fs_partition) {
                // Write data directly to partition (no magic byte validation)
                esp_err_t err = esp_partition_write(fs_partition, fs_bytes_written, data, len);
                if (err != ESP_OK) {
                    Logger::error("OTA", "Partition write failed at offset %u: %d", static_cast<unsigned>(fs_bytes_written), err);
                    fs_update_error = true;
                    fs_error_msg = "Failed to write to partition";
                    return;
                }
                fs_bytes_written += len;
                
                if (index % 10240 == 0) {  // Log every ~10KB
                    Logger::info("OTA", "Written %u bytes", static_cast<unsigned>(fs_bytes_written));
                }
            }
            
            if (final) {
                if (!fs_update_error) {
                    Logger::info("OTA", "Filesystem update success: %u bytes written", static_cast<unsigned>(fs_bytes_written));
                } else {
                    Logger::error("OTA", "Filesystem update failed: %s", fs_error_msg.c_str());
                }
            } });

        // Firmware OTA update (app partition). Registered after /api/update/fs:
        // this server also matches "/api/update" as a prefix of
        // "/api/update/fs", so registered first it took filesystem uploads too.
        server.on("/api/update", HTTP_POST, [this](AsyncWebServerRequest *request)
                  {
            if (!requireAuth(request))
                return;
            bool success = !Update.hasError();
            String response_json;
            
            if (success) {
                response_json = "{\"success\":true}";
            } else {
                // Get detailed error message
                String error_msg = "Unknown error";
                uint8_t error = Update.getError();
                switch(error) {
                    case UPDATE_ERROR_OK: error_msg = "No error"; break;
                    case UPDATE_ERROR_WRITE: error_msg = "Flash write failed"; break;
                    case UPDATE_ERROR_ERASE: error_msg = "Flash erase failed"; break;
                    case UPDATE_ERROR_READ: error_msg = "Flash read failed"; break;
                    case UPDATE_ERROR_SPACE: error_msg = "Not enough space"; break;
                    case UPDATE_ERROR_SIZE: error_msg = "Bad size given"; break;
                    case UPDATE_ERROR_STREAM: error_msg = "Stream read timeout"; break;
                    case UPDATE_ERROR_MD5: error_msg = "MD5 check failed"; break;
                    case UPDATE_ERROR_MAGIC_BYTE: error_msg = "Wrong magic byte"; break;
                    case UPDATE_ERROR_ACTIVATE: error_msg = "Could not activate partition"; break;
                    case UPDATE_ERROR_NO_PARTITION: error_msg = "Partition not found"; break;
                    case UPDATE_ERROR_BAD_ARGUMENT: error_msg = "Bad argument"; break;
                    case UPDATE_ERROR_ABORT: error_msg = "Update aborted"; break;
                    default: error_msg = "Error code: " + String(error); break;
                }
                response_json = "{\"success\":false,\"error\":\"" + error_msg + "\"}";
            }
            
            AsyncWebServerResponse* response = request->beginResponse(200, "application/json", response_json);
            response->addHeader("Connection", "close");
            request->send(response);
            
            if (success) {
                WebServer::scheduleRestart(1000);
            } }, [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final)
                  {
            if (!index) {
                Logger::info("OTA", "Firmware update started: %s", filename.c_str());
                if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                    Logger::error("OTA", "Update.begin failed: %d", Update.getError());
                    Update.printError(Serial);
                }
            }
            
            if (Update.write(data, len) != len) {
                Logger::error("OTA", "Update.write failed: %d", Update.getError());
                Update.printError(Serial);
            }
            
            if (final) {
                if (Update.end(true)) {
                    Logger::info("OTA", "Firmware update success, rebooting...");
                } else {
                    Logger::error("OTA", "Update.end failed: %d", Update.getError());
                    Update.printError(Serial);
                }
            } });
    }

    void WebServer::setupGithubUpdates()
    {
        // Check for available releases on the given track (?track=stable|beta,
        // defaults to stable). Returns the filtered release list; staleness
        // relative to the running firmware is computed client-side.
        server.on("/api/updates/check", HTTP_GET, [this](AsyncWebServerRequest *request)
                  {
            if (!requireAuth(request))
                return;

            std::string track = "stable";
            if (request->hasParam("track")) {
                String t = request->getParam("track")->value();
                if (t == "beta") track = "beta";
            }

            std::string error;
            std::vector<GithubRelease> releases = otaUpdater->checkForUpdate(track, error);

            if (!error.empty()) {
                AsyncWebServerResponse *response = request->beginResponse(
                    502, "application/json", createErrorJson(error.c_str()).c_str());
                request->send(response);
                return;
            }

            DynamicJsonDocument doc(8192);
            JsonArray arr = doc.to<JsonArray>();
            for (const GithubRelease &r : releases) {
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
            request->send(200, "application/json", json.c_str()); });

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
                    request->send(400, "application/json",
                                  createErrorJson("firmwareAssetUrl and fsAssetUrl are required").c_str());
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

    namespace
    {
        // Alpaca parameter names are case-insensitive, and may arrive in the
        // query string (GET) or the form-encoded body (PUT) - search both.
        const AsyncWebParameter *findAlpacaParam(AsyncWebServerRequest *request, const char *name)
        {
            const size_t count = request->params();
            for (size_t i = 0; i < count; ++i)
            {
                const AsyncWebParameter *param = request->getParam(i);
                if (param != nullptr && !param->isFile() && Alpaca::paramNameEquals(param->name().c_str(), name))
                    return param;
            }
            return nullptr;
        }

        uint32_t getAlpacaClientTransactionId(AsyncWebServerRequest *request)
        {
            const AsyncWebParameter *param = findAlpacaParam(request, "ClientTransactionID");
            return param != nullptr ? Alpaca::parseClientTransactionId(param->value().c_str()) : 0;
        }
    }

    std::string WebServer::buildAlpacaResponseBool(AsyncWebServerRequest *request, bool value, int errorNumber, const std::string &errorMessage) const
    {
        StaticJsonDocument<192> doc;
        doc["Value"] = value;
        doc["ClientTransactionID"] = getAlpacaClientTransactionId(request);
        doc["ServerTransactionID"] = ++alpacaServerTransactionId;
        doc["ErrorNumber"] = errorNumber;
        doc["ErrorMessage"] = errorMessage;
        std::string json;
        serializeJson(doc, json);
        return json;
    }

    std::string WebServer::buildAlpacaResponseDouble(AsyncWebServerRequest *request, double value, int errorNumber, const std::string &errorMessage) const
    {
        StaticJsonDocument<192> doc;
        doc["Value"] = value;
        doc["ClientTransactionID"] = getAlpacaClientTransactionId(request);
        doc["ServerTransactionID"] = ++alpacaServerTransactionId;
        doc["ErrorNumber"] = errorNumber;
        doc["ErrorMessage"] = errorMessage;
        std::string json;
        serializeJson(doc, json);
        return json;
    }

    std::string WebServer::buildAlpacaResponseVoid(AsyncWebServerRequest *request, int errorNumber, const std::string &errorMessage) const
    {
        StaticJsonDocument<192> doc;
        doc["ClientTransactionID"] = getAlpacaClientTransactionId(request);
        doc["ServerTransactionID"] = ++alpacaServerTransactionId;
        doc["ErrorNumber"] = errorNumber;
        doc["ErrorMessage"] = errorMessage;
        std::string json;
        serializeJson(doc, json);
        return json;
    }

    namespace
    {
        std::string buildAlpacaResponseString(AsyncWebServerRequest *request, const std::string &value, uint32_t &txnCounter)
        {
            StaticJsonDocument<256> doc;
            doc["Value"] = value;
            doc["ClientTransactionID"] = getAlpacaClientTransactionId(request);
            doc["ServerTransactionID"] = ++txnCounter;
            doc["ErrorNumber"] = 0;
            doc["ErrorMessage"] = "";
            std::string json;
            serializeJson(doc, json);
            return json;
        }

        std::string buildAlpacaResponseStringWithError(AsyncWebServerRequest *request, const std::string &value, int errorNumber, const std::string &errorMessage, uint32_t &txnCounter)
        {
            StaticJsonDocument<384> doc;
            doc["Value"] = value;
            doc["ClientTransactionID"] = getAlpacaClientTransactionId(request);
            doc["ServerTransactionID"] = ++txnCounter;
            doc["ErrorNumber"] = errorNumber;
            doc["ErrorMessage"] = errorMessage;
            std::string json;
            serializeJson(doc, json);
            return json;
        }

        std::string buildAlpacaResponseInt(AsyncWebServerRequest *request, int value, uint32_t &txnCounter)
        {
            StaticJsonDocument<192> doc;
            doc["Value"] = value;
            doc["ClientTransactionID"] = getAlpacaClientTransactionId(request);
            doc["ServerTransactionID"] = ++txnCounter;
            doc["ErrorNumber"] = 0;
            doc["ErrorMessage"] = "";
            std::string json;
            serializeJson(doc, json);
            return json;
        }

        std::string buildAlpacaResponseIntArray(AsyncWebServerRequest *request, const std::vector<int> &values, uint32_t &txnCounter)
        {
            DynamicJsonDocument doc(256);
            JsonArray arr = doc.createNestedArray("Value");
            for (int v : values)
                arr.add(v);
            doc["ClientTransactionID"] = getAlpacaClientTransactionId(request);
            doc["ServerTransactionID"] = ++txnCounter;
            doc["ErrorNumber"] = 0;
            doc["ErrorMessage"] = "";
            std::string json;
            serializeJson(doc, json);
            return json;
        }

        std::string buildAlpacaResponseStringArray(AsyncWebServerRequest *request, const std::vector<std::string> &values, uint32_t &txnCounter)
        {
            DynamicJsonDocument doc(256);
            JsonArray arr = doc.createNestedArray("Value");
            for (const auto &v : values)
                arr.add(v);
            doc["ClientTransactionID"] = getAlpacaClientTransactionId(request);
            doc["ServerTransactionID"] = ++txnCounter;
            doc["ErrorNumber"] = 0;
            doc["ErrorMessage"] = "";
            std::string json;
            serializeJson(doc, json);
            return json;
        }
    }

    Alpaca::SafetyInputs WebServer::buildAlpacaSafetyInputs() const
    {
        const SensorSnapshot snapshot = getSensorSnapshot();
        const uint32_t now = millis();
        const Config &cfg = getConfigCallback();

        Alpaca::SafetyInputs in;
        in.hasEverHadGoodData = snapshot.dataTimestamp != 0;
        in.secondsSinceLastGoodData = ageMs(now, snapshot.dataTimestamp) / 1000;
        in.skyLightFault = snapshot.tsl.status != SensorStatus::OK;
        in.irSkyFault = snapshot.mlx.status != SensorStatus::OK;
        in.requiredSensorFault = snapshot.tsl.status != SensorStatus::OK ||
                                  snapshot.mlx.status != SensorStatus::OK;

        SkyQualityMetrics sqm = SkyQuality::calculate(snapshot.tsl.lux);
        in.sqm = sqm.sqm;

        bool usingHumidityFallback = snapshot.bme.status != SensorStatus::OK;
        float humidity = usingHumidityFallback ? 53.0f : snapshot.bme.humidity;
        CloudMetrics cloudMetrics = CloudDetection::calculate(
            snapshot.mlx.objectTemp,
            snapshot.mlx.ambientTemp,
            humidity,
            cfg.cloudDetection.clearSkyThreshold,
            cfg.cloudDetection.cloudyThreshold,
            cfg.cloudDetection.humidityCorrection);
        in.cloudCoverPercent = cloudMetrics.cloudCoverPercent;
        in.humidityPercent = humidity;
        in.temperatureC = snapshot.bme.temperature;
        in.dewpointC = snapshot.bme.dewpoint;
        in.environmentSensorFault = usingHumidityFallback;

        in.rainSensorEnabled = cfg.rain.enabled;
        in.rainSensorHealthy = snapshot.rg15.online && !snapshot.rg15.stale &&
                               snapshot.rg15.status == SensorStatus::OK && !snapshot.rg15.lensBad;
        // rainLatched holds for rain.rainClearDelayMs after the last drop -
        // the hold-off before a roof should re-open.
        in.raining = snapshot.rg15.isRaining || snapshot.rg15.rainLatched;

        constexpr uint32_t WIND_STALE_MS = 5000;
        in.windSensorEnabled = cfg.wind.enabled;
        in.windSensorHealthy = snapshot.wind.status == SensorStatus::OK && snapshot.wind.timestamp != 0 &&
                               ageMs(now, snapshot.wind.timestamp) <= WIND_STALE_MS;
        in.windSpeedMs = snapshot.wind.speedMs;
        in.windGustMs = snapshot.wind.gustMs;

        return in;
    }

    Alpaca::SafetyThresholds WebServer::buildAlpacaSafetyThresholds(const Config &cfg)
    {
        Alpaca::SafetyThresholds thresholds;
        thresholds.manualOverrideUnsafe = cfg.alpaca.manualOverrideUnsafe;
        thresholds.staleAfterSeconds = cfg.alpaca.staleAfterSeconds;
        thresholds.cloudCoverEnabled = cfg.alpaca.cloudCoverEnabled;
        thresholds.cloudCoverUnsafePercent = cfg.alpaca.cloudCoverUnsafePercent;
        thresholds.sqmMinEnabled = cfg.alpaca.sqmMinEnabled;
        thresholds.sqmMinSafe = cfg.alpaca.sqmMinSafe;
        thresholds.humidityMaxEnabled = cfg.alpaca.humidityMaxEnabled;
        thresholds.humidityMaxSafe = cfg.alpaca.humidityMaxSafe;
        thresholds.dewpointMarginEnabled = cfg.alpaca.dewpointMarginEnabled;
        thresholds.dewpointMarginMinC = cfg.alpaca.dewpointMarginMinC;
        thresholds.rainUnsafeEnabled = cfg.alpaca.rainUnsafeEnabled;
        thresholds.rainSensorRequired = cfg.alpaca.rainSensorRequired;
        thresholds.windSpeedUnsafeEnabled = cfg.alpaca.windSpeedUnsafeEnabled;
        thresholds.windSpeedUnsafeMs = cfg.alpaca.windSpeedUnsafeMs;
        thresholds.windGustUnsafeEnabled = cfg.alpaca.windGustUnsafeEnabled;
        thresholds.windGustUnsafeMs = cfg.alpaca.windGustUnsafeMs;
        return thresholds;
    }

    Alpaca::SafetyResult WebServer::evaluateAlpacaSafety() const
    {
        return Alpaca::evaluateSafety(buildAlpacaSafetyInputs(), buildAlpacaSafetyThresholds(getConfigCallback()));
    }

    void WebServer::updateSafetyStatus()
    {
        const Alpaca::SafetyResult result = evaluateAlpacaSafety();
        const uint32_t now = millis();
        const bool reportedSafe = safeDelayFilter.update(result.isSafe, now / 1000, getConfigCallback().alpaca.safeDelaySeconds);

        if (safetyMutex && xSemaphoreTake(safetyMutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            if (reportedSafe != safetyStatus.isSafe || safetyStatus.evaluatedAtMs == 0)
            {
                if (safetyStatus.evaluatedAtMs != 0)
                    Logger::info(TAG, "SafetyMonitor now %s", reportedSafe ? "SAFE" : "UNSAFE");
                safetyStatus.changedAtMs = now;
            }
            safetyStatus.isSafe = reportedSafe;
            safetyStatus.rawSafe = result.isSafe;
            safetyStatus.reasonFlags = result.reasonFlags;
            safetyStatus.reasons = result.unsafeReasons;
            safetyStatus.secondsUntilSafe = safeDelayFilter.secondsUntilSafe();
            safetyStatus.evaluatedAtMs = now;
            xSemaphoreGive(safetyMutex);
        }

        processAlerts(getSafetyStatus());
    }

    WebServer::NightState WebServer::computeNight(const SensorSnapshot &snapshot, const Config &cfg)
    {
        NightState night;
        const time_t now = time(nullptr);
        if (snapshot.gps.hasFix)
        {
            night.source = "gps";
            night.latitude = snapshot.gps.latitude;
            night.longitude = snapshot.gps.longitude;
        }
        else if (cfg.location.set)
        {
            night.source = "manual";
            night.latitude = cfg.location.latitude;
            night.longitude = cfg.location.longitude;
        }
        if (now < 1704067200 || night.source == nullptr)
            return night; // no clock or no location: unknown
        night.known = true;
        night.sunAltitudeDeg = Astro::sunElevationDeg(static_cast<int64_t>(now), night.latitude, night.longitude);
        night.isNight = night.sunAltitudeDeg < cfg.alerts.nightSunAltitudeDeg;
        return night;
    }

    void WebServer::processAlerts(const SafetyStatus &status)
    {
        const Config &cfg = getConfigCallback();
        if (cfg.alerts.mqttEnabled)
            publishMqttSafety(status);

        const SensorSnapshot snapshot = getSensorSnapshot();
        const Alpaca::ObservingConditionsSnapshot obs = buildAlpacaObservingConditionsSnapshot();

        Alerts::AlertInputs in;
        in.nowSeconds = millis() / 1000;
        in.safetyKnown = status.evaluatedAtMs != 0;
        in.isSafe = status.isSafe;
        in.unsafeReasons = status.reasons;

        in.rainEnabled = cfg.rain.enabled;
        in.raining = snapshot.rg15.isRaining || snapshot.rg15.rainLatched;
        in.rainRateMmPerHour = obs.rainRateMmPerHour;
        in.lensFault = snapshot.rg15.lensBad;

        in.sensors[0] = {"TSL2591 light", true, obs.skyLight.valid};
        in.sensors[1] = {"MLX90614 IR", true, obs.irSky.valid};
        in.sensors[2] = {"BME280 environment", true, obs.environment.valid};
        in.sensors[3] = {"RG-15 rain", cfg.rain.enabled, obs.rain.valid};
        in.sensors[4] = {"Wind", obs.wind.present, obs.wind.valid};

        in.environmentValid = obs.environment.valid;
        in.temperatureC = obs.temperatureC;
        in.dewpointC = obs.dewpointC;
        in.skyValid = obs.irSky.valid;
        in.cloudCoverPercent = obs.cloudCoverPercent;
        const NightState night = computeNight(snapshot, cfg);
        in.nightKnown = night.known;
        in.isNight = night.isNight;

        Alerts::AlertRules rules;
        const AlertsConfig &a = cfg.alerts;
        rules.onSafetyChange = a.unsafe.level || a.safe.level;
        rules.onRain = a.rainStarted.level || a.rainStopped.level;
        rules.onSensorFault = a.sensorFault.level || a.sensorRecovered.level;
        rules.onDewRisk = a.dewRisk.level != 0;
        rules.dewRiskMarginC = a.dewRiskMarginC;
        rules.onClearSky = a.clearSky.level != 0;
        rules.clearSkyCloudPercent = a.clearSkyCloudPercent;
        rules.onCloudedOver = a.cloudedOver.level != 0;
        rules.cloudedOverCloudPercent = cfg.alerts.cloudedOverCloudPercent;
        rules.skyNightOnly = cfg.alerts.skyNightOnly;
        rules.cooldownSeconds = cfg.alerts.cooldownSeconds;

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
        // off. Each alert gets its configured level and sound; push channels
        // need the master switch, paired phones only need Bluetooth.
        const time_t wallClock = time(nullptr);
        const uint32_t epoch = wallClock >= 1704067200 ? static_cast<uint32_t>(wallClock) : 0;
        for (Alerts::Alert alert : alertEngine.update(in, rules))
        {
            const AlertsConfig::EventSetting *setting = eventSettingFor(a, alert.type);
            if (setting == nullptr || setting->level == 0)
                continue;
            alert.level = static_cast<Alerts::AlertLevel>(setting->level);
            alert.sound = setting->sound;

            if (cfg.alerts.enabled)
            {
                alertDispatcher->dispatch(alert, cfg.alerts, cfg.deviceName);
                ble.publishAlert(alert);
            }
            if (alert.level == Alerts::AlertLevel::Wake)
                ble.raiseAlarm(status.reasonFlags | (alert.type == Alerts::AlertType::RainStarted ? Alpaca::UNSAFE_RAIN : 0u) |
                                   (alert.type == Alerts::AlertType::SensorFault || alert.type == Alerts::AlertType::LensFault ? Alpaca::UNSAFE_SENSOR_FAULT : 0u),
                               epoch);
            else
                ble.raiseInfo(status.reasonFlags, epoch);
        }

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
            alertDispatcher->dispatch(ack, cfg.alerts, cfg.deviceName);
        }
    }

    const AlertsConfig::EventSetting *WebServer::eventSettingFor(const AlertsConfig &a, Alerts::AlertType type)
    {
        switch (type)
        {
        case Alerts::AlertType::Unsafe:
            return &a.unsafe;
        case Alerts::AlertType::Safe:
            return &a.safe;
        case Alerts::AlertType::RainStarted:
            return &a.rainStarted;
        case Alerts::AlertType::RainStopped:
            return &a.rainStopped;
        case Alerts::AlertType::SensorFault:
        case Alerts::AlertType::LensFault:
            return &a.sensorFault;
        case Alerts::AlertType::SensorRecovered:
            return &a.sensorRecovered;
        case Alerts::AlertType::DewRisk:
            return &a.dewRisk;
        case Alerts::AlertType::ClearSky:
            return &a.clearSky;
        case Alerts::AlertType::CloudedOver:
            return &a.cloudedOver;
        default:
            return nullptr;
        }
    }

    void WebServer::publishMqttSafety(const SafetyStatus &status)
    {
        const uint32_t now = millis();
        const bool changed = !mqttSafetyPublished || status.isSafe != mqttLastPublishedSafe;
        if (!changed && now - mqttSafetyPublishedAt < MQTT_SAFETY_REPUBLISH_MS)
            return;

        DynamicJsonDocument doc(1024);
        doc["isSafe"] = status.isSafe;
        JsonArray reasons = doc.createNestedArray("reasons");
        for (const std::string &reason : status.reasons)
            reasons.add(reason);
        std::string payload;
        serializeJson(doc, payload);

        if (mqttClient != nullptr && mqttClient->publishSubtopic("safety", payload, true))
        {
            mqttSafetyPublished = true;
            mqttLastPublishedSafe = status.isSafe;
            mqttSafetyPublishedAt = now;
        }
    }

    void WebServer::setupAlertRoutes()
    {
        server.on("/api/alerts/test", HTTP_POST, [this](AsyncWebServerRequest *request)
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
            else {
                request->send(400, "application/json", createErrorJson("Unknown channel (expected mqtt, pushover, ntfy, webhook or all)").c_str());
                return;
            }

            const uint8_t enabledMask =
                (alerts.mqttEnabled ? alertChannelBit(AlertChannel::Mqtt) : 0) |
                (alerts.pushoverEnabled ? alertChannelBit(AlertChannel::Pushover) : 0) |
                (alerts.ntfyEnabled ? alertChannelBit(AlertChannel::Ntfy) : 0) |
                (alerts.webhookEnabled ? alertChannelBit(AlertChannel::Webhook) : 0);
            if ((mask & enabledMask) == 0) {
                request->send(400, "application/json", createErrorJson("That channel isn't enabled - enable it and save settings first").c_str());
                return;
            }

            pendingAlertTestMask.fetch_or(mask & enabledMask);
            request->send(202, "application/json", "{\"success\":true,\"message\":\"Test notification queued\"}"); });

        server.on("/api/ble/ack", HTTP_POST, [this](AsyncWebServerRequest *request)
                  {
            if (!requireAuth(request))
                return;
            if (!ble.alarmStatus().alarmActive) {
                request->send(409, "application/json", createErrorJson("No phone alarm is active").c_str());
                return;
            }
            ble.requestLocalAck();
            request->send(202, "application/json", "{\"success\":true}"); });

        server.on("/api/ble/forget-bonds", HTTP_POST, [this](AsyncWebServerRequest *request)
                  {
            if (!requireAuth(request))
                return;
            if (!ble.isActive()) {
                request->send(409, "application/json", createErrorJson("Bluetooth is off").c_str());
                return;
            }
            ble.requestForgetBonds();
            request->send(202, "application/json", "{\"success\":true}"); });

        server.on("/api/alerts/recent", HTTP_GET, [this](AsyncWebServerRequest *request)
                  {
            const std::vector<AlertRecord> records = alertDispatcher->recent();
            DynamicJsonDocument doc(8192);
            doc["enabled"] = getConfigCallback().alerts.enabled;
            JsonArray arr = doc.createNestedArray("alerts");
            const uint32_t nowSeconds = millis() / 1000;
            // Newest first
            for (auto it = records.rbegin(); it != records.rend(); ++it) {
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
                for (size_t i = 0; i < ALERT_CHANNEL_COUNT; ++i) {
                    if (it->status[i] == DeliveryStatus::NotSent)
                        continue;
                    JsonObject ch = channels.createNestedObject(alertChannelName(static_cast<AlertChannel>(i)));
                    ch["status"] = deliveryStatusName(it->status[i]);
                    ch["detail"] = it->detail[i];
                }
            }
            std::string json;
            serializeJson(doc, json);
            request->send(200, "application/json", json.c_str()); });
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
        const SensorSnapshot snapshot = getSensorSnapshot();
        const uint32_t now = millis();
        const Config &cfg = getConfigCallback();
        const uint32_t staleAfter = cfg.sensor.readIntervalMs + SENSOR_STALE_GRACE_MS;

        auto sourceState = [now, staleAfter](bool present, SensorStatus status, uint32_t lastUpdate)
        {
            Alpaca::SourceState state;
            state.present = present;
            state.ageSeconds = ageMs(now, lastUpdate) / 1000.0;
            state.valid = present && status == SensorStatus::OK && lastUpdate != 0 && ageMs(now, lastUpdate) <= staleAfter;
            return state;
        };

        Alpaca::ObservingConditionsSnapshot snap;
        snap.skyLight = sourceState(true, snapshot.tsl.status, snapshot.tslLastUpdate);
        snap.irSky = sourceState(true, snapshot.mlx.status, snapshot.mlxLastUpdate);
        snap.environment = sourceState(true, snapshot.bme.status, snapshot.bmeLastUpdate);

        // The RG-15 polls on its own interval and tracks its own staleness.
        snap.rain.present = cfg.rain.enabled;
        snap.rain.ageSeconds = ageMs(now, snapshot.rg15.timestamp) / 1000.0;
        snap.rain.valid = cfg.rain.enabled && snapshot.rg15.online && !snapshot.rg15.stale &&
                          snapshot.rg15.status == SensorStatus::OK && snapshot.rg15.timestamp != 0;

        // The anemometer samples every second; allow a few missed ticks.
        constexpr uint32_t WIND_STALE_MS = 5000;
        const bool windFresh = snapshot.wind.status == SensorStatus::OK && snapshot.wind.timestamp != 0 &&
                               ageMs(now, snapshot.wind.timestamp) <= WIND_STALE_MS;
        snap.wind.present = cfg.wind.enabled;
        snap.wind.valid = cfg.wind.enabled && windFresh;
        snap.wind.ageSeconds = ageMs(now, snapshot.wind.timestamp) / 1000.0;
        snap.windVane.present = cfg.wind.enabled && cfg.wind.directionEnabled;
        // Calm is valid (direction reported as 0); only a vane fault isn't.
        snap.windVane.valid = snap.windVane.present && windFresh && !snapshot.wind.vaneFault;
        snap.windVane.ageSeconds = snap.wind.ageSeconds;
        snap.windSpeedMs = snapshot.wind.speedMs;
        snap.windGustMs = snapshot.wind.gustMs;
        snap.windDirectionDeg = snapshot.wind.directionValid ? snapshot.wind.directionDeg : 0.0f;

        // Cloud cover may use a nominal humidity when the BME280 is down -
        // it only shifts the correction term - but Alpaca's Humidity
        // property must never report that made-up value.
        const float humidityForCloud = snap.environment.valid ? snapshot.bme.humidity : 53.0f;
        CloudMetrics cloudMetrics = CloudDetection::calculate(
            snapshot.mlx.objectTemp,
            snapshot.mlx.ambientTemp,
            humidityForCloud,
            cfg.cloudDetection.clearSkyThreshold,
            cfg.cloudDetection.cloudyThreshold,
            cfg.cloudDetection.humidityCorrection);
        snap.cloudCoverPercent = cloudMetrics.cloudCoverPercent;

        SkyQualityMetrics sqm = SkyQuality::calculate(snapshot.tsl.lux);
        snap.skyQualityMagArcsec2 = sqm.sqm;
        snap.skyBrightnessLux = snapshot.tsl.lux;
        snap.skyTemperatureC = snapshot.mlx.objectTemp;
        snap.temperatureC = snapshot.bme.temperature;
        snap.humidityPercent = snapshot.bme.humidity;
        snap.dewpointC = snapshot.bme.dewpoint;
        snap.pressureHPa = snapshot.bme.pressure;
        snap.rainRateMmPerHour = Alpaca::rainRateToMmPerHour(snapshot.rg15.rInt, snapshot.rg15.imperial);

        return snap;
    }

    namespace
    {
        constexpr size_t ALPACA_SAFETY_MONITOR = 0;
        constexpr size_t ALPACA_OBSERVING_CONDITIONS = 1;

        // Interface versions advertised via InterfaceVersion. These are the
        // ASCOM Platform 7 versions, which add Connect/Disconnect/Connecting/
        // DeviceState to every device.
        constexpr int SAFETY_MONITOR_INTERFACE_VERSION = 3;
        constexpr int OBSERVING_CONDITIONS_INTERFACE_VERSION = 2;

        struct ObservingPropertyName
        {
            const char *route;      // lowercase Alpaca method name
            const char *stateName;  // PascalCase name used in DeviceState
        };

        constexpr ObservingPropertyName OBSERVING_PROPERTIES[] = {
            {"cloudcover", "CloudCover"},
            {"dewpoint", "DewPoint"},
            {"humidity", "Humidity"},
            {"pressure", "Pressure"},
            {"rainrate", "RainRate"},
            {"skybrightness", "SkyBrightness"},
            {"skyquality", "SkyQuality"},
            {"skytemperature", "SkyTemperature"},
            {"starfwhm", "StarFWHM"},
            {"temperature", "Temperature"},
            {"winddirection", "WindDirection"},
            {"windgust", "WindGust"},
            {"windspeed", "WindSpeed"}};

        // ISO 8601 UTC timestamp for DeviceState, or empty if the clock has
        // never been set (NTP/GPS) - a 1970 timestamp would be worse than none.
        std::string alpacaTimestampNow()
        {
            const time_t now = time(nullptr);
            if (now < 1704067200)
                return "";
            struct tm utc;
            gmtime_r(&now, &utc);
            char buffer[32];
            strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
            return buffer;
        }

        std::string buildAlpacaResponseDeviceState(AsyncWebServerRequest *request, const std::function<void(JsonArray &)> &fill, uint32_t &txnCounter)
        {
            DynamicJsonDocument doc(1536);
            JsonArray arr = doc.createNestedArray("Value");
            fill(arr);
            const std::string timestamp = alpacaTimestampNow();
            if (!timestamp.empty())
            {
                JsonObject item = arr.createNestedObject();
                item["Name"] = "TimeStamp";
                item["Value"] = timestamp;
            }
            doc["ClientTransactionID"] = getAlpacaClientTransactionId(request);
            doc["ServerTransactionID"] = ++txnCounter;
            doc["ErrorNumber"] = 0;
            doc["ErrorMessage"] = "";
            std::string json;
            serializeJson(doc, json);
            return json;
        }

        void sendAlpacaBadRequest(AsyncWebServerRequest *request, const char *message)
        {
            request->send(400, "text/plain", message);
        }
    }

    void WebServer::setupAlpacaRoutes()
    {
        // --- Setup pages ---
        // NINA's (and other clients') "Setup" button opens
        // /setup/v1/<devicetype>/<n>/setup in a browser. All of the device's
        // settings live on the SPA's Settings page, so send every setup URL
        // there. server.on() also matches "/setup/..." as a prefix in this
        // ESPAsyncWebServer version, so this single route covers them all.
        server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request)
                  { request->redirect("/settings?section=alpaca"); });

        // --- Management API ---
        server.on("/management/apiversions", HTTP_GET, [this](AsyncWebServerRequest *request)
                  { request->send(200, "application/json", buildAlpacaResponseIntArray(request, {1}, alpacaServerTransactionId).c_str()); });

        server.on("/management/v1/description", HTTP_GET, [this](AsyncWebServerRequest *request)
                  {
            DynamicJsonDocument doc(384);
            JsonObject value = doc.createNestedObject("Value");
            value["ServerName"] = FIRMWARE_NAME;
            value["Manufacturer"] = "SQMeter";
            value["ManufacturerVersion"] = FIRMWARE_VERSION;
            value["Location"] = getConfigCallback().deviceName;
            doc["ClientTransactionID"] = getAlpacaClientTransactionId(request);
            doc["ServerTransactionID"] = ++alpacaServerTransactionId;
            doc["ErrorNumber"] = 0;
            doc["ErrorMessage"] = "";
            std::string json;
            serializeJson(doc, json);
            request->send(200, "application/json", json.c_str()); });

        server.on("/management/v1/configureddevices", HTTP_GET, [this](AsyncWebServerRequest *request)
                  {
            const uint64_t mac = ESP.getEfuseMac();
            DynamicJsonDocument doc(768);
            JsonArray value = doc.createNestedArray("Value");
            if (getConfigCallback().alpaca.enabled) {
                JsonObject safety = value.createNestedObject();
                safety["DeviceName"] = "SQMeter SafetyMonitor";
                safety["DeviceType"] = "SafetyMonitor";
                safety["DeviceNumber"] = 0;
                safety["UniqueID"] = Alpaca::buildUniqueId(mac, "safetymonitor", 0);

                JsonObject obsCond = value.createNestedObject();
                obsCond["DeviceName"] = "SQMeter ObservingConditions";
                obsCond["DeviceType"] = "ObservingConditions";
                obsCond["DeviceNumber"] = 0;
                obsCond["UniqueID"] = Alpaca::buildUniqueId(mac, "observingconditions", 0);
            }
            doc["ClientTransactionID"] = getAlpacaClientTransactionId(request);
            doc["ServerTransactionID"] = ++alpacaServerTransactionId;
            doc["ErrorNumber"] = 0;
            doc["ErrorMessage"] = "";
            std::string json;
            serializeJson(doc, json);
            request->send(200, "application/json", json.c_str()); });

        // --- Device API ---
        // One handler for every /api/v1/<devicetype>/<n>/<method> route.
        // Registering ~50 routes separately cost ~10 KB of heap (each handler
        // holds its own URI string and std::function); dispatching here costs
        // a few string compares per request.
        server.on("/api/v1", HTTP_ANY, [this](AsyncWebServerRequest *request)
                  { handleAlpacaDeviceRequest(request); });
    }

    void WebServer::handleAlpacaDeviceRequest(AsyncWebServerRequest *request)
    {
        static const char *SAFETY_NAME = "SQMeter SafetyMonitor";
        static const char *SAFETY_DESCRIPTION =
            "Reports observatory safety from rain (RG-15), wind (optional anemometer), cloud cover, sky brightness, humidity and dew-point margin measured by the onboard SQMeter sensors.";
        static const char *CONDITIONS_NAME = "SQMeter ObservingConditions";
        static const char *CONDITIONS_DESCRIPTION =
            "Reports sky quality, sky brightness, cloud cover, sky temperature, temperature, humidity, dew point, pressure, and - when fitted - rain rate (RG-15) and wind speed, gust and direction (anemometer/vane).";
        static const char *ALPACA_DISABLED_MESSAGE = "Alpaca support is disabled in device settings";

        // /api/v1/<type>/<number>/<method>
        const String &url = request->url();
        const int typeStart = 8; // strlen("/api/v1/")
        const int numberStart = url.indexOf('/', typeStart) + 1;
        const int methodStart = numberStart > 0 ? url.indexOf('/', numberStart) + 1 : 0;
        if (numberStart <= 0 || methodStart <= 0 || url.indexOf('/', methodStart) >= 0)
        {
            sendAlpacaBadRequest(request, "Invalid Alpaca device type, device number, method or HTTP verb");
            return;
        }
        const String type = url.substring(typeStart, numberStart - 1);
        const String number = url.substring(numberStart, methodStart - 1);
        const String method = url.substring(methodStart);
        const bool isSafetyMonitor = type == "safetymonitor";
        if (number != "0" || (!isSafetyMonitor && type != "observingconditions") || method.isEmpty())
        {
            sendAlpacaBadRequest(request, "Invalid Alpaca device type, device number, method or HTTP verb");
            return;
        }

        const bool get = request->method() == HTTP_GET;
        const bool put = request->method() == HTTP_PUT;
        const size_t deviceIndex = isSafetyMonitor ? ALPACA_SAFETY_MONITOR : ALPACA_OBSERVING_CONDITIONS;
        const bool enabled = getConfigCallback().alpaca.enabled;
        auto reply = [request](const std::string &json)
        { request->send(200, "application/json", json.c_str()); };

        // --- Common ASCOM device API ---
        if (method == "connected" && get)
            return reply(buildAlpacaResponseBool(request, enabled && alpacaConnected[deviceIndex], 0, ""));
        if (method == "connected" && put)
        {
            const AsyncWebParameter *param = findAlpacaParam(request, "Connected");
            bool connected = false;
            if (param == nullptr || !Alpaca::parseAlpacaBool(param->value().c_str(), connected))
                return sendAlpacaBadRequest(request, "Missing or invalid Connected parameter (expected true or false)");
            if (connected && !enabled)
                return reply(buildAlpacaResponseVoid(request, Alpaca::ALPACA_ERR_NOT_CONNECTED, ALPACA_DISABLED_MESSAGE));
            alpacaConnected[deviceIndex] = connected;
            return reply(buildAlpacaResponseVoid(request, 0, ""));
        }
        // Platform 7 asynchronous connect: connecting completes instantly,
        // so Connecting is always false.
        if (method == "connect" && put)
        {
            if (!enabled)
                return reply(buildAlpacaResponseVoid(request, Alpaca::ALPACA_ERR_NOT_CONNECTED, ALPACA_DISABLED_MESSAGE));
            alpacaConnected[deviceIndex] = true;
            return reply(buildAlpacaResponseVoid(request, 0, ""));
        }
        if (method == "disconnect" && put)
        {
            alpacaConnected[deviceIndex] = false;
            return reply(buildAlpacaResponseVoid(request, 0, ""));
        }
        if (method == "connecting" && get)
            return reply(buildAlpacaResponseBool(request, false, 0, ""));
        if (method == "name" && get)
            return reply(buildAlpacaResponseString(request, isSafetyMonitor ? SAFETY_NAME : CONDITIONS_NAME, alpacaServerTransactionId));
        if (method == "description" && get)
            return reply(buildAlpacaResponseString(request, isSafetyMonitor ? SAFETY_DESCRIPTION : CONDITIONS_DESCRIPTION, alpacaServerTransactionId));
        if (method == "driverinfo" && get)
            return reply(buildAlpacaResponseString(request, "Native ESP32 firmware, no external bridge - https://github.com/DeanJ87/SQMeter", alpacaServerTransactionId));
        if (method == "driverversion" && get)
            return reply(buildAlpacaResponseString(request, FIRMWARE_VERSION, alpacaServerTransactionId));
        if (method == "interfaceversion" && get)
            return reply(buildAlpacaResponseInt(request, isSafetyMonitor ? SAFETY_MONITOR_INTERFACE_VERSION : OBSERVING_CONDITIONS_INTERFACE_VERSION,
                                                alpacaServerTransactionId));
        if (method == "supportedactions" && get)
            return reply(buildAlpacaResponseStringArray(request, {}, alpacaServerTransactionId));
        // No custom actions or raw commands are supported.
        if (put && (method == "action" || method == "commandblind" || method == "commandbool" || method == "commandstring"))
            return reply(buildAlpacaResponseVoid(request, Alpaca::ALPACA_ERR_NOT_IMPLEMENTED, "Custom actions and commands are not supported"));

        // --- SafetyMonitor ---
        if (isSafetyMonitor)
        {
            if (method == "issafe" && get)
            {
                if (!enabled)
                    return reply(buildAlpacaResponseBool(request, false, Alpaca::ALPACA_ERR_NOT_CONNECTED, ALPACA_DISABLED_MESSAGE));
                return reply(buildAlpacaResponseBool(request, getSafetyStatus().isSafe, 0, ""));
            }
            if (method == "devicestate" && get)
            {
                const bool isSafe = enabled && getSafetyStatus().isSafe;
                return reply(buildAlpacaResponseDeviceState(request, [isSafe](JsonArray &arr)
                                                            {
                    JsonObject item = arr.createNestedObject();
                    item["Name"] = "IsSafe";
                    item["Value"] = isSafe; }, alpacaServerTransactionId));
            }
            return sendAlpacaBadRequest(request, "Invalid Alpaca device type, device number, method or HTTP verb");
        }

        // --- ObservingConditions ---
        if (method == "averageperiod" && get)
            return reply(buildAlpacaResponseDouble(request, 0.0, 0, ""));
        if (method == "averageperiod" && put)
        {
            const AsyncWebParameter *param = findAlpacaParam(request, "AveragePeriod");
            double hours = 0.0;
            if (param == nullptr || !Alpaca::parseAlpacaDouble(param->value().c_str(), hours))
                return sendAlpacaBadRequest(request, "Missing or invalid AveragePeriod parameter");
            Alpaca::PropertyResult result = Alpaca::validateAveragePeriod(hours);
            return reply(buildAlpacaResponseVoid(request, result.ok ? 0 : result.errorNumber, result.errorMessage));
        }
        // Readings refresh every sensor cycle already; nothing to force.
        if (method == "refresh" && put)
            return reply(buildAlpacaResponseVoid(request, 0, ""));
        if ((method == "sensordescription" || method == "timesincelastupdate") && get)
        {
            const AsyncWebParameter *param = findAlpacaParam(request, "SensorName");
            if (param == nullptr)
                return sendAlpacaBadRequest(request, "Missing SensorName parameter");
            if (method == "sensordescription")
            {
                Alpaca::StringResult result = Alpaca::getSensorDescription(param->value().c_str(), buildAlpacaObservingConditionsSnapshot());
                return reply(buildAlpacaResponseStringWithError(request, result.value, result.ok ? 0 : result.errorNumber, result.errorMessage,
                                                               alpacaServerTransactionId));
            }
            Alpaca::PropertyResult result = Alpaca::getTimeSinceLastUpdate(param->value().c_str(), buildAlpacaObservingConditionsSnapshot());
            return reply(buildAlpacaResponseDouble(request, result.ok ? result.value : 0.0, result.ok ? 0 : result.errorNumber, result.errorMessage));
        }
        if (method == "devicestate" && get)
        {
            const Alpaca::ObservingConditionsSnapshot snapshot = buildAlpacaObservingConditionsSnapshot();
            return reply(buildAlpacaResponseDeviceState(request, [enabled, &snapshot](JsonArray &arr)
                                                        {
                if (!enabled)
                    return;
                // DeviceState lists only properties that currently have a value.
                for (const ObservingPropertyName &property : OBSERVING_PROPERTIES) {
                    Alpaca::PropertyResult result = Alpaca::getObservingConditionsProperty(property.route, snapshot);
                    if (!result.ok)
                        continue;
                    JsonObject item = arr.createNestedObject();
                    item["Name"] = property.stateName;
                    item["Value"] = result.value;
                } }, alpacaServerTransactionId));
        }
        if (get)
        {
            for (const ObservingPropertyName &property : OBSERVING_PROPERTIES)
            {
                if (method != property.route)
                    continue;
                if (!enabled)
                    return reply(buildAlpacaResponseDouble(request, 0, Alpaca::ALPACA_ERR_NOT_CONNECTED, ALPACA_DISABLED_MESSAGE));
                Alpaca::PropertyResult result = Alpaca::getObservingConditionsProperty(property.route, buildAlpacaObservingConditionsSnapshot());
                return reply(result.ok ? buildAlpacaResponseDouble(request, result.value, 0, "")
                                       : buildAlpacaResponseDouble(request, 0, result.errorNumber, result.errorMessage));
            }
        }
        sendAlpacaBadRequest(request, "Invalid Alpaca device type, device number, method or HTTP verb");
    }

    void WebServer::handleAlpacaDiscovery()
    {
        if (!alpacaDiscoveryStarted)
            return;

        int packetSize = alpacaDiscoveryUdp.parsePacket();
        if (packetSize <= 0)
            return;

        uint8_t buf[64];
        int len = alpacaDiscoveryUdp.read(buf, sizeof(buf));
        if (len > 0 && Alpaca::isValidDiscoveryRequest(buf, static_cast<size_t>(len)))
        {
            std::string response = Alpaca::buildDiscoveryResponse(PORT);
            alpacaDiscoveryUdp.beginPacket(alpacaDiscoveryUdp.remoteIP(), alpacaDiscoveryUdp.remotePort());
            alpacaDiscoveryUdp.write(reinterpret_cast<const uint8_t *>(response.data()), response.size());
            alpacaDiscoveryUdp.endPacket();
        }
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

        const uint32_t startedAt = millis();
        const bool ok = rg15Sensor.testCommunication();
        const RG15Reading reading = rg15Sensor.copyReading();
        const RG15Diagnostics diagnostics = rg15Sensor.getDiagnostics();
        const uint32_t now = millis();

        StaticJsonDocument<1024> response;
        response["ok"] = ok && reading.status == SensorStatus::OK;
        response["command"] = diagnostics.lastCommand ? diagnostics.lastCommand->c_str() : "R";
        response["bytes_written"] = diagnostics.lastBytesWritten;
        response["elapsed_ms"] = now - startedAt;
        if (diagnostics.lastRawResponse)
            response["raw_response"] = diagnostics.lastRawResponse->c_str();
        else
            response["raw_response"] = nullptr;
        if (diagnostics.lastAck)
            response["ack"] = diagnostics.lastAck->c_str();
        else
            response["ack"] = nullptr;
        response["acknowledged"] = diagnostics.lastAck.has_value();
        response["parsed"] = reading.status == SensorStatus::OK;
        response["online"] = reading.online;
        response["stale"] = reading.stale;
        if (diagnostics.lastSuccessfulReadMs != 0)
            response["last_successful_read_age_ms"] = static_cast<uint32_t>(now - diagnostics.lastSuccessfulReadMs);
        else
            response["last_successful_read_age_ms"] = nullptr;
        if (diagnostics.lastError)
            response["error"] = diagnostics.lastError->c_str();
        else
            response["error"] = nullptr;
        response["hint"] = "Check RG-15 Serial OUT -> ESP32 RX, Serial IN -> ESP32 TX, common ground, baud rate, and voltage level.";

        String responseStr;
        serializeJson(response, responseStr);
        request->send(ok && reading.status == SensorStatus::OK ? 200 : 400, "application/json", responseStr.c_str());
    }

    void WebServer::handleTSL2591DarkCalibration(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;

        const TSL2591Diagnostics diagnostics = tslSensor.getDiagnostics();
        if (diagnostics.sampleCount == 0)
        {
            request->send(400, "application/json", createErrorJson("No TSL2591 samples available for dark calibration").c_str());
            return;
        }

        Config updated = getConfigCallback();
        updated.skyCalibration.darkVisibleOffset = diagnostics.rollingVisible;
        updated.skyCalibration.darkFullOffset = 0.0F;
        updated.skyCalibration.darkIrOffset = 0.0F;
        updated.skyCalibration.darkSampleCount = diagnostics.sampleCount;
        const time_t epochSeconds = time(nullptr);
        updated.skyCalibration.darkCalibratedAt = epochSeconds >= 1704067200 ? static_cast<int64_t>(epochSeconds) : static_cast<int64_t>(millis());

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
        StaticJsonDocument<256> response;
        response["ok"] = ok;
        response["command"] = "O";
        response["message"] = ok ? "RG-15 total accumulation reset command sent" : "RG-15 total accumulation reset failed";

        String responseStr;
        serializeJson(response, responseStr);
        request->send(ok ? 200 : 400, "application/json", responseStr.c_str());
    }

    void WebServer::handleRG15Reboot(AsyncWebServerRequest *request)
    {
        if (!requireAuth(request))
            return;

        const bool ok = rg15Sensor.rebootSensor();
        StaticJsonDocument<256> response;
        response["ok"] = ok;
        response["command"] = "K";
        response["message"] = ok ? "RG-15 reboot command sent" : "RG-15 reboot command failed";

        String responseStr;
        serializeJson(response, responseStr);
        request->send(ok ? 200 : 400, "application/json", responseStr.c_str());
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

        Logger::info(TAG, "Testing MQTT connection to %s:%d", broker, port);

        WiFiClient testWifiClient;
        PubSubClient testMqtt(broker, port, testWifiClient);

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
            response["success"] = false;

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
        request->send(connected ? 200 : 400, "application/json", responseStr.c_str());
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
        AsyncWebSocket *server,
        AsyncWebSocketClient *client,
        AwsEventType type,
        void *arg,
        uint8_t *data,
        size_t len)
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
        AsyncWebSocket *server,
        AsyncWebSocketClient *client,
        AwsEventType type,
        void *arg,
        uint8_t *data,
        size_t len)
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

    WebServer::SensorSnapshot WebServer::getSensorSnapshot() const
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

    void WebServer::appendSafetyStatus(JsonObject target) const
    {
        const SafetyStatus status = getSafetyStatus();
        const uint32_t now = millis();
        target["isSafe"] = status.isSafe;
        target["rawSafe"] = status.rawSafe;
        target["alpacaEnabled"] = getConfigCallback().alpaca.enabled;
        target["reasonFlags"] = status.reasonFlags;
        JsonArray reasons = target.createNestedArray("reasons");
        for (const std::string &reason : status.reasons)
            reasons.add(reason);
        target["secondsUntilSafe"] = status.secondsUntilSafe;
        target["evaluatedAgeMs"] = ageMs(now, status.evaluatedAtMs);
        target["changedAgeMs"] = ageMs(now, status.changedAtMs);
    }

    std::string WebServer::createSensorDataJson() const
    {
        DynamicJsonDocument doc(6144);
        const SensorSnapshot snapshot = getSensorSnapshot();
        const uint32_t now = millis();
        const uint32_t dataAge = ageMs(now, snapshot.dataTimestamp);
        const uint32_t staleAfter = getConfigCallback().sensor.readIntervalMs + SENSOR_STALE_GRACE_MS;

        doc["dataTimestamp"] = snapshot.dataTimestamp;
        doc["dataAgeMs"] = dataAge;
        doc["dataStale"] = snapshot.dataTimestamp == 0 || dataAge > staleAfter;

        // Light sensor data (TSL2591)
        const auto &tslReading = snapshot.tsl;
        JsonObject lightSensor = doc.createNestedObject("lightSensor");
        lightSensor["lux"] = tslReading.lux;
        lightSensor["rawLux"] = tslReading.rawLux;
        lightSensor["visible"] = tslReading.visible;
        lightSensor["infrared"] = tslReading.infrared;
        lightSensor["full"] = tslReading.full;
        lightSensor["status"] = static_cast<int>(tslReading.status);
        lightSensor["timestamp"] = tslReading.timestamp;
        lightSensor["ageMs"] = ageMs(now, tslReading.timestamp);
        lightSensor["gainName"] = snapshot.tslDiagnostics.gainName;
        lightSensor["gainFactor"] = snapshot.tslDiagnostics.gainFactor;
        lightSensor["integrationMs"] = snapshot.tslDiagnostics.integrationMs;
        lightSensor["averagingWindowSeconds"] = snapshot.tslDiagnostics.averagingWindowSeconds;
        lightSensor["calibrated"] = snapshot.tslDiagnostics.calibrated;
        lightSensor["saturated"] = snapshot.tslDiagnostics.saturated;

        // Sky quality calculations
        SkyQualityMetrics sqm = SkyQuality::calculate(tslReading.lux);
        JsonObject sky = doc.createNestedObject("skyQuality");
        sky["sqm"] = sqm.sqm;
        sky["rawSqm"] = tslReading.rawSqm;
        sky["calibratedSqm"] = tslReading.calibratedSqm;
        sky["nelm"] = sqm.nelm;
        sky["bortle"] = sqm.bortle;
        sky["description"] = SkyQuality::getBortleDescription(sqm.bortle);
        sky["nightMode"] = snapshot.tslDiagnostics.nightMode;

        JsonObject diagnostics = doc.createNestedObject("lightDiagnostics");
        diagnostics["rollingVisible"] = snapshot.tslDiagnostics.rollingVisible;
        diagnostics["correctedVisible"] = snapshot.tslDiagnostics.correctedVisible;
        diagnostics["darkVisibleOffset"] = snapshot.tslDiagnostics.darkVisibleOffset;
        diagnostics["sampleCount"] = snapshot.tslDiagnostics.sampleCount;
        diagnostics["rejectedSamples"] = snapshot.tslDiagnostics.rejectedSamples;
        diagnostics["consecutiveSaturatedSamples"] = snapshot.tslDiagnostics.consecutiveSaturatedSamples;
        diagnostics["consecutiveLowSamples"] = snapshot.tslDiagnostics.consecutiveLowSamples;

        // Environmental sensor data (BME280)
        const auto &bmeReading = snapshot.bme;
        JsonObject environment = doc.createNestedObject("environment");
        environment["temperature"] = bmeReading.temperature;
        environment["humidity"] = bmeReading.humidity;
        environment["pressure"] = bmeReading.pressure;
        environment["dewpoint"] = bmeReading.dewpoint;
        environment["status"] = static_cast<int>(bmeReading.status);
        environment["timestamp"] = bmeReading.timestamp;
        environment["ageMs"] = ageMs(now, bmeReading.timestamp);

        // IR temperature sensor data (MLX90614)
        const auto &mlxReading = snapshot.mlx;
        JsonObject irTemperature = doc.createNestedObject("irTemperature");
        irTemperature["objectTemp"] = mlxReading.objectTemp;
        irTemperature["ambientTemp"] = mlxReading.ambientTemp;
        irTemperature["status"] = static_cast<int>(mlxReading.status);
        irTemperature["timestamp"] = mlxReading.timestamp;
        irTemperature["ageMs"] = ageMs(now, mlxReading.timestamp);

        // Cloud detection from IR temperature sensor
        // Use BME280 humidity if available, otherwise default to 53%
        bool usingHumidityFallback = bmeReading.status != SensorStatus::OK;
        float humidity = usingHumidityFallback ? 53.0f : bmeReading.humidity;
        const Config &cfg = getConfigCallback();
        CloudMetrics cloudMetrics = CloudDetection::calculate(
            mlxReading.objectTemp,
            mlxReading.ambientTemp,
            humidity,
            cfg.cloudDetection.clearSkyThreshold,
            cfg.cloudDetection.cloudyThreshold,
            cfg.cloudDetection.humidityCorrection);

        JsonObject cloud = doc.createNestedObject("cloudConditions");
        cloud["temperatureDelta"] = cloudMetrics.temperatureDelta;
        cloud["correctedDelta"] = cloudMetrics.correctedDelta;
        cloud["cloudCoverPercent"] = cloudMetrics.cloudCoverPercent;
        cloud["condition"] = static_cast<int>(cloudMetrics.condition);
        cloud["description"] = cloudMetrics.description;
        cloud["humidityUsed"] = humidity;
        cloud["humiditySource"] = usingHumidityFallback ? "default" : "bme280";
        cloud["bme280Available"] = !usingHumidityFallback;

        // GPS data (if initialized)
        if (snapshot.gpsInitialized)
        {
            const GPSReading &gpsReading = snapshot.gps;
            JsonObject gps = doc.createNestedObject("gps");
            gps["hasFix"] = gpsReading.hasFix;
            gps["satellites"] = gpsReading.satellites;
            gps["latitude"] = gpsReading.latitude;
            gps["longitude"] = gpsReading.longitude;
            gps["altitude"] = gpsReading.altitude;
            gps["hdop"] = gpsReading.hdop / 100.0;
            gps["age"] = gpsReading.age;
            gps["timestamp"] = gpsReading.timestamp;
            gps["ageMs"] = ageMs(now, gpsReading.timestamp);
        }

        // RG-15 rain sensor data (only when it's switched on)
        if (getConfigCallback().rain.enabled)
        {
            JsonObject rain = doc.createNestedObject("rainSensor");
            appendRG15Diagnostics(rain, snapshot.rg15, snapshot.rg15Diagnostics, now);
        }

        if (getConfigCallback().wind.enabled)
        {
            JsonObject wind = doc.createNestedObject("wind");
            wind["status"] = static_cast<int>(snapshot.wind.status);
            wind["speedMs"] = snapshot.wind.speedMs;
            wind["gustMs"] = snapshot.wind.gustMs;
            wind["instantMs"] = snapshot.wind.instantMs;
            wind["directionValid"] = snapshot.wind.directionValid;
            wind["directionDeg"] = snapshot.wind.directionDeg;
            wind["vaneFault"] = snapshot.wind.vaneFault;
            wind["samples"] = snapshot.wind.samples;
            wind["ageMs"] = ageMs(now, snapshot.wind.timestamp);
        }

        JsonObject safety = doc.createNestedObject("safety");
        appendSafetyStatus(safety);

        std::string json;
        serializeJson(doc, json);
        return json;
    }

    std::string WebServer::createStatusJson() const
    {
        DynamicJsonDocument doc(6144); // Includes MQTT, partition, boot, sensor and BLE diagnostics
        const SensorSnapshot snapshot = getSensorSnapshot();
        const uint32_t now = millis();

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

        const NightState night = computeNight(snapshot, getConfigCallback());
        JsonObject sky = doc.createNestedObject("sky");
        sky["locationSource"] = night.source != nullptr ? night.source : "none";
        sky["nightKnown"] = night.known;
        if (night.known)
        {
            sky["isNight"] = night.isNight;
            sky["sunAltitudeDeg"] = serialized(String(night.sunAltitudeDeg, 1));
        }

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
        const esp_partition_t *fs_partition = esp_partition_find_first(
            ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, NULL);
        if (fs_partition)
        {
            partitions["fsAddress"] = fs_partition->address;
            partitions["fsSize"] = fs_partition->size;
        }

        // Current time info (ISO format)
        JsonObject timeObj = doc.createNestedObject("time");
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

        // Sensor status
        JsonObject sensors = doc.createNestedObject("sensors");

        JsonObject tsl = sensors.createNestedObject("tsl2591");
        tsl["initialized"] = snapshot.tslInitialized;
        tsl["status"] = static_cast<int>(snapshot.tsl.status);
        tsl["lastUpdate"] = snapshot.tslLastUpdate;

        JsonObject bme = sensors.createNestedObject("bme280");
        bme["initialized"] = snapshot.bmeInitialized;
        bme["status"] = static_cast<int>(snapshot.bme.status);
        bme["lastUpdate"] = snapshot.bmeLastUpdate;

        JsonObject mlx = sensors.createNestedObject("mlx90614");
        mlx["initialized"] = snapshot.mlxInitialized;
        mlx["status"] = static_cast<int>(snapshot.mlx.status);
        mlx["lastUpdate"] = snapshot.mlxLastUpdate;

        JsonObject gps = sensors.createNestedObject("gps");
        gps["initialized"] = snapshot.gpsInitialized;
        gps["status"] = static_cast<int>(snapshot.gps.status);
        gps["lastUpdate"] = snapshot.gpsLastUpdate;

        JsonObject rg15 = sensors.createNestedObject("rg15");
        appendRG15Diagnostics(rg15, snapshot.rg15, snapshot.rg15Diagnostics, now);
        rg15["initialized"] = snapshot.rg15Initialized;
        rg15["lastUpdate"] = snapshot.rg15LastUpdate;

        JsonObject windStatus = sensors.createNestedObject("wind");
        windStatus["enabled"] = getConfigCallback().wind.enabled;
        windStatus["status"] = static_cast<int>(snapshot.wind.status);
        windStatus["vaneFault"] = snapshot.wind.vaneFault;
        windStatus["ageMs"] = ageMs(now, snapshot.wind.timestamp);

        // GPS data
        if (snapshot.gpsInitialized)
        {
            const GPSReading &gpsReading = snapshot.gps;
            JsonObject gpsData = doc.createNestedObject("gpsData");
            gpsData["hasFix"] = gpsReading.hasFix;
            gpsData["satellites"] = gpsReading.satellites;
            gpsData["latitude"] = gpsReading.latitude;
            gpsData["longitude"] = gpsReading.longitude;
            gpsData["altitude"] = gpsReading.altitude;
            gpsData["hdop"] = gpsReading.hdop / 100.0; // Convert to actual value
            gpsData["age"] = gpsReading.age;
        }

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
            mqtt["broker"] = mqttStatus.broker.c_str(); // Explicitly convert std::string
            mqtt["port"] = mqttStatus.port;
            mqtt["topic"] = mqttStatus.topic.c_str(); // Explicitly convert std::string
            mqtt["availabilityTopic"] = mqttStatus.availabilityTopic.c_str();
            mqtt["clientId"] = mqttStatus.clientId.c_str();
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
