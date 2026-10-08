#include "BlePayloads.h"

#include <algorithm>
#include <cmath>

namespace SQM
{
    namespace Ble
    {

        namespace
        {
            void putU16(std::string &out, uint16_t value)
            {
                out += static_cast<char>(value & 0xFF);
                out += static_cast<char>((value >> 8) & 0xFF);
            }

            void putU32(std::string &out, uint32_t value)
            {
                for (int shift = 0; shift < 32; shift += 8)
                    out += static_cast<char>((value >> shift) & 0xFF);
            }

            uint16_t scaled(float value, float scale, uint16_t max)
            {
                if (!std::isfinite(value) || value <= 0.0f)
                    return 0;
                const float raw = std::round(value * scale);
                return raw >= static_cast<float>(max) ? max : static_cast<uint16_t>(raw);
            }
        } // namespace

        uint8_t stateFlags(const State &state)
        {
            uint8_t flags = 0;
            if (state.safetyKnown)
                flags |= FLAG_SAFETY_KNOWN;
            if (state.safetyKnown && state.isSafe)
                flags |= FLAG_SAFE;
            if (state.rainEnabled)
            {
                flags |= FLAG_RAIN_SENSOR;
                if (state.raining)
                    flags |= FLAG_RAINING;
                if (state.rainHealthy)
                    flags |= FLAG_RAIN_HEALTHY;
            }
            return flags;
        }

        std::string encodeSafety(const State &state)
        {
            std::string out;
            out += static_cast<char>(state.safetyKnown && state.isSafe ? 1 : 0);
            out += static_cast<char>(state.safetyKnown && state.rawSafe ? 1 : 0);
            putU32(out, state.reasonFlags);
            return out;
        }

        std::string encodeRain(const State &state)
        {
            std::string out;
            out += static_cast<char>(stateFlags(state) & (FLAG_RAIN_SENSOR | FLAG_RAINING | FLAG_RAIN_HEALTHY));
            putU16(out, state.rainEnabled ? scaled(state.rainRateMmPerHour, 100.0f, 0xFFFE) : 0);
            return out;
        }

        std::string encodeAdvertisement(const State &state)
        {
            std::string out;
            putU16(out, MANUFACTURER_ID);
            out += 'S';
            out += 'Q';
            out += static_cast<char>(ADVERT_VERSION);
            out += static_cast<char>(stateFlags(state));
            putU16(out, state.sqmValid ? scaled(state.sqm, 100.0f, 0xFFFE) : 0xFFFF);
            return out;
        }

    } // namespace Ble
} // namespace SQM
