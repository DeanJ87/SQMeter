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
#include <nvs.h>
#include <nvs_flash.h>
#include "calculations/CloudDetection.h"

namespace SQM
{

    WebServer::WebServer(
        TSL2591Sensor &tsl,
        BME280Sensor &bme,
        MLX90614Sensor &mlx,
        GPSSensor &gps,
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
          timeManager(timeMgr),
          mqttClient(mqtt),
          getConfigCallback(getConfig),
          saveConfigCallback(saveConfig),
          lastSensorBroadcast(0),
          lastStatusBroadcast(0)
    {
    }

    WebServer::~WebServer()
    {
        wsSensors.closeAll();
        wsStatus.closeAll();
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

        // Config endpoints
        server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request)
                  { handleGetConfig(request); });

        AsyncCallbackJsonWebHandler *configHandler = new AsyncCallbackJsonWebHandler(
            "/api/config",
            [this](AsyncWebServerRequest *request, JsonVariant &json)
            {
                String jsonStr;
                serializeJson(json, jsonStr);
                auto configOpt = Config::fromJson(jsonStr.c_str());

                if (!configOpt)
                {
                    request->send(400, "application/json", createErrorJson("Invalid configuration").c_str());
                    return;
                }

                if (saveConfigCallback(*configOpt))
                {
                    request->send(200, "application/json", "{\"success\":true}");
                }
                else
                {
                    request->send(500, "application/json", createErrorJson("Failed to save configuration").c_str());
                }
            });
        server.addHandler(configHandler);

