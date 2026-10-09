#pragma once

#include <cstdint>
#include <string>

namespace SQM
{
    namespace Ble
    {

        // GATT layout - documented in docs/user-guide/ble.md. All multi-byte
        // integers are little-endian.
        constexpr const char *SERVICE_UUID = "c5a10000-7d1e-4b8a-9f3c-2e5d6a7b8c90";
        constexpr const char *SAFETY_CHAR_UUID = "c5a10001-7d1e-4b8a-9f3c-2e5d6a7b8c90";
        constexpr const char *RAIN_CHAR_UUID = "c5a10002-7d1e-4b8a-9f3c-2e5d6a7b8c90";
        constexpr const char *ALERT_CHAR_UUID = "c5a10003-7d1e-4b8a-9f3c-2e5d6a7b8c90";
        constexpr const char *SUMMARY_CHAR_UUID = "c5a10004-7d1e-4b8a-9f3c-2e5d6a7b8c90";

        // Bluetooth SIG "reserved for testing" company ID, followed by an
        // "SQ" magic so scanners (e.g. a Home Assistant BLE proxy) can
        // recognise SQMeter advertisements without connecting.
        constexpr uint16_t MANUFACTURER_ID = 0xFFFF;
        constexpr uint8_t ADVERT_VERSION = 1;

        enum StateFlag : uint8_t
        {
            FlagSafe = 1u << 0,
            FlagRaining = 1u << 1,
            FlagRainSensor = 1u << 2, // rain sensor enabled
            FlagSafetyKnown = 1u << 3,
            FlagRainHealthy = 1u << 4,
        };

        struct State
        {
            bool safetyKnown = false;
            bool isSafe = false;
            bool rawSafe = false;
            uint32_t reasonFlags = 0;
            bool rainEnabled = false;
            bool rainHealthy = false;
            bool raining = false;
            float rainRateMmPerHour = 0.0f;
            bool sqmValid = false;
            float sqm = 0.0f;
        };

        uint8_t stateFlags(const State &state);

        // Safety characteristic: [isSafe u8][rawSafe u8][reasonFlags u32] (6 bytes)
        std::string encodeSafety(const State &state);

        // Rain characteristic: [flags u8][rate u16, 0.01 mm/h] (3 bytes)
        std::string encodeRain(const State &state);

        // Manufacturer-specific advertising data:
        // [company u16]['S']['Q'][version u8][flags u8][sqm u16, 0.01 mag/arcsec², 0xFFFF = n/a] (8 bytes)
        std::string encodeAdvertisement(const State &state);

    } // namespace Ble
} // namespace SQM
