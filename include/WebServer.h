#pragma once

#include "Config.h"
#include "sensors/TSL2591Sensor.h"
#include "sensors/BME280Sensor.h"
#include "sensors/MLX90614Sensor.h"
#include "sensors/GPSSensor.h"
#include "sensors/RG15Sensor.h"
#include "sensors/WindSensor.h"
#include "calculations/SkyQuality.h"
#include "TimeManager.h"
#include "MQTTClient.h"
#include "OtaUpdater.h"
#include "SafetyEvaluator.h"
#include "ObservingConditionsMapper.h"
#include "AlpacaProtocol.h"
#include "AlertDispatcher.h"
#include "AlertEngine.h"
#include "SafetyStatus.h"
#include "BleService.h"
#include <atomic>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <WiFiUdp.h>
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
            WindSensor &wind,
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
        SafetyStatus getSafetyStatus() const;

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
            WindReading wind;
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
        WindSensor &windSensor;
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

        WiFiUDP alpacaDiscoveryUdp;
        bool alpacaDiscoveryStarted = false;
        mutable uint32_t alpacaServerTransactionId = 0;
        // Per-device Connected state (index 0 = SafetyMonitor, 1 = ObservingConditions).
        // Shared by all clients - the device is always reachable, so this
        // only reflects what clients last set via Connect/Disconnect/Connected.
        bool alpacaConnected[2] = {false, false};

        SafetyStatus safetyStatus;
        Alpaca::SafeDelayFilter safeDelayFilter;
        SemaphoreHandle_t safetyMutex = xSemaphoreCreateMutex();
        TaskHandle_t loopTaskHandle = nullptr;
        uint32_t lastSafetyEvaluation = 0;
        static constexpr uint32_t SAFETY_EVALUATION_INTERVAL_MS = 1000;
        void updateSafetyStatus();

        // Alerts
        Alerts::AlertEngine alertEngine;
        std::unique_ptr<AlertDispatcher> alertDispatcher;
        std::atomic<uint8_t> pendingAlertTestMask{0}; // set by HTTP handler, sent from the loop task
        bool mqttSafetyPublished = false;
        bool mqttLastPublishedSafe = false;
        uint32_t mqttSafetyPublishedAt = 0;
        static constexpr uint32_t MQTT_SAFETY_REPUBLISH_MS = 60000;
        void processAlerts(const SafetyStatus &status);

        struct NightState
        {
            const char *source = nullptr; // "gps", "manual" or null
            double latitude = 0.0;
            double longitude = 0.0;
            bool known = false;
            bool isNight = false;
            double sunAltitudeDeg = 0.0;
        };
        static NightState computeNight(const SensorSnapshot &snapshot, const Config &cfg);
        void publishMqttSafety(const SafetyStatus &status);
        void setupAlertRoutes();

        BleService ble;
        // Separate from alertEngine: phone alarms follow the Bluetooth alarm
        // settings, not the push-notification ones.
        Alerts::AlertEngine bleAlarmEngine;
        void processBleAlarms(const Alerts::AlertInputs &inputs, const SafetyStatus &status, const Config &cfg);

        // Setup route handlers
        void setupStaticRoutes();
        void setupAPIRoutes();
        void setupWebSocket();
        void setupOTA();
        void setupGithubUpdates();
        void setupAlpacaRoutes();
        void handleAlpacaDeviceRequest(AsyncWebServerRequest *request);
        void handleAlpacaDiscovery();

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
        void appendSafetyStatus(JsonObject target) const;
        std::string createStatusJson() const;
        static std::string createErrorJson(const char *error);
        static bool scheduleRestart(uint32_t delayMs);
        static uint32_t ageMs(uint32_t now, uint32_t timestamp);

        // Alpaca helpers
        Alpaca::SafetyInputs buildAlpacaSafetyInputs() const;
        static Alpaca::SafetyThresholds buildAlpacaSafetyThresholds(const Config &cfg);
        Alpaca::SafetyResult evaluateAlpacaSafety() const;
        Alpaca::ObservingConditionsSnapshot buildAlpacaObservingConditionsSnapshot() const;
        std::string buildAlpacaResponseBool(AsyncWebServerRequest *request, bool value, int errorNumber, const std::string &errorMessage) const;
        std::string buildAlpacaResponseDouble(AsyncWebServerRequest *request, double value, int errorNumber, const std::string &errorMessage) const;
        std::string buildAlpacaResponseVoid(AsyncWebServerRequest *request, int errorNumber, const std::string &errorMessage) const;
    };

} // namespace SQM
