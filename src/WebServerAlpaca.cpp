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

// ASCOM Alpaca: the device API, management API and setup pages, routed through lib/AlpacaLogic.

namespace SQM
{
    using namespace WebShared;

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
