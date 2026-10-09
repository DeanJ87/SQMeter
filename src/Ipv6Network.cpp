#include "Ipv6Network.h"

#include "Logger.h"
#include "NetAddress.h"
#include "WiFiManager.h"

#include <cstring>
#include <lwip/mld6.h>
#include <lwip/tcpip.h>

namespace SQM
{
    namespace Ipv6Network
    {
        namespace
        {
            constexpr const char *TAG = "IPv6";

            bool isZero(const Net::Ipv6 &address)
            {
                for (uint8_t b : address)
                {
                    if (b != 0)
                        return false;
                }
                return true;
            }

            // MLD state lives in the TCP/IP task; join from there.
            void joinOnTcpipTask(void *arg)
            {
                auto *joined = static_cast<volatile bool *>(arg);
                Net::Ipv6 group{};
                Net::parseIpv6(ALPACA_DISCOVERY_GROUP, group);
                ip6_addr_t lwipGroup{};
                memcpy(lwipGroup.addr, group.data(), group.size());
                ip6_addr_clear_zone(&lwipGroup);
                // Any interface with IPv6 - in practice the station (the setup hotspot has none).
                *joined = mld6_joingroup(IP6_ADDR_ANY6, &lwipGroup) == ERR_OK;
            }
        } // namespace

        void addLanOnlyMiddleware(AsyncWebServer &server)
        {
            server.addMiddleware(
                [](AsyncWebServerRequest *request, ArMiddlewareNext next)
                {
                    AsyncClient *client = request->client();
                    if (client == nullptr)
                        return next();
                    const ip6_addr_t remote = client->getRemoteAddress6();
                    Net::Ipv6 peer{};
                    memcpy(peer.data(), remote.addr, peer.size());
                    // IPv4 connections report no IPv6 address: unchanged.
                    if (isZero(peer) || Net::allowedPeer(peer, WiFiManager::ipv6Addresses()))
                        return next();
                    Logger::warn(
                        TAG, "Refused %s from %s (outside the local network)", request->url().c_str(), Net::formatIpv6(peer).c_str());
                    request->send(403, "text/plain", "IPv6 requests are only accepted from the local network");
                });
        }

        bool joinAlpacaDiscoveryGroup()
        {
            if (!WiFiManager::ipv6Running() || WiFiManager::ipv6Addresses().empty())
                return false;
            static volatile bool joined = false;
            if (tcpip_callback(joinOnTcpipTask, const_cast<bool *>(&joined)) != ERR_OK)
                return false;
            // The callback runs within a tick or two; the caller retries if not yet.
            delay(20);
            if (joined)
                Logger::info(TAG, "Alpaca discovery listening on %s", ALPACA_DISCOVERY_GROUP);
            return joined;
        }

        void appendStatus(JsonObject wifi)
        {
            JsonObject ipv6 = wifi.createNestedObject("ipv6");
            ipv6["enabled"] = WiFiManager::ipv6Running();
            JsonArray addresses = ipv6.createNestedArray("addresses");
            for (const Net::Ipv6 &address : WiFiManager::ipv6Addresses())
            {
                JsonObject entry = addresses.createNestedObject();
                entry["address"] = Net::formatIpv6(address);
                entry["scope"] = Net::scopeName(Net::scopeOf(address));
            }
        }
    } // namespace Ipv6Network
} // namespace SQM
