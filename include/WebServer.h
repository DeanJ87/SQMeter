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
#include "LanguagePack.h"
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
        friend class LanguagePack;

    public:
        using GetConfigCallback = std::function<const Config &()>;
        using SaveConfigCallback = std::function<bool(const Config &)>;

        // The sensors the server reads and acts on (calibration, RG-15 commands).
        struct Sensors
        {
            TSL2591Sensor &tsl;
            BME280Sensor &bme;
            MLX90614Sensor &mlx;
            GPSSensor &gps;
            RG15Sensor &rg15;
            WindSensor &wind;
        };

        WebServer(
            const Sensors &sensors, TimeManager *timeMgr, MQTTClient *mqtt, GetConfigCallback getConfig, SaveConfigCallback saveConfig);
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
        uint32_t lastAlertsRevision = 0;
        std::atomic<uint32_t> configRevision{0};
        uint32_t lastConfigRevision = 0;
        SensorSnapshot sensorSnapshot;
        SemaphoreHandle_t sensorSnapshotMutex;
        bool wifiConnectActive;
        bool wifiConnectConfigSaved;
        uint32_t wifiConnectStartedAt;
        std::string pendingWifiSSID;
        std::string pendingWifiPassword;

        std::unique_ptr<OtaUpdater> otaUpdater;
        std::unique_ptr<LanguagePack> languagePack;

        AsyncUDP alpacaDiscoveryUdp; // answers in the network task, not the main loop
        // IPv6 discovery (ff12::a1:2345) needs an IPv6 address first; retried from handle().
        bool alpacaIpv6Pending = false;
        uint32_t lastAlpacaIpv6Attempt = 0;
        void retryAlpacaIpv6Discovery(uint32_t now);
        // begin(), step by step.
        void restoreAlertSchedule();
        void subscribeArmCommands();
        void startBle();
        void startAlpacaDiscovery();
        static void handleNotFound(AsyncWebServerRequest *request);
        // The Alpaca HTTP API lives in lib/AlpacaLogic (Alpaca::Router) so the
        // CI simulator runs the same code; this feeds it the device's state.
        class AlpacaBackend : public Alpaca::Backend
        {
        public:
            explicit AlpacaBackend(WebServer &owner)
                : owner(owner)
            {
            }
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

        // When alerts go out (specs/021): the send mode and the pause state
        // ("armed"), with why and since when. Saved in NVS. Pause/resume
        // requests come from HTTP (AsyncTCP task) and MQTT; the loop task
        // applies them, and Alpaca connects in the alert pass.
        Alerts::AlertSchedule alertSchedule;
        // -1 none, else source * 2 + (resume ? 1 : 0), source an Alerts::ScheduleReason.
        std::atomic<int8_t> pendingArm{-1};
        static int8_t encodeArm(bool resume, Alerts::ScheduleReason source)
        {
            return static_cast<int8_t>(static_cast<int>(source) * 2 + (resume ? 1 : 0));
        }
        // Whether the imaging app is still checking each Alpaca device.
        Alpaca::ClientWatch clientWatch;
        // Copies for the HTTP handlers, which run on the AsyncTCP task.
        Alerts::ScheduleState scheduleShared;
        Alpaca::ClientWatch clientWatchShared;
        mutable portMUX_TYPE scheduleLock = portMUX_INITIALIZER_UNLOCKED;
        void shareSchedule();
        Alerts::ScheduleState sharedSchedule() const; // with any pending pause/resume applied
        Alpaca::ClientWatch sharedClientWatch() const;
        uint32_t mqttArmedConnection = 0xFFFFFFFF;
        void applyPendingArm();
        void scheduleChanged(bool wasSending);
        void publishArmedState();
        std::unique_ptr<AlertDispatcher> alertDispatcher;
        // Set by the HTTP handler, sent from the loop task. event < 0 is the
        // plain channel test; otherwise an index into the sample events.
        struct PendingAlertTest
        {
            uint8_t mask = 0;
            const Core::SampleAlert *sample = nullptr; // nullptr: the generic test
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
        void seedAlertEngine();
        void watchClients(Alerts::AlertInputs &inputs, const Config &cfg, const std::string &localTime);
        void updateBle(const Alerts::AlertInputs &inputs, const Core::AlertSources &sources);
        void sendAlerts(const Core::AlertStep &step, const Core::AlertSources &sources, const SensorSnapshot &snapshot, uint32_t epoch);
        void forwardBleAcks(const Config &cfg, const SensorSnapshot &snapshot);

        static Core::NightState computeNight(const SensorSnapshot &snapshot, const Config &cfg);
        // What the settings dependencies need to know (lib/SettingsDeps).
        Deps::Facts settingsFacts(const SensorSnapshot &snapshot) const;
        // Alert channels switched on but inactive, with the reason. Alerts
        // being off doesn't block: tests work while alerts are off.
        ChannelBlocks channelBlocks(const SensorSnapshot &snapshot) const;
        // "HH:MM" and "YYYY-MM-DD" in the device's time zone, or "--:--"/"--" before the clock is set.
        static void localClock(std::string &timeText, std::string &dateText);
        void publishMqttSafety(const SafetyStatus &status);
        void setupAlertRoutes();
        void handleAlertTest(AsyncWebServerRequest *request);
        void handleAlertsClear(AsyncWebServerRequest *request);
        void handleBleAck(AsyncWebServerRequest *request);
        void handleBleForgetBonds(AsyncWebServerRequest *request);
        void handleArm(AsyncWebServerRequest *request, bool armed);
        void handleAlertsArmed(AsyncWebServerRequest *request);
        void handleAlertsRecent(AsyncWebServerRequest *request);

        BleService ble;

        // Setup route handlers
        void setupStaticRoutes();
        void setupAPIRoutes();
        void setupWebSocket();
        void setupOTA();
        void handleFsUploadDone(AsyncWebServerRequest *request);
        void handleFirmwareUploadDone(AsyncWebServerRequest *request);
        void setupGithubUpdates();
        void handleUpdatesCheck(AsyncWebServerRequest *request);
        void handleUpdatesApply(AsyncWebServerRequest *request, JsonVariant &json);
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
        // setupAPIRoutes(), by group (registration order matters: more
        // specific paths first).
        void setupReadingRoutes();
        void setupSettingsRoutes();
        void setupNetworkRoutes();
        static void handleSafetyHistory(AsyncWebServerRequest *request);
        void handleSettingsEffective(AsyncWebServerRequest *request);
        void handleGetSafety(AsyncWebServerRequest *request);
        void handleSetConfig(AsyncWebServerRequest *request, JsonVariant &json);
        void handleWiFiConnect(AsyncWebServerRequest *request, JsonVariant &json);

        // WebSocket handlers
        void onSensorWebSocketEvent(AsyncWebSocketClient *client, AwsEventType type);
        void onStatusWebSocketEvent(AsyncWebSocketClient *client, AwsEventType type);

        // Helper functions
        bool requireAuth(AsyncWebServerRequest *request) const;
        SensorSnapshot getSensorSnapshot() const;
        std::string createSensorDataJson() const;
        void appendSafetyStatus(JsonObject target) const;
        std::string createStatusJson() const;
        // createStatusJson() sections, in document order.
        void appendFirmware(JsonDocument &doc) const;
        void appendRuntime(JsonDocument &doc) const;
        void appendTime(JsonDocument &doc) const;
        void appendWifi(JsonDocument &doc) const;
        void appendMqttStatus(JsonDocument &doc) const;
        static std::string createErrorJson(const char *error);
        static uint32_t ageMs(uint32_t now, uint32_t timestamp);

        // Alpaca helpers
        Alpaca::SafetyInputs buildAlpacaSafetyInputs() const;
        static Alpaca::SafetyThresholds buildAlpacaSafetyThresholds(const Config &cfg);
        Alpaca::SafetyResult evaluateAlpacaSafety() const;
        Alpaca::ObservingConditionsSnapshot buildAlpacaObservingConditionsSnapshot() const;
    };

} // namespace SQM
