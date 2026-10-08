#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace SQM
{
    // DNS for the setup hotspot: every name resolves to the device, so phones
    // and laptops find the captive portal. Kept free of Arduino for tests.
    namespace CaptiveDns
    {
        constexpr uint16_t PORT = 53;

        // Builds the reply to `query` in `out`. A (IPv4) questions get
        // `ipv4` (network byte order of a.b.c.d is {a,b,c,d}); any other type
        // (AAAA, HTTPS, ...) gets an empty NOERROR answer at once, so clients
        // fall back to IPv4 instead of waiting. Returns false for anything
        // that isn't a single-question standard query (no reply is sent).
        bool buildResponse(const uint8_t *query, size_t len, const uint8_t ipv4[4], std::vector<uint8_t> &out);
    } // namespace CaptiveDns
} // namespace SQM
