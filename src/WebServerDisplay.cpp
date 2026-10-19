#include "WebServer.h"

#include "OledDisplay.h"

// The optional OLED (specs/031-oled-display): its view, its /api/status
// section and the wake/blank routes. The handlers only set requests the loop
// task applies (constitution IV).

namespace SQM
{
    void WebServer::feedDisplay(const Core::DisplayNetwork &network)
    {
        if (display == nullptr || !display->enabled())
            return;
        const Config &cfg = getConfigCallback();
        const SensorSnapshot snapshot = getSensorSnapshot();
        const Readings::Snapshot readings = Core::buildReadings(snapshot, cfg, millis(), static_cast<int64_t>(time(nullptr)));
        const Core::NightState nightState = computeNight(snapshot, cfg);
        display->setView(Core::displayView(snapshot, readings, getSafetyStatus(), cfg, network), {nightState.known, nightState.isNight});
    }

    void WebServer::appendDisplayStatus(JsonDocument &doc) const
    {
        const Core::DisplayStatus state = display != nullptr ? display->status() : Core::DisplayStatus{};
        Core::writeDisplayStatus(doc.as<JsonObject>(), getConfigCallback(), state);
    }

    void WebServer::setupDisplayRoutes()
    {
        server.on("/api/display/wake", HTTP_POST, [this](AsyncWebServerRequest *request) { handleDisplayRequest(request, true); });
        server.on("/api/display/blank", HTTP_POST, [this](AsyncWebServerRequest *request) { handleDisplayRequest(request, false); });
    }

    void WebServer::handleDisplayRequest(AsyncWebServerRequest *request, bool wake)
    {
        if (!requireAuth(request))
            return;
        if (display == nullptr || !getConfigCallback().display.enabled)
        {
            request->send(409, "application/json", createErrorJson("Display is off in settings").c_str());
            return;
        }
        if (!display->detected())
        {
            request->send(409, "application/json", createErrorJson("Display not detected").c_str());
            return;
        }
        if (display->forcedOff())
        {
            request->send(409, "application/json", createErrorJson("Display is off to save power").c_str());
            return;
        }
        if (wake)
            display->requestWake();
        else
            display->requestBlank();
        request->send(200, "application/json", "{\"success\":true}");
    }
} // namespace SQM
