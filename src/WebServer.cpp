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

extern uint32_t bootCount;

namespace SQM
{
    namespace
    {
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
    }

    void WebServer::begin()
    {
        Logger::info(TAG, "Starting web server on port %d", PORT);

        // CRITICAL: Register API routes BEFORE static file serving
        // Otherwise /api/* requests get treated as filesystem paths
        setupAPIRoutes();
        setupWebSocket();
        setupOTA();
        setupStaticRoutes(); // Must be last - has catch-all serveStatic

        // SPA fallback - serve index.html for any non-API routes
        server.onNotFound([](AsyncWebServerRequest *request)
                          { 
            String path = request->url();
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

        const uint32_t now = millis();

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
        server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    }

    void WebServer::setupAPIRoutes()
    {
        // Status endpoint
        server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request)
                  { handleGetStatus(request); });

        // Sensors endpoint
        server.on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest *request)
                  { handleGetSensors(request); });

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
            });
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
        // Firmware OTA update (app partition)
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
                
                Logger::info("OTA", "Found filesystem partition at 0x%x, size %d bytes", 
                    fs_partition->address, fs_partition->size);
                
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
                    Logger::error("OTA", "Partition write failed at offset %d: %d", fs_bytes_written, err);
                    fs_update_error = true;
                    fs_error_msg = "Failed to write to partition";
                    return;
                }
                fs_bytes_written += len;
                
                if (index % 10240 == 0) {  // Log every ~10KB
                    Logger::info("OTA", "Written %d bytes", fs_bytes_written);
                }
            }
            
            if (final) {
                if (!fs_update_error) {
                    Logger::info("OTA", "Filesystem update success: %d bytes written", fs_bytes_written);
                } else {
                    Logger::error("OTA", "Filesystem update failed: %s", fs_error_msg.c_str());
                }
            } });
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
        if (wsSensors.count() == 0)
            return;

        // Send only sensor data to Dashboard clients
        std::string json = createSensorDataJson();
        wsSensors.textAll(json.c_str());
    }

    void WebServer::broadcastStatusData()
    {
        if (wsStatus.count() == 0)
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

    std::string WebServer::createSensorDataJson() const
    {
        DynamicJsonDocument doc(5120);
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
        CloudMetrics cloudMetrics = CloudDetection::calculate(
            mlxReading.objectTemp,
            mlxReading.ambientTemp,
            humidity);

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

        // RG-15 rain sensor data
        {
            JsonObject rain = doc.createNestedObject("rainSensor");
            appendRG15Diagnostics(rain, snapshot.rg15, snapshot.rg15Diagnostics, now);
        }

        std::string json;
        serializeJson(doc, json);
        return json;
    }

    std::string WebServer::createStatusJson() const
    {
        StaticJsonDocument<4096> doc; // Includes MQTT, partition, and boot diagnostics
        const SensorSnapshot snapshot = getSensorSnapshot();
        const uint32_t now = millis();

        // Firmware version
        JsonObject firmware = doc.createNestedObject("firmware");
        firmware["name"] = FIRMWARE_NAME;
        firmware["version"] = FIRMWARE_VERSION;
        firmware["buildDate"] = FIRMWARE_BUILD_DATE;
        firmware["buildTime"] = FIRMWARE_BUILD_TIME;

        // System stats
        doc["uptime"] = millis() / 1000;
        doc["freeHeap"] = ESP.getFreeHeap();
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
