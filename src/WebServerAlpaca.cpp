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
#include "ConnectionMemory.h"
#include <esp_attr.h>
#include <Preferences.h>

extern uint32_t bootCount;

// ASCOM Alpaca: the device API, management API and setup pages, routed through lib/AlpacaLogic.

// Which devices an imaging app has connected, in memory that survives a
// restart without power loss (spec 011 FR-008).
RTC_NOINIT_ATTR static SQM::Alpaca::ConnectionMemory alpacaConnectionMemory;

namespace SQM
{
    using namespace WebShared;

    namespace
    {
        Alpaca::ResetKind resetKind(esp_reset_reason_t reason)
        {
            switch (reason)
            {
            case ESP_RST_POWERON:
                return Alpaca::ResetKind::PowerOn;
            case ESP_RST_EXT:
                return Alpaca::ResetKind::External;
            case ESP_RST_SW:
                return Alpaca::ResetKind::Software;
            case ESP_RST_PANIC:
                return Alpaca::ResetKind::Panic;
            case ESP_RST_INT_WDT:
            case ESP_RST_TASK_WDT:
            case ESP_RST_WDT:
                return Alpaca::ResetKind::Watchdog;
            case ESP_RST_BROWNOUT:
                return Alpaca::ResetKind::Brownout;
            case ESP_RST_DEEPSLEEP:
                return Alpaca::ResetKind::DeepSleep;
            default:
                return Alpaca::ResetKind::Unknown;
            }
        }
    } // namespace

    void WebServer::restoreAlpacaConnections()
    {
        bool connected[Alpaca::DEVICE_COUNT] = {};
        if (Alpaca::keepsConnections(resetKind(esp_reset_reason())) && Alpaca::recallConnections(alpacaConnectionMemory, connected) &&
            (connected[0] || connected[1]))
        {
            // N.I.N.A. had the device connected and the device restarted on its
            // own: keep answering Connected = true so it doesn't see a drop.
            alpacaRouter.restoreConnections(connected);
            alpacaConnectionsRestored = true;
            Logger::info(TAG, "Kept imaging-app connections across the restart");
        }
        rememberAlpacaConnections();
    }

    void WebServer::rememberAlpacaConnections()
    {
        bool connected[Alpaca::DEVICE_COUNT] = {};
        alpacaRouter.connectedDevices(connected);
        alpacaConnectionMemory = Alpaca::rememberConnections(connected);
    }

    Alpaca::ObservingConditionsSnapshot WebServer::buildAlpacaObservingConditionsSnapshot() const
    {
        return Core::observingConditions(getSensorSnapshot(), getConfigCallback(), millis());
    }

    void WebServer::setupAlpacaRoutes()
    {
        // --- Setup pages ---
        // NINA's (and other clients') "Setup" button opens
        // /setup/v1/<devicetype>/<n>/setup in a browser. All of the device's
        // settings live on the SPA's Settings page, so send every setup URL
        // there. server.on() also matches "/setup/..." as a prefix in this
        // ESPAsyncWebServer version, so this single route covers them all.
        server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request) { request->redirect("/settings?section=alpaca"); });

        // --- Management + device API ---
        // Everything is handled by Alpaca::Router (lib/AlpacaLogic), which the
        // native simulator ConformU tests in CI also runs. One handler per
        // prefix: registering ~50 routes separately cost ~10 KB of heap.
        server.on("/management", HTTP_GET, [this](AsyncWebServerRequest *request) { handleAlpacaRequest(request); });
        server.on("/api/v1", HTTP_ANY, [this](AsyncWebServerRequest *request) { handleAlpacaRequest(request); });
    }

    Alpaca::ServerIdentity WebServer::alpacaIdentity()
    {
        return {FIRMWARE_NAME, "SQMeter", FIRMWARE_VERSION, ESP.getEfuseMac()};
    }

    bool WebServer::AlpacaBackend::alpacaEnabled() const
    {
        return owner.getConfigCallback().alpaca.enabled;
    }
    bool WebServer::AlpacaBackend::isSafe() const
    {
        return owner.getSafetyStatus().isSafe;
    }
    Alpaca::ObservingConditionsSnapshot WebServer::AlpacaBackend::observingConditions() const
    {
        return owner.buildAlpacaObservingConditionsSnapshot();
    }
    std::string WebServer::AlpacaBackend::serverName() const
    {
        return owner.getConfigCallback().deviceName;
    }
    std::string WebServer::AlpacaBackend::location() const
    {
        return Core::alpacaLocation(owner.getSensorSnapshot(), owner.getConfigCallback());
    }

    // ISO 8601 UTC for DeviceState, or empty if the clock has never been set
    // (NTP/GPS) - a 1970 timestamp would be worse than none.
    std::string WebServer::AlpacaBackend::timestampUtc() const
    {
        return Core::isoUtc(static_cast<int64_t>(time(nullptr)));
    }

    void WebServer::handleAlpacaRequest(AsyncWebServerRequest *request)
    {
        Alpaca::Request alpaca;
        alpaca.get = request->method() == HTTP_GET;
        alpaca.put = request->method() == HTTP_PUT;
        alpaca.path = request->url().c_str();
        const size_t count = request->params();
        alpaca.params.reserve(count);
        for (size_t i = 0; i < count; ++i)
        {
            const AsyncWebParameter *param = request->getParam(i);
            if (param != nullptr && !param->isFile())
                alpaca.params.emplace_back(param->name().c_str(), param->value().c_str());
        }
        Alpaca::Response response;
        if (!alpacaRouter.handle(alpaca, response))
        {
            response.status = 400;
            response.contentType = "text/plain";
            response.body = "Invalid Alpaca device type, device number, method or HTTP verb";
        }
        request->send(response.status, response.contentType, response.body.c_str());
    }

} // namespace SQM
