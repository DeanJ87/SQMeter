#include "WebServer.h"
#include "DeviceCore.h"
#include "Ipv6Network.h"
#include "TcpDiagnostics.h"
#include "WebHealthJson.h"

#include <ctime>

// Web-stall diagnostics: since the Arduino-ESP32 3.x port the web server
// sometimes answers nobody for 15-25 s. The loop task samples the TCP
// connections and the listener once a second (lib/WebHealth decides what is a
// stall); /api/status reports them as connections.tcp and connections.stalls.

namespace SQM
{
    void WebServer::sampleWebHealth(uint32_t now)
    {
        if (now - lastWebHealthSample < WEB_HEALTH_SAMPLE_MS)
            return;
        lastWebHealthSample = now;

        WebHealth::Sample sample;
        sample.nowMs = now;
        const time_t wallClock = time(nullptr);
        sample.epoch = wallClock >= Core::CLOCK_VALID_EPOCH ? static_cast<uint32_t>(wallClock) : 0;
        sample.accepted = Ipv6Network::acceptedConnections();
        sample.alpacaRequests = alpacaRouter.activity(Alpaca::Device::SafetyMonitor).requests +
                                alpacaRouter.activity(Alpaca::Device::ObservingConditions).requests;
        sample.tcp = TcpDiagnostics::count();
        sample.freeHeap = ESP.getFreeHeap();
        sample.asyncTcp = TcpDiagnostics::asyncTcpActivity(sample.asyncTcpStackFree);

        const std::lock_guard<std::mutex> lock(webHealthLock);
        webStalls.update(sample);
    }

    void WebServer::appendWebHealth(JsonObject connections) const
    {
        const WebHealth::TcpCounts now = TcpDiagnostics::count();
        const std::lock_guard<std::mutex> lock(webHealthLock);
        WebHealth::writeTcp(connections.createNestedObject("tcp"), now, Ipv6Network::acceptedConnections(), webStalls.peak());
        WebHealth::writeStalls(connections.createNestedObject("stalls"), webStalls);
    }
} // namespace SQM
