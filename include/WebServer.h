#pragma once

#include "Config.h"
#include "sensors/TSL2591Sensor.h"
#include "sensors/BME280Sensor.h"
#include "sensors/MLX90614Sensor.h"
#include "sensors/GPSSensor.h"
#include "sensors/RG15Sensor.h"
#include "sensors/WindSensor.h"
#include "DeviceCore.h"
#include "TimeManager.h"
#include "MQTTClient.h"
#include "OtaUpdater.h"
#include "SafetyEvaluator.h"
#include "ObservingConditionsMapper.h"
#include "AlpacaProtocol.h"
#include "AlpacaRouter.h"
#include "Readings.h"
#include "AlertDispatcher.h"
#include "AlertEngine.h"
#include "SafetyStatus.h"
#include "BleService.h"
#include <atomic>
#include <ESPAsyncWebServer.h>
#include <AsyncWebSocket.h>
#include <ArduinoJson.h>
#include <memory>
#include <vector>
#include <AsyncUDP.h>
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

        // The setup screen is trying new WiFi credentials (POST /api/wifi/connect).
        bool isWifiConnectPending() const { return wifiConnectActive; }
        static bool scheduleRestart(uint32_t delayMs);

    private:
        static constexpr const char *TAG = "WebServer";
        static constexpr uint16_t PORT = 80;
        static constexpr uint32_t WS_SENSOR_BROADCAST_INTERVAL_MS = 1000; // Sensors update every 1s
        static constexpr uint32_t WS_STATUS_BROADCAST_INTERVAL_MS = 2000; // Status updates every 2s
        static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 10000;

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

        AsyncUDP alpacaDiscoveryUdp; // answers in the network task, not the main loop
        // The Alpaca HTTP API lives in lib/AlpacaLogic (Alpaca::Router) so the
        // CI simulator runs the same code; this feeds it the device's state.
        class AlpacaBackend : public Alpaca::Backend
        {
        public:
            explicit AlpacaBackend(WebServer &owner) : owner(owner) {}
            bool alpacaEnabled() const override;
            bool isSafe() const override;
            Alpaca::ObservingConditionsSnapshot observingConditions() const override;
            std::string location() const override;
            std::string timestampUtc() const override;

        private:
            WebServer &owner;
        };
        static Alpaca::ServerIdentity alpacaIdentity();
        AlpacaBackend alpacaBackend{*this};
        Alpaca::Router alpacaRouter{alpacaBackend, alpacaIdentity()};

        SafetyStatus safetyStatus;
        Alpaca::SafeDelayFilter safeDelayFilter;
        SemaphoreHandle_t safetyMutex = xSemaphoreCreateMutex();
        TaskHandle_t loopTaskHandle = nullptr;
        uint32_t lastSafetyEvaluation = 0;
        static constexpr uint32_t SAFETY_EVALUATION_INTERVAL_MS = 1000;
        void updateSafetyStatus();

        // Alerts
        Alerts::AlertEngine alertEngine;
        bool alertEngineSeeded = false;

        // Alerts on/off ("armed"): off while you're not imaging, so weather
        // flapping with the scope packed away doesn't wake anyone. Saved in
        // NVS. Requests come from HTTP (AsyncTCP task), MQTT and Alpaca
        // connects; the loop task applies them.
        bool alertsArmed = true;
        std::atomic<int8_t> pendingArm{-1}; // -1 none, 0 off, 1 on
        bool lastAlpacaConnected = false;
        uint32_t mqttArmedConnection = 0xFFFFFFFF;
        void applyPendingArm();
        void publishArmedState();
        std::unique_ptr<AlertDispatcher> alertDispatcher;
        // Set by the HTTP handler, sent from the loop task. event < 0 is the
        // plain channel test; otherwise an index into the sample events.
        struct PendingAlertTest
        {
            uint8_t mask = 0;
            int8_t event = -1;
            uint8_t level = 2;
            char sound[33] = {};
            char title[AlertsConfig::MAX_TEMPLATE_TITLE + 1] = {};
            char message[AlertsConfig::MAX_TEMPLATE_MESSAGE + 1] = {};
        };
        PendingAlertTest pendingAlertTest;
        portMUX_TYPE pendingAlertTestLock = portMUX_INITIALIZER_UNLOCKED;
        uint32_t mqttSafetyConnection = 0xFFFFFFFF;
        uint32_t mqttStatePublishedAt = 0;
        uint32_t mqttStateConnection = 0xFFFFFFFF;
        std::string discoveryKey;
        uint32_t discoveryConnection = 0xFFFFFFFF;
        Readings::DiscoveryDevice discoveryDevice;
        bool discoveryWasOn = false;
        Readings::Snapshot buildReadings() const;
        void appendDiagnostics(JsonObject root, const SensorSnapshot &snapshot) const;
        void publishMqttReadings(uint32_t now);
        void publishDiscovery(const MQTTConfig &mqtt, const Readings::Groups &groups);
        bool mqttSafetyPublished = false;
        bool mqttLastPublishedSafe = false;
        uint32_t mqttSafetyPublishedAt = 0;
        static constexpr uint32_t MQTT_SAFETY_REPUBLISH_MS = 60000;
        void processAlerts(const SafetyStatus &status);

        static Core::NightState computeNight(const SensorSnapshot &snapshot, const Config &cfg);
        // "HH:MM" and "YYYY-MM-DD" in the device's time zone, or "--:--"/"--" before the clock is set.
        static void localClock(std::string &timeText, std::string &dateText);
        void publishMqttSafety(const SafetyStatus &status);
        void setupAlertRoutes();

        BleService ble;

        // Setup route handlers
        void setupStaticRoutes();
        void setupAPIRoutes();
        void setupWebSocket();
        void setupOTA();
        void setupGithubUpdates();
        void setupAlpacaRoutes();
        void handleAlpacaRequest(AsyncWebServerRequest *request);

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
        static uint32_t ageMs(uint32_t now, uint32_t timestamp);

        // Alpaca helpers
        Alpaca::SafetyInputs buildAlpacaSafetyInputs() const;
        static Alpaca::SafetyThresholds buildAlpacaSafetyThresholds(const Config &cfg);
        Alpaca::SafetyResult evaluateAlpacaSafety() const;
        Alpaca::ObservingConditionsSnapshot buildAlpacaObservingConditionsSnapshot() const;
    };

} // namespace SQM
