#include "WebServer.h"
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
#include "CaptivePortal.h"
#include "Ipv6Network.h"
#include "DualStackClient.h"
#include "NetAddress.h"
#include "WiFiManager.h"
#include "HeapTrace.h"
#include "SunPosition.h"
#include "SafetyHistory.h"
#include <Preferences.h>

extern uint32_t bootCount;

// The web server's core: start-up, the main-loop pass, the sensor snapshot,
// static files and the live-update WebSockets. Routes and documents are in
// WebServerApi.cpp, WebServerAlerts.cpp, WebServerAlpaca.cpp,
// WebServerStatus.cpp and WebServerUpdates.cpp.

namespace SQM
{
    using namespace WebShared;

    namespace
    {
        // Where captive-portal probes land: the WiFi setup screen on the hotspot.
        std::string setupScreenUrl()
        {
            return std::string("http://") + WiFi.softAPIP().toString().c_str() + "/wifi";
        }

        // The request came in over the setup hotspot (not the home network),
        // so it may be sent to the setup screen.
        bool arrivedViaHotspot(AsyncWebServerRequest *request)
        {
            if ((WiFi.getMode() & WIFI_AP) == 0)
                return false;
            AsyncClient *client = request->client();
            return client != nullptr && client->localIP() == WiFi.softAPIP();
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

        esp_timer_handle_t restartTimer = nullptr;

        void restartTimerCallback(void *)
        {
            ESP.restart();
        }

    } // namespace

    WebServer::WebServer(
        const Sensors &sensors, TimeManager *timeMgr, MQTTClient *mqtt, GetConfigCallback getConfig, SaveConfigCallback saveConfig)
        : server(PORT),
          wsSensors("/ws/sensors"),
          wsStatus("/ws/status"),
          tslSensor(sensors.tsl),
          bmeSensor(sensors.bme),
          mlxSensor(sensors.mlx),
          gpsSensor(sensors.gps),
          rg15Sensor(sensors.rg15),
          windSensor(sensors.wind),
          timeManager(timeMgr),
          mqttClient(mqtt),
          getConfigCallback(getConfig),
          // A save bumps configRevision (/api/status): open pages reload the settings.
          saveConfigCallback(
              [this, saveConfig](const Config &cfg)
              {
                  const bool saved = saveConfig(cfg);
                  if (saved)
                      ++configRevision;
                  return saved;
              }),
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
        restoreAlertSchedule();
        subscribeArmCommands();
        Logger::info(TAG, "Starting web server on port %d", PORT);

        // CRITICAL: Register API routes BEFORE static file serving
        // Otherwise /api/* requests get treated as filesystem paths
        restoreAlpacaConnections();
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
        startBle();
        if (getConfigCallback().alpaca.enabled)
            startAlpacaDiscovery();

        // SPA fallback - serve index.html for any non-API routes
        server.onNotFound([](AsyncWebServerRequest *request) { handleNotFound(request); });

        // Not server.begin(): the device owns the listener (Ipv6Network::listen).
        Ipv6Network::listen(server, PORT);
        Logger::info(TAG, "Web server started");
    }

    void WebServer::restoreAlertSchedule()
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

    // Home Assistant MQTT switch: command <topic>/alerts/armed/set, state
    // <topic>/alerts/armed.
    void WebServer::subscribeArmCommands()
    {
        if (mqttClient == nullptr)
            return;
        mqttClient->onCommand(
            "alerts/armed/set",
            [this](const std::string &payload)
            {
                bool armed = false;
                if (parseArmPayload(payload, armed))
                    pendingArm = encodeArm(armed, Alerts::ScheduleReason::UserMqtt);
            });
    }

    void WebServer::startBle()
    {
        if (!BleService::available())
            return;
        if (!getConfigCallback().ble.enabled)
        {
            BleService::releaseControllerMemory();
            HeapTrace::mark("bluetooth memory released");
            return;
        }
        ble.begin(getConfigCallback().deviceName, getConfigCallback().ble.passkey);
        HeapTrace::mark("bluetooth");
    }

