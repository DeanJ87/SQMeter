#pragma once

#include <ArduinoJson.h>
#include <ESPAsyncWebServer.h>

namespace SQM
{
    // IPv6 on the joined network (specs/015-ipv6-dual-stack): the LAN-only
    // rule for inbound IPv6, Alpaca discovery on the IPv6 group, and the
    // addresses in /api/status. Decisions are in lib/NetAddress.
    namespace Ipv6Network
    {
        // ff12::a1:2345 - the Alpaca IPv6 discovery group (link scope).
        constexpr const char *ALPACA_DISCOVERY_GROUP = "ff12::a1:2345";

        // FR-011: refuses (403) IPv6 requests from outside link-local and the
        // device's own /64 prefixes, for every handler and WebSocket upgrade.
        void addLanOnlyMiddleware(AsyncWebServer &server);

        // Joins the Alpaca discovery group once the station has an IPv6
        // address; true once joined (call until it is).
        bool joinAlpacaDiscoveryGroup();

        // wifi.ipv6 = {enabled, addresses: [{address, scope}]}.
        void appendStatus(JsonObject wifi);
    } // namespace Ipv6Network
} // namespace SQM