        // System endpoints
        server.on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest *request)
                  { handleRestart(request); });

        // WiFi endpoints
        server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request)
                  { handleWiFiScan(request); });

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
                JsonObject jsonObj = json.as<JsonObject>();

                if (!jsonObj.containsKey("ssid") || !jsonObj.containsKey("password"))
                {
                    request->send(400, "application/json", "{\"error\":\"Missing SSID or password\"}");
                    return;
                }

                const char *ssid = jsonObj["ssid"];
                const char *password = jsonObj["password"];

                Logger::info(TAG, "Testing connection to SSID: '%s'", ssid);

                // Test connection
                WiFi.disconnect();
                delay(100);
                WiFi.begin(ssid, password);

                int attempts = 0;
                while (WiFi.status() != WL_CONNECTED && attempts < 20)
                {
                    delay(500);
                    attempts++;
                    Logger::debug(TAG, "Connecting... attempt %d/20, status=%d", attempts, WiFi.status());
                }

                if (WiFi.status() != WL_CONNECTED)
                {
                    Logger::error(TAG, "Failed to connect. Status: %d", WiFi.status());
                    String errorMsg;
                    switch (WiFi.status())
                    {
                    case WL_NO_SSID_AVAIL:
                        errorMsg = "{\"error\":\"Network not found\"}";
                        break;
                    case WL_CONNECT_FAILED:
                        errorMsg = "{\"error\":\"Wrong password or connection refused\"}";
                        break;
                    default:
                        errorMsg = "{\"error\":\"Connection failed\"}";
                    }
                    request->send(401, "application/json", errorMsg.c_str());
                    WiFi.disconnect();
                    return;
                }

                Logger::info(TAG, "Connected! IP: %s", WiFi.localIP().toString().c_str());

                // Save config
                Config config = getConfigCallback();
                config.wifi.ssid = ssid;
                config.wifi.password = password;

                if (!saveConfigCallback(config))
                {
                    request->send(200, "application/json", "{\"success\":true,\"message\":\"Connected but not saved\"}");
                    return;
                }

                // Return success with new IP address using ArduinoJson
                StaticJsonDocument<256> doc;
                doc["success"] = true;
                doc["message"] = "Connected successfully!";
                doc["ip"] = WiFi.localIP().toString();

                String *responseStr = new String();
                serializeJson(doc, *responseStr);

                AsyncWebServerResponse *response = request->beginResponse(200, "application/json", *responseStr);
                request->send(response);
                delete responseStr;

                // Disable AP after response is sent (in async task to avoid crash)
                // The AP will stay up so user can see the response and navigate to new IP
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
        server.on("/api/update", HTTP_POST, [](AsyncWebServerRequest *request)
                  {
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
                delay(1000);
                ESP.restart();
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

        server.on("/api/update/fs", HTTP_POST, [](AsyncWebServerRequest *request)
                  {
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
                delay(1000);
                ESP.restart();
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
        std::string json = cfg.toJson();
        request->send(200, "application/json", json.c_str());
    }

    void WebServer::handleRestart(AsyncWebServerRequest *request)
    {
        request->send(200, "application/json", "{\"success\":true,\"message\":\"Restarting...\"}");
        delay(1000);
        ESP.restart();
    }

    void WebServer::handleWiFiScan(AsyncWebServerRequest *request)
    {
        int n = WiFi.scanNetworks();

        StaticJsonDocument<2048> doc;
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
    }

    void WebServer::handleMQTTTest(AsyncWebServerRequest *request, JsonVariant &json)
    {
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

    std::string WebServer::createSensorDataJson() const
    {
        StaticJsonDocument<1024> doc; // Increased to accommodate GPS data

        // Light sensor data (TSL2591)
        const auto &tslReading = tslSensor.getReading();
        JsonObject lightSensor = doc.createNestedObject("lightSensor");
        lightSensor["lux"] = tslReading.lux;
        lightSensor["visible"] = tslReading.visible;
        lightSensor["infrared"] = tslReading.infrared;
        lightSensor["full"] = tslReading.full;
        lightSensor["status"] = static_cast<int>(tslReading.status);

        // Sky quality calculations
        SkyQualityMetrics sqm = SkyQuality::calculate(tslReading.lux);
        JsonObject sky = doc.createNestedObject("skyQuality");
        sky["sqm"] = sqm.sqm;
        sky["nelm"] = sqm.nelm;
        sky["bortle"] = sqm.bortle;
        sky["description"] = SkyQuality::getBortleDescription(sqm.bortle);

        // Environmental sensor data (BME280)
        const auto &bmeReading = bmeSensor.getReading();
        JsonObject environment = doc.createNestedObject("environment");
        environment["temperature"] = bmeReading.temperature;
        environment["humidity"] = bmeReading.humidity;
        environment["pressure"] = bmeReading.pressure;
        environment["dewpoint"] = bmeReading.dewpoint;
        environment["status"] = static_cast<int>(bmeReading.status);

        // IR temperature sensor data (MLX90614)
        const auto &mlxReading = mlxSensor.getReading();
        JsonObject irTemperature = doc.createNestedObject("irTemperature");
        irTemperature["objectTemp"] = mlxReading.objectTemp;
        irTemperature["ambientTemp"] = mlxReading.ambientTemp;
        irTemperature["status"] = static_cast<int>(mlxReading.status);

        // Cloud detection from IR temperature sensor
        // Use BME280 humidity if available, otherwise default to 53%
        float humidity = (bmeReading.status == SensorStatus::OK) ? bmeReading.humidity : 53.0f;
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

        // GPS data (if initialized)
        if (gpsSensor.isInitialized())
        {
            const GPSReading &gpsReading = gpsSensor.getReading();
            JsonObject gps = doc.createNestedObject("gps");
            gps["hasFix"] = gpsReading.hasFix;
            gps["satellites"] = gpsReading.satellites;
            gps["latitude"] = gpsReading.latitude;
            gps["longitude"] = gpsReading.longitude;
            gps["altitude"] = gpsReading.altitude;
            gps["hdop"] = gpsReading.hdop;
            gps["age"] = gpsReading.age;
        }

        std::string json;
        serializeJson(doc, json);
        return json;
    }

    std::string WebServer::createStatusJson() const
    {
        StaticJsonDocument<2048> doc; // Increased from 1024 to accommodate MQTT and partition data

        // Firmware version
        JsonObject firmware = doc.createNestedObject("firmware");
        firmware["name"] = FIRMWARE_NAME;
        firmware["version"] = FIRMWARE_VERSION;
        firmware["buildDate"] = FIRMWARE_BUILD_DATE;
        firmware["buildTime"] = FIRMWARE_BUILD_TIME;

        // System stats
        doc["uptime"] = millis() / 1000;
        doc["freeHeap"] = ESP.getFreeHeap();
        doc["heapSize"] = ESP.getHeapSize();
        doc["cpuFreqMHz"] = ESP.getCpuFreqMHz();
        doc["flashSize"] = ESP.getFlashChipSize();
        doc["sketchSize"] = ESP.getSketchSize();
        doc["freeSketchSpace"] = ESP.getFreeSketchSpace();

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

        // Sensor status
        JsonObject sensors = doc.createNestedObject("sensors");

        JsonObject tsl = sensors.createNestedObject("tsl2591");
        tsl["initialized"] = tslSensor.isInitialized();
        tsl["status"] = static_cast<int>(tslSensor.getReading().status);
        tsl["lastUpdate"] = tslSensor.getLastUpdateTime();

        JsonObject bme = sensors.createNestedObject("bme280");
        bme["initialized"] = bmeSensor.isInitialized();
        bme["status"] = static_cast<int>(bmeSensor.getReading().status);
        bme["lastUpdate"] = bmeSensor.getLastUpdateTime();

        JsonObject mlx = sensors.createNestedObject("mlx90614");
        mlx["initialized"] = mlxSensor.isInitialized();
        mlx["status"] = static_cast<int>(mlxSensor.getReading().status);
        mlx["lastUpdate"] = mlxSensor.getLastUpdateTime();

        JsonObject gps = sensors.createNestedObject("gps");
        gps["initialized"] = gpsSensor.isInitialized();
        gps["status"] = static_cast<int>(gpsSensor.getReading().status);
        gps["lastUpdate"] = gpsSensor.getLastUpdateTime();

        // GPS data
        if (gpsSensor.isInitialized())
        {
            const GPSReading &gpsReading = gpsSensor.getReading();
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
        }

        std::string json;
        serializeJson(doc, json);
        return json;
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
