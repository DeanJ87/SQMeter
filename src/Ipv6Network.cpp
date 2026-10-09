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
            constexpr char REFUSED[] = "HTTP/1.1 403 Forbidden\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\n"
                                       "IPv6 requests are only accepted from the local network";

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

        void listenIpv6(AsyncWebServer &server, uint16_t port)
        {
            // Lives as long as the web server (the device's lifetime).
            static AsyncServer *listener = nullptr;
            if (listener != nullptr)
                return;
            listener = new AsyncServer(IPv6Address(), port);
            // As AsyncWebServer does for its own listener.
            listener->onClient(
                [](void *arg, AsyncClient *client)
                {
                    if (client == nullptr)
                        return;
                    // Only IPv6 connections reach this listener.
                    const ip6_addr_t remote = client->getRemoteAddress6();
                    Net::Ipv6 peer{};
                    memcpy(peer.data(), remote.addr, peer.size());
                    if (!Net::allowedPeer(peer, WiFiManager::ipv6Addresses()))
                    {
                        Logger::warn(TAG, "Refused a connection from %s (outside the local network)", Net::formatIpv6(peer).c_str());
                        client->write(REFUSED, sizeof(REFUSED) - 1);
                        client->close();
                        return;
                    }
                    client->setRxTimeout(3);
                    if (new AsyncWebServerRequest(static_cast<AsyncWebServer *>(arg), client) == nullptr)
                    {
                        client->abort();
                        delete client;
                    }
                },
                &server);
            listener->begin();
            Logger::info(TAG, "Web server listening on IPv6 port %u", port);
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
