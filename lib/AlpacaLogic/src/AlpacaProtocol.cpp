#include "AlpacaProtocol.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace SQM
{
    namespace Alpaca
    {

        namespace
        {
            char asciiLower(char c)
            {
                return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
            }
        }

        bool paramNameEquals(const std::string &a, const std::string &b)
        {
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); ++i)
            {
                if (asciiLower(a[i]) != asciiLower(b[i]))
                    return false;
            }
            return true;
        }

        uint32_t parseClientTransactionId(const std::string &raw)
        {
            if (raw.empty() || raw.size() > 10)
                return 0;
            uint64_t value = 0;
            for (char c : raw)
            {
                if (c < '0' || c > '9')
                    return 0;
                value = value * 10 + static_cast<uint64_t>(c - '0');
            }
            return value > UINT32_MAX ? 0 : static_cast<uint32_t>(value);
        }

        bool parseAlpacaBool(const std::string &raw, bool &out)
        {
            if (paramNameEquals(raw, "true"))
            {
                out = true;
                return true;
            }
            if (paramNameEquals(raw, "false"))
            {
                out = false;
                return true;
            }
            return false;
        }

        bool parseAlpacaDouble(const std::string &raw, double &out)
        {
            if (raw.empty())
                return false;
            char *end = nullptr;
            const double value = std::strtod(raw.c_str(), &end);
            if (end == raw.c_str() || *end != '\0' || !std::isfinite(value))
                return false;
            out = value;
            return true;
        }

        std::string buildUniqueId(uint64_t mac48, const std::string &deviceTypeLower, uint32_t deviceNumber)
        {
            char macHex[13];
            std::snprintf(macHex, sizeof(macHex), "%012llx", static_cast<unsigned long long>(mac48 & 0xFFFFFFFFFFFFULL));
            return std::string("sqmeter-") + macHex + "-" + deviceTypeLower + "-" + std::to_string(deviceNumber);
        }

    } // namespace Alpaca
} // namespace SQM
