#pragma once

#include <cstdint>
#include <cstddef>
#include <string>

namespace SQM
{
    namespace Alpaca
    {

        constexpr uint16_t DISCOVERY_UDP_PORT = 32227;

        // Validates an incoming UDP payload against the Alpaca discovery
        // protocol: a valid request is the ASCII string "alpacadiscovery1"
        // (case-sensitive, per the ASCOM Alpaca discovery spec), optionally
        // with trailing data that's ignored. No networking - just parsing,
        // so it's unit-testable without a socket.
        bool isValidDiscoveryRequest(const uint8_t *data, size_t len);

        // Builds the JSON discovery response body: {"AlpacaPort": <port>}
        std::string buildDiscoveryResponse(uint16_t alpacaPort);

    } // namespace Alpaca
} // namespace SQM
