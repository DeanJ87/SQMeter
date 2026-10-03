#pragma once

#include <cstdint>
#include <string>

namespace SQM
{
    namespace Alpaca
    {

        // Alpaca parameter names (ClientTransactionID, ClientID, Connected,
        // ...) are case-insensitive per the ASCOM Alpaca API spec, unlike
        // the lowercase-only URL paths. ASCII-only comparison is sufficient
        // since every Alpaca parameter name is ASCII.
        bool paramNameEquals(const std::string &a, const std::string &b);

        // ClientTransactionID is a uint32. Anything missing, non-numeric,
        // negative or out of range is treated as 0, which the spec defines
        // as "no transaction ID supplied" - never an error.
        uint32_t parseClientTransactionId(const std::string &raw);

        // Parses an Alpaca boolean parameter value ("true"/"false",
        // case-insensitive). Returns false on anything else so the caller
        // can reply with InvalidValue.
        bool parseAlpacaBool(const std::string &raw, bool &out);

        // Builds a per-device UniqueID from the chip's 48-bit MAC, e.g.
        // "sqmeter-a1b2c3d4e5f6-observingconditions-0", so two SQMeters on
        // the same network don't advertise colliding IDs.
        std::string buildUniqueId(uint64_t mac48, const std::string &deviceTypeLower, uint32_t deviceNumber);

    } // namespace Alpaca
} // namespace SQM
