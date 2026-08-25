#include "AlpacaDiscovery.h"
#include <cstring>

namespace SQM
{
    namespace Alpaca
    {
        namespace
        {
            constexpr char DISCOVERY_MAGIC[] = "alpacadiscovery1";
            constexpr size_t DISCOVERY_MAGIC_LEN = sizeof(DISCOVERY_MAGIC) - 1; // exclude trailing NUL
        }

        bool isValidDiscoveryRequest(const uint8_t *data, size_t len)
        {
            if (data == nullptr || len < DISCOVERY_MAGIC_LEN)
                return false;

            return std::memcmp(data, DISCOVERY_MAGIC, DISCOVERY_MAGIC_LEN) == 0;
        }

        std::string buildDiscoveryResponse(uint16_t alpacaPort)
        {
            return "{\"AlpacaPort\":" + std::to_string(alpacaPort) + "}";
        }

    } // namespace Alpaca
} // namespace SQM
