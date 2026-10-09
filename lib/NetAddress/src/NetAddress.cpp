#include "NetAddress.h"

#include <algorithm>
#include <cctype>
#include <cstdio>

namespace SQM
{
    namespace Net
    {
        namespace
        {
            int hexValue(char c)
            {
                if (c >= '0' && c <= '9')
                    return c - '0';
                if (c >= 'a' && c <= 'f')
                    return c - 'a' + 10;
                if (c >= 'A' && c <= 'F')
                    return c - 'A' + 10;
                return -1;
            }

            // Dotted IPv4 into 4 bytes; each part 0-255, no leading "+" or blanks.
            bool parseIpv4(const std::string &text, uint8_t *out)
            {
                int part = 0;
                size_t i = 0;
                while (part < 4)
                {
                    if (i >= text.size() || !std::isdigit(static_cast<unsigned char>(text[i])))
                        return false;
                    unsigned value = 0;
                    size_t digits = 0;
                    while (i < text.size() && std::isdigit(static_cast<unsigned char>(text[i])))
                    {
                        value = value * 10 + static_cast<unsigned>(text[i] - '0');
                        if (++digits > 3 || value > 255)
                            return false;
                        ++i;
                    }
                    out[part++] = static_cast<uint8_t>(value);
                    if (part < 4)
                    {
                        if (i >= text.size() || text[i] != '.')
                            return false;
                        ++i;
                    }
                }
                return i == text.size();
            }

            // Splits on ':' keeping empty fields ("a::b" -> "a", "", "b").
            std::vector<std::string> splitColons(const std::string &text)
            {
                std::vector<std::string> parts(1);
                for (char c : text)
                {
                    if (c == ':')
                        parts.emplace_back();
                    else
                        parts.back() += c;
                }
                return parts;
            }

            bool parseGroup(const std::string &group, uint16_t &out)
            {
                if (group.empty() || group.size() > 4)
                    return false;
                unsigned value = 0;
                for (char c : group)
                {
                    const int digit = hexValue(c);
                    if (digit < 0)
                        return false;
                    value = value * 16 + static_cast<unsigned>(digit);
                }
                out = static_cast<uint16_t>(value);
                return true;
            }

            // The groups on one side of "::" (empty text is no groups).
            bool parseGroups(const std::string &text, bool allowIpv4Tail, std::vector<uint16_t> &out)
            {
                if (text.empty())
                    return true;
                const std::vector<std::string> parts = splitColons(text);
                for (size_t i = 0; i < parts.size(); ++i)
                {
                    const bool last = i + 1 == parts.size();
                    if (last && allowIpv4Tail && parts[i].find('.') != std::string::npos)
                    {
                        uint8_t v4[4];
                        if (!parseIpv4(parts[i], v4))
                            return false;
                        out.push_back(static_cast<uint16_t>(v4[0] << 8 | v4[1]));
                        out.push_back(static_cast<uint16_t>(v4[2] << 8 | v4[3]));
                        continue;
                    }
                    uint16_t group = 0;
                    if (!parseGroup(parts[i], group))
                        return false;
                    out.push_back(group);
                }
                return true;
            }

            bool parsePort(const std::string &text, uint16_t &out)
            {
                if (text.empty() || text.size() > 5)
                    return false;
                unsigned value = 0;
                for (char c : text)
                {
                    if (!std::isdigit(static_cast<unsigned char>(c)))
                        return false;
                    value = value * 10 + static_cast<unsigned>(c - '0');
                }
                if (value < 1 || value > 65535)
                    return false;
                out = static_cast<uint16_t>(value);
                return true;
            }

            bool samePrefix64(const Ipv6 &a, const Ipv6 &b)
            {
                for (size_t i = 0; i < 8; ++i)
                {
                    if (a[i] != b[i])
                        return false;
                }
                return true;
            }

            HostError parseBracketed(const std::string &text, Host &out)
            {
                const size_t close = text.find(']');
                if (close == std::string::npos)
                    return HostError::BadIpv6;
                const std::string inside = text.substr(1, close - 1);
                if (inside.find('%') != std::string::npos)
                    return HostError::HasZone;
                Ipv6 address{};
                if (!parseIpv6(inside, address))
                    return HostError::BadIpv6;
                const std::string rest = text.substr(close + 1);
                if (!rest.empty() && (rest[0] != ':' || !parsePort(rest.substr(1), out.port)))
                    return HostError::BadPort;
                out.name = inside;
                out.ipv6 = true;
                return HostError::None;
            }
        } // namespace

