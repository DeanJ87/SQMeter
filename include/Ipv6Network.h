#pragma once

#include <ArduinoJson.h>
#include <AsyncUDP.h>
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

        // The web server's own listener is IPv4-only on this platform's
        // AsyncTCP (research R2): a second, IPv6-only listener on the same
        // port hands its connections to the same server. FR-011: a peer
        // outside link-local and the device's own /64 prefixes gets a 403 and
        // the connection is closed before any request is read - so not even an
        // upload handler (which writes as data arrives) runs for it.
        void listenIpv6(AsyncWebServer &server, uint16_t port);

        // Joins the Alpaca discovery group once the station has an IPv6
        // address; true once joined (call until it is).
        bool joinAlpacaDiscoveryGroup();

        // FR-011 for Alpaca discovery: IPv4 always; IPv6 only from the local network.
        bool allowedDiscoveryPeer(AsyncUDPPacket &packet);

        // wifi.ipv6 = {enabled, addresses: [{address, scope}]}.
        void appendStatus(JsonObject wifi);
    } // namespace Ipv6Network
} // namespace SQM
