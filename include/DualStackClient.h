#pragma once

#include <WiFiClient.h>
#include <lwip/sockets.h>

namespace SQM
{
    // A WiFiClient that also connects over IPv6 (specs/015-ipv6-dual-stack).
    // Arduino-ESP32 2.0.17's WiFiClient resolves names IPv4-only and opens
    // IPv4 sockets; this one resolves with getaddrinfo, so an IPv6 literal or
    // a name with only an IPv6 address works. lwIP returns the IPv4 address
    // when a name has both, so dual-stack servers keep using IPv4 and broken
    // upstream IPv6 never delays a connection (FR-008). Used for plain-TCP
    // outbound connections (MQTT, http webhooks); TLS stays IPv4 on this
    // platform (research R6).
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
