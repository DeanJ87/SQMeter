#pragma once

#include "Config.h"
#include "sensors/TSL2591Sensor.h"
#include "sensors/BME280Sensor.h"
#include "sensors/MLX90614Sensor.h"
#include "sensors/GPSSensor.h"
#include "sensors/RG15Sensor.h"
#include "calculations/SkyQuality.h"
#include "TimeManager.h"
#include "MQTTClient.h"
#include "OtaUpdater.h"
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <memory>
#include <vector>
#include <functional>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

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
            RG15Sensor &rg15,
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
        void refreshSensorSnapshot(uint32_t dataTimestampMs);

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
        static constexpr uint32_t SENSOR_STALE_GRACE_MS = 1000;
        static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;

        struct SensorSnapshot
        {
            TSL2591Reading tsl;
            TSL2591Diagnostics tslDiagnostics;
            BME280Reading bme;
            MLX90614Reading mlx;
            GPSReading gps;
            RG15Reading rg15;
            RG15Diagnostics rg15Diagnostics;
            bool gpsInitialized = false;
            bool rg15Initialized = false;
            bool tslInitialized = false;
            bool bmeInitialized = false;
            bool mlxInitialized = false;
            uint32_t tslLastUpdate = 0;
            uint32_t bmeLastUpdate = 0;
            uint32_t mlxLastUpdate = 0;
            uint32_t gpsLastUpdate = 0;
            uint32_t rg15LastUpdate = 0;
            uint32_t dataTimestamp = 0;
            uint32_t capturedAt = 0;
        };

        AsyncWebServer server;
        AsyncWebSocket wsSensors; // /ws/sensors for Dashboard
        AsyncWebSocket wsStatus;  // /ws/status for System page

        TSL2591Sensor &tslSensor;
        BME280Sensor &bmeSensor;
        MLX90614Sensor &mlxSensor;
        GPSSensor &gpsSensor;
        RG15Sensor &rg15Sensor;
        TimeManager *timeManager;
        MQTTClient *mqttClient;
        GetConfigCallback getConfigCallback;
        SaveConfigCallback saveConfigCallback;

        uint32_t lastSensorBroadcast;
        uint32_t lastStatusBroadcast;
        SensorSnapshot sensorSnapshot;
        SemaphoreHandle_t sensorSnapshotMutex;
        bool wifiConnectActive;
        bool wifiConnectConfigSaved;
        uint32_t wifiConnectStartedAt;
        std::string pendingWifiSSID;
        std::string pendingWifiPassword;

        std::unique_ptr<OtaUpdater> otaUpdater;

        // Setup route handlers
        void setupStaticRoutes();
        void setupAPIRoutes();
        void setupWebSocket();
        void setupOTA();
        void setupGithubUpdates();

        // API endpoint handlers
        void handleGetStatus(AsyncWebServerRequest *request);
        void handleGetSensors(AsyncWebServerRequest *request);
        void handleGetConfig(AsyncWebServerRequest *request);
        void handleRestart(AsyncWebServerRequest *request);
        void handleWiFiScan(AsyncWebServerRequest *request);
        void handleRG15Test(AsyncWebServerRequest *request);
        void handleTSL2591DarkCalibration(AsyncWebServerRequest *request);
        void handleRG15ResetTotal(AsyncWebServerRequest *request);
        void handleRG15Reboot(AsyncWebServerRequest *request);
        void handleMQTTTest(AsyncWebServerRequest *request, JsonVariant &json);
        void pollWiFiConnect();

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
        bool requireAuth(AsyncWebServerRequest *request) const;
        SensorSnapshot getSensorSnapshot() const;
        std::string createSensorDataJson() const;
        std::string createStatusJson() const;
        static std::string createErrorJson(const char *error);
        static bool scheduleRestart(uint32_t delayMs);
        static uint32_t ageMs(uint32_t now, uint32_t timestamp);
    };

} // namespace SQM