        bool parseIpv6(const std::string &text, Ipv6 &out)
        {
            if (text.empty() || text.size() > 45)
                return false;
            const size_t gap = text.find("::");
            if (gap != std::string::npos && text.find("::", gap + 1) != std::string::npos)
                return false;

            std::vector<uint16_t> head;
            std::vector<uint16_t> tail;
            if (gap == std::string::npos)
            {
                if (!parseGroups(text, true, head) || head.size() != 8)
                    return false;
            }
            else
            {
                const std::string left = text.substr(0, gap);
                const std::string right = text.substr(gap + 2);
                if (!parseGroups(left, false, head) || !parseGroups(right, true, tail))
                    return false;
                if (head.size() + tail.size() > 7)
                    return false;
            }

            std::array<uint16_t, 8> groups{};
            for (size_t i = 0; i < head.size(); ++i)
                groups[i] = head[i];
            for (size_t i = 0; i < tail.size(); ++i)
                groups[8 - tail.size() + i] = tail[i];
            for (size_t i = 0; i < 8; ++i)
            {
                out[i * 2] = static_cast<uint8_t>(groups[i] >> 8);
                out[i * 2 + 1] = static_cast<uint8_t>(groups[i] & 0xff);
            }
            return true;
        }

        std::string formatIpv6(const Ipv6 &address)
        {
            uint16_t groups[8];
            for (size_t i = 0; i < 8; ++i)
                groups[i] = static_cast<uint16_t>(address[i * 2] << 8 | address[i * 2 + 1]);

            // Longest run of zero groups, at least two long; the first one wins a tie.
            int bestStart = -1;
            int bestLength = 1;
            for (int i = 0; i < 8;)
            {
                if (groups[i] != 0)
                {
                    ++i;
                    continue;
                }
                int j = i;
                while (j < 8 && groups[j] == 0)
                    ++j;
                if (j - i > bestLength)
                {
                    bestStart = i;
                    bestLength = j - i;
                }
                i = j;
            }

            std::string text;
            char buffer[6];
            for (int i = 0; i < 8; ++i)
            {
                if (i == bestStart)
                {
                    text += "::";
                    i += bestLength - 1;
                    continue;
                }
                if (!text.empty() && text.back() != ':')
                    text += ':';
                std::snprintf(buffer, sizeof(buffer), "%x", groups[i]);
                text += buffer;
            }
            return text;
        }

        bool isIpv4Mapped(const Ipv6 &address)
        {
            for (size_t i = 0; i < 10; ++i)
            {
                if (address[i] != 0)
                    return false;
            }
            return address[10] == 0xff && address[11] == 0xff;
        }

        Scope scopeOf(const Ipv6 &address)
        {
            bool zero = true;
            for (size_t i = 0; i < 15; ++i)
                zero = zero && address[i] == 0;
            if (zero && address[15] == 0)
                return Scope::Unspecified;
            if (zero && address[15] == 1)
                return Scope::Loopback;
            if (address[0] == 0xff)
                return Scope::Multicast;
            if (address[0] == 0xfe && (address[1] & 0xc0) == 0x80)
                return Scope::LinkLocal;
            if ((address[0] & 0xfe) == 0xfc)
                return Scope::UniqueLocal;
            return Scope::Global;
        }

        const char *scopeName(Scope scope)
        {
            switch (scope)
            {
            case Scope::Unspecified:
                return "unspecified";
            case Scope::Loopback:
                return "loopback";
            case Scope::Multicast:
                return "multicast";
            case Scope::LinkLocal:
                return "link-local";
            case Scope::UniqueLocal:
                return "unique-local";
            case Scope::Global:
                return "global";
            }
            return "global";
        }

        bool allowedPeer(const Ipv6 &peer, const std::vector<Ipv6> &own)
        {
            if (isIpv4Mapped(peer))
                return true;
            const Scope scope = scopeOf(peer);
            if (scope == Scope::Loopback || scope == Scope::LinkLocal)
                return true;
            if (scope != Scope::UniqueLocal && scope != Scope::Global)
                return false;
            for (const Ipv6 &mine : own)
            {
                const Scope mineScope = scopeOf(mine);
                if ((mineScope == Scope::UniqueLocal || mineScope == Scope::Global) && samePrefix64(peer, mine))
                    return true;
            }
            return false;
        }

