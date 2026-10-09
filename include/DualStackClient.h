#pragma once

#include <WiFiClient.h>
#include <lwip/sockets.h>

namespace SQM
{
    // A WiFiClient that also connects to IPv6 literals and IPv6-only names
    // (specs/015-ipv6-dual-stack). It resolves with getaddrinfo; lwIP returns
    // the IPv4 address when a name has both, so dual-stack servers keep using
    // IPv4 and broken upstream IPv6 never delays a connection (FR-008). Used
    // for plain-TCP outbound connections (MQTT, http webhooks). Since
    // Arduino-ESP32 3.x (spec 027) the TLS clients and SNTP resolve names the
    // same way on their own.
    class DualStackClient : public WiFiClient
    {
    public:
        using WiFiClient::connect;
        int connect(const char *host, uint16_t port) override;
        int connect(const char *host, uint16_t port, int32_t timeoutMs) override;

    private:
        static constexpr int32_t DEFAULT_TIMEOUT_MS = 3000;
        int connectIpv6(const ::sockaddr_in6 &address, int32_t timeoutMs);
    };
} // namespace SQM
