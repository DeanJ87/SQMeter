#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace SQM
{
    // IPv6 addresses, hosts and URLs (specs/015-ipv6-dual-stack): parsing,
    // RFC 5952 text, scope, the LAN-only rule for IPv6 peers, and the host and
    // URL forms settings accept. Kept free of Arduino and lwIP so the native
    // tests and the browser (web/src/lib/netAddress.ts, held to the same
    // answers by test/fixtures/net-address/cases.json) agree.
    namespace Net
    {
        using Ipv6 = std::array<uint8_t, 16>;

        enum class Scope
        {
            Unspecified,
            Loopback,
            Multicast,
            LinkLocal,
            UniqueLocal,
            Global,
        };

        // Text form without brackets or a zone ("fd00::10", "::ffff:10.0.0.1").
        bool parseIpv6(const std::string &text, Ipv6 &out);
        // RFC 5952: lower case, longest zero run (2+ groups) as "::".
        std::string formatIpv6(const Ipv6 &address);
        Scope scopeOf(const Ipv6 &address);
        // "link-local", "unique-local", "global", ... as reported in /api/status.
        const char *scopeName(Scope scope);
        bool isIpv4Mapped(const Ipv6 &address);

        // FR-011: an IPv6 peer is accepted when it's loopback, link-local, an
        // IPv4-mapped address (IPv4 rules apply), or in the same /64 as one of
        // the device's own unique-local or global addresses.
        bool allowedPeer(const Ipv6 &peer, const std::vector<Ipv6> &own);

        // A host as typed in settings: a name, an IPv4 address, or an IPv6
        // address bare ("fd00::10") or in brackets, optionally with a port
        // ("[fd00::10]:1883").
        struct Host
        {
            std::string name; // without brackets
            bool ipv6 = false;
            uint16_t port = 0; // 0: none given
        };

        enum class HostError
        {
            None,
            Empty,
            BadIpv6,
            NeedsBrackets,
            PortInField,
            HasZone,
            BadPort,
            Spaces,
        };

        HostError parseHost(const std::string &text, Host &out);
        // The text shown for a HostError (nullptr for None).
        const char *hostErrorText(HostError error);

        struct HttpUrl
        {
            bool https = false;
            Host host;
            uint16_t port = 0; // the effective port (80/443 unless given)
            std::string path;  // starts with "/" ("/" when none given)
        };

        enum class UrlError
        {
            None,
            Scheme,
            Host,
            HttpsIpv6,
        };

        // http(s)://host[:port][/path], IPv6 hosts in brackets. On UrlError::Host,
        // `hostError` says why.
        UrlError parseHttpUrl(const std::string &text, HttpUrl &out, HostError *hostError = nullptr);
        std::string urlErrorText(UrlError error, HostError hostError);

        // "fd00::10" -> "[fd00::10]", names and IPv4 unchanged (for URLs and Host headers).
        std::string hostForUrl(const Host &host);
    } // namespace Net
} // namespace SQM
