#pragma once

#include "Config.h"
#include "sensors/TSL2591Sensor.h"
#include "sensors/BME280Sensor.h"
#include "sensors/MLX90614Sensor.h"
#include "sensors/GPSSensor.h"
#include "calculations/SkyQuality.h"
#include "TimeManager.h"
#include "MQTTClient.h"
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <memory>
#include <vector>
#include <functional>

namespace SQM
{

    class WebServer
    {
    public:
        using GetConfigCallback = std::function<const Config &()>;
        using SaveConfigCallback = std::function<bool(const Config &)>;

        WebServer(
            TSL2591Sensor &tsl,
            BME280Sensor &bme,
            MLX90614Sensor &mlx,
            GPSSensor &gps,
            TimeManager *timeMgr,
            MQTTClient *mqtt,
            GetConfigCallback getConfig,
            SaveConfigCallback saveConfig);
        ~WebServer();

        // Delete copy operations
        WebServer(const WebServer &) = delete;
        WebServer &operator=(const WebServer &) = delete;

        void begin();
        void handle();

        // Broadcast sensor data to Dashboard WebSocket clients
        void broadcastSensorData();

        // Broadcast status data to System WebSocket clients
        void broadcastStatusData();

        // OTA update status
        void setOTAProgress(int progress);
        void setOTAError(const char *error);

    private:
        static constexpr const char *TAG = "WebServer";
        static constexpr uint16_t PORT = 80;
        static constexpr uint32_t WS_SENSOR_BROADCAST_INTERVAL_MS = 1000; // Sensors update every 1s
        static constexpr uint32_t WS_STATUS_BROADCAST_INTERVAL_MS = 2000; // Status updates every 2s

        AsyncWebServer server;
        AsyncWebSocket wsSensors; // /ws/sensors for Dashboard
        AsyncWebSocket wsStatus;  // /ws/status for System page

        TSL2591Sensor &tslSensor;
        BME280Sensor &bmeSensor;
        MLX90614Sensor &mlxSensor;
        GPSSensor &gpsSensor;
        TimeManager *timeManager;
        MQTTClient *mqttClient;
        GetConfigCallback getConfigCallback;
        SaveConfigCallback saveConfigCallback;

        uint32_t lastSensorBroadcast;
        uint32_t lastStatusBroadcast;

        // Setup route handlers
        void setupStaticRoutes();
        void setupAPIRoutes();
        void setupWebSocket();
        void setupOTA();

        // API endpoint handlers
        void handleGetStatus(AsyncWebServerRequest *request);
        void handleGetSensors(AsyncWebServerRequest *request);
        void handleGetConfig(AsyncWebServerRequest *request);
        void handleRestart(AsyncWebServerRequest *request);
        void handleWiFiScan(AsyncWebServerRequest *request);
        void handleMQTTTest(AsyncWebServerRequest *request, JsonVariant &json);

        // WebSocket handlers
        void onSensorWebSocketEvent(
            AsyncWebSocket *server,
            AsyncWebSocketClient *client,
            AwsEventType type,
            void *arg,
            uint8_t *data,
            size_t len);

        void onStatusWebSocketEvent(
            AsyncWebSocket *server,
            AsyncWebSocketClient *client,
            AwsEventType type,
            void *arg,
            uint8_t *data,
            size_t len);

        // Helper functions
        std::string createSensorDataJson() const;
        std::string createStatusJson() const;
        static std::string createErrorJson(const char *error);
    };

} // namespace SQM