    void WebServer::startAlpacaDiscovery()
    {
        // With IPv6 on, one dual-stack socket takes IPv4 broadcasts and the
        // IPv6 discovery group (spec 015, FR-005).
        const bool ipv6 = WiFiManager::ipv6Running();
        const bool listening = ipv6 ? alpacaDiscoveryUdp.listen(IP_ANY_TYPE, Alpaca::DISCOVERY_UDP_PORT)
                                    : alpacaDiscoveryUdp.listen(Alpaca::DISCOVERY_UDP_PORT);
        alpacaIpv6Pending = listening && ipv6;
        if (!listening)
        {
            Logger::error(TAG, "Failed to start Alpaca UDP discovery listener");
            return;
        }
        // Replies straight from the UDP task: discovery answers within
        // milliseconds instead of waiting for a main-loop pass.
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

    void WebServer::handleNotFound(AsyncWebServerRequest *request)
    {
        const String path = request->url();
        const bool hostIsDevice = request->host() == WiFi.softAPIP().toString();
        switch (CaptivePortal::notFound(path.c_str(), arrivedViaHotspot(request), hostIsDevice))
        {
        case CaptivePortal::NotFound::AlpacaError:
            // Alpaca device API: the spec requires HTTP 400 with a plain-text
            // body for an unknown device type/number, method, or HTTP verb.
            Logger::debug(TAG, "400 Invalid Alpaca request: %s %s", request->methodToString(), path.c_str());
            request->send(400, "text/plain", "Invalid Alpaca device type, device number, method or HTTP verb");
            return;
        case CaptivePortal::NotFound::SetupScreen:
            // On the hotspot every hostname resolves here; other sites' pages
            // go to the setup screen. Never over the home network.
            request->redirect(setupScreenUrl().c_str());
            return;
        case CaptivePortal::NotFound::ApiError:
            Logger::debug(TAG, "404 Not Found (API): %s", path.c_str());
            request->send(404, "application/json", "{\"error\":\"Not found\"}");
            return;
        case CaptivePortal::NotFound::FileMissing:
            // A file the app asked for that isn't there (e.g. /lang.json while
            // a language downloads): a plain 404, not the app's page.
            request->send(404, "text/plain", "Not found");
            return;
        case CaptivePortal::NotFound::AppPage:
            Logger::debug(TAG, "SPA fallback for: %s", path.c_str());
            request->send(LittleFS, "/index.html", "text/html");
            return;
        }
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
        capWebSocketClients(wsSensors);
        capWebSocketClients(wsStatus);
        rememberAlpacaConnections();
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
            Core::LocalClock clock;
            localClock(clock.time, clock.date);
            const SafetyStatus safety = getSafetyStatus();
            const Alpaca::ObservingConditionsSnapshot obs = buildAlpacaObservingConditionsSnapshot();
            const Core::NightState night = computeNight(getSensorSnapshot(), cfg);
            const Core::TestWording wording{pendingTest.level, pendingTest.sound, pendingTest.title, pendingTest.message};
            const Alerts::Alert test = Core::buildTestAlert(sample, wording, Core::AlertSources{cfg, obs, night, safety}, clock);
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
        // ... and at once when the recent alerts or the settings change.
        const uint32_t alertsRevision = alertDispatcher ? alertDispatcher->recentRevision() : 0;
        const bool changed = alertsRevision != lastAlertsRevision || configRevision != lastConfigRevision;
        if (now - lastStatusBroadcast >= WS_STATUS_BROADCAST_INTERVAL_MS || changed)
        {
            broadcastStatusData();
            lastStatusBroadcast = now;
            lastAlertsRevision = alertsRevision;
            lastConfigRevision = configRevision;
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
                    if (!CaptivePortal::probeOpensSetup(arrivedViaHotspot(request)))
                    {
                        handleNotFound(request);
                        return;
                    }
                    Logger::info(TAG, "Captive check %s%s -> setup screen", request->host().c_str(), request->url().c_str());
                    request->redirect(setupScreenUrl().c_str());
                });
        }

        // Serve files from LittleFS
        server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html").setCacheControl("no-cache, no-store, must-revalidate");
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

} // namespace SQM