        HostError parseHost(const std::string &text, Host &out)
        {
            out = Host{};
            if (text.empty())
                return HostError::Empty;
            for (char c : text)
            {
                if (std::isspace(static_cast<unsigned char>(c)))
                    return HostError::Spaces;
            }
            if (text[0] == '[')
                return parseBracketed(text, out);
            if (text.find('%') != std::string::npos)
                return HostError::HasZone;

            const size_t colons = static_cast<size_t>(std::count(text.begin(), text.end(), ':'));
            if (colons == 0)
            {
                out.name = text;
                return HostError::None;
            }
            if (colons == 1)
            {
                // "name:1883" or "10.0.0.5:1883" - the port has its own field.
                return HostError::PortInField;
            }
            Ipv6 address{};
            if (parseIpv6(text, address))
            {
                out.name = text;
                out.ipv6 = true;
                return HostError::None;
            }
            // "fd00::10:1883"-like text that isn't an address: suggest brackets.
            const size_t last = text.rfind(':');
            uint16_t port = 0;
            Ipv6 withoutPort{};
            if (parsePort(text.substr(last + 1), port) && parseIpv6(text.substr(0, last), withoutPort))
                return HostError::NeedsBrackets;
            return HostError::BadIpv6;
        }

        const char *hostErrorText(HostError error)
        {
            switch (error)
            {
            case HostError::None:
                return nullptr;
            case HostError::Empty:
                return "Enter a host name or address";
            case HostError::BadIpv6:
                return "Not a valid IPv6 address";
            case HostError::NeedsBrackets:
                return "Put IPv6 addresses in brackets to add a port, e.g. [fd00::10]:1883";
            case HostError::PortInField:
                return "Put the port in the Port field";
            case HostError::HasZone:
                return "Leave out the %zone - the device has one network interface";
            case HostError::BadPort:
                return "Port must be 1-65535";
            case HostError::Spaces:
                return "Host can't contain spaces";
            }
            return nullptr;
        }

        UrlError parseHttpUrl(const std::string &text, HttpUrl &out, HostError *hostError)
        {
            out = HttpUrl{};
            if (hostError)
                *hostError = HostError::None;
            std::string rest;
            if (text.rfind("http://", 0) == 0)
            {
                rest = text.substr(7);
            }
            else if (text.rfind("https://", 0) == 0)
            {
                out.https = true;
                rest = text.substr(8);
            }
            else
            {
                return UrlError::Scheme;
            }

            const size_t slash = rest.find_first_of("/?#");
            const std::string authority = rest.substr(0, slash);
            out.path = slash == std::string::npos ? "/" : rest.substr(slash);
            if (out.path[0] != '/')
                out.path = "/" + out.path;

            HostError error = HostError::None;
            if (!authority.empty() && authority[0] == '[')
            {
                error = parseHost(authority, out.host);
            }
            else
            {
                // name[:port] or a.b.c.d[:port]; a second ':' means an unbracketed IPv6 address.
                const size_t colon = authority.find(':');
                const std::string name = authority.substr(0, colon);
                if (colon != std::string::npos && authority.find(':', colon + 1) != std::string::npos)
                    error = HostError::NeedsBrackets;
                else
                    error = parseHost(name, out.host);
                if (error == HostError::None && colon != std::string::npos && !parsePort(authority.substr(colon + 1), out.host.port))
                    error = HostError::BadPort;
            }
            if (error != HostError::None)
            {
                if (hostError)
                    *hostError = error;
                return UrlError::Host;
            }
            if (out.https && out.host.ipv6)
                return UrlError::HttpsIpv6;
            out.port = out.host.port != 0 ? out.host.port : (out.https ? 443 : 80);
            return UrlError::None;
        }

        std::string urlErrorText(UrlError error, HostError hostError)
        {
            switch (error)
            {
            case UrlError::None:
                return "";
            case UrlError::Scheme:
                return "URL must start with http:// or https://";
            case UrlError::Host:
            {
                const char *text = hostErrorText(hostError);
                return text ? text : "Not a valid URL";
            }
            case UrlError::HttpsIpv6:
                return "https to an IPv6 address isn't supported yet - use a host name, or http";
            }
            return "";
        }

        std::string hostForUrl(const Host &host)
        {
            return host.ipv6 ? "[" + host.name + "]" : host.name;
        }
    } // namespace Net
} // namespace SQM
