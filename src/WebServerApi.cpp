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
#include "Ipv6Network.h"
#include "DualStackClient.h"
#include "NetAddress.h"
#include "WiFiManager.h"
#include "HeapTrace.h"
#include "SunPosition.h"
#include "SafetyHistory.h"
#include <Preferences.h>

extern uint32_t bootCount;

// The REST API: settings, status, restart, WiFi, sensor actions and the MQTT test.

namespace SQM
{
    using namespace WebShared;

    void WebServer::setupAPIRoutes()
    {
        setupReadingRoutes();
        setupSettingsRoutes();
        setupNetworkRoutes();
    }

    void WebServer::setupReadingRoutes()
    {
        server.on("/api/status", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetStatus(request); });
        server.on("/api/sensors", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetSensors(request); });

        // Plain-text "1" (safe) or "0" (unsafe) - the SafetyMonitor verdict
        // for scripts and loggers.
        server.on(
            "/api/safe",
            HTTP_GET,
            [this](AsyncWebServerRequest *request) { request->send(200, "text/plain", getSafetyStatus().isSafe ? "1" : "0"); });

        // Before /api/safety, which would otherwise match this path too.
        server.on("/api/safety/history", HTTP_GET, [](AsyncWebServerRequest *request) { handleSafetyHistory(request); });
        // What's actually in effect (specs/020-settings-dependencies).
        server.on("/api/settings/effective", HTTP_GET, [this](AsyncWebServerRequest *request) { handleSettingsEffective(request); });
        server.on("/api/safety", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetSafety(request); });
    }

    void WebServer::setupSettingsRoutes()
    {
        server.on(
            "/api/sensors/tsl2591/calibrate-dark",
            HTTP_POST,
            [this](AsyncWebServerRequest *request) { handleTSL2591DarkCalibration(request); });

        server.on("/api/config", HTTP_GET, [this](AsyncWebServerRequest *request) { handleGetConfig(request); });
        AsyncCallbackJsonWebHandler *configHandler = new AsyncCallbackJsonWebHandler(
            "/api/config",
            [this](AsyncWebServerRequest *request, JsonVariant &json) { handleSetConfig(request, json); },
            CONFIG_JSON_BUFFER_SIZE);
        configHandler->setMethod(HTTP_POST | HTTP_PUT);
        server.addHandler(configHandler);

        server.on("/api/restart", HTTP_POST, [this](AsyncWebServerRequest *request) { handleRestart(request); });
    }

    void WebServer::setupNetworkRoutes()
    {
        server.on("/api/wifi/scan", HTTP_GET, [this](AsyncWebServerRequest *request) { handleWiFiScan(request); });

        server.on("/api/sensors/rg15/test", HTTP_POST, [this](AsyncWebServerRequest *request) { handleRG15Test(request); });
        server.on("/api/sensors/rg15/reset-total", HTTP_POST, [this](AsyncWebServerRequest *request) { handleRG15ResetTotal(request); });
        server.on("/api/sensors/rg15/reboot", HTTP_POST, [this](AsyncWebServerRequest *request) { handleRG15Reboot(request); });

        server.addHandler(new AsyncCallbackJsonWebHandler(
            "/api/mqtt/test", [this](AsyncWebServerRequest *request, JsonVariant &json) { handleMQTTTest(request, json); }));
        server.addHandler(new AsyncCallbackJsonWebHandler(
            "/api/wifi/connect", [this](AsyncWebServerRequest *request, JsonVariant &json) { handleWiFiConnect(request, json); }));
    }

    void WebServer::handleSafetyHistory(AsyncWebServerRequest *request)
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
    }

    void WebServer::handleSettingsEffective(AsyncWebServerRequest *request)
    {
        const Deps::Facts facts = settingsFacts(getSensorSnapshot());
        const std::vector<Deps::Entry> entries = Deps::evaluate(getConfigCallback(), facts);
        DynamicJsonDocument doc(Deps::reportCapacity(entries));
        Deps::writeReport(doc.to<JsonObject>(), entries, facts);
        std::string json;
        serializeJson(doc, json);
        request->send(200, "application/json", json.c_str());
    }

    void WebServer::handleGetSafety(AsyncWebServerRequest *request)
    {
        DynamicJsonDocument doc(1536);
        appendSafetyStatus(doc.to<JsonObject>());
        std::string json;
        serializeJson(doc, json);
        request->send(200, "application/json", json.c_str());
    }

    void WebServer::handleSetConfig(AsyncWebServerRequest *request, JsonVariant &json)
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
            request->send(400, "application/json", createErrorJson(reason.empty() ? "Invalid configuration" : reason.c_str()).c_str());
            return;
        }

        const std::string previousLanguage = currentConfig.language;
        if (!saveConfigCallback(*configOpt))
        {
            request->send(500, "application/json", createErrorJson("Failed to save configuration").c_str());
            return;
        }
        tslSensor.configureSkyMeasurement(configOpt->skyAveraging, configOpt->skyCalibration);
        if (previousLanguage != configOpt->language)
            languagePack->onLanguageChanged(configOpt->language);
        request->send(200, "application/json", "{\"success\":true}");
    }

    void WebServer::handleWiFiConnect(AsyncWebServerRequest *request, JsonVariant &json)
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
        const bool success = ok && reading.status == SensorStatus::Ok;

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

    namespace
    {
        // PubSubClient's state() after a failed connect.
        const char *mqttStateText(int state)
        {
            switch (state)
            {
            case -4:
                return "Connection timeout";
            case -3:
                return "Connection lost";
            case -2:
                return "Connect failed";
            case -1:
                return "Disconnected";
            case 1:
                return "Bad protocol";
            case 2:
                return "Bad client ID";
            case 3:
                return "Unavailable";
            case 4:
                return "Bad credentials - check username/password";
            case 5:
                return "Unauthorized";
            default:
                return "Unknown error";
            }
        }
    } // namespace

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

        // Try to connect with or without auth
        const bool connected = strlen(username) > 0 ? testMqtt.connect(clientId, username, password) : testMqtt.connect(clientId);

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

            response["error"] = mqttStateText(state);
            response["state"] = state;
        }

        String responseStr;
        serializeJson(response, responseStr);
        request->send(connected ? 200 : 502, "application/json", responseStr.c_str());
    }

} // namespace SQM
