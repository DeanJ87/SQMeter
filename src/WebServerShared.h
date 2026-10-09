#pragma once

// Constants shared by the WebServer sources (WebServer*.cpp).

#include <cstddef>
#include <cstdint>

namespace SQM::WebShared
{
    constexpr size_t CONFIG_JSON_BUFFER_SIZE = 12288; // full config incl. custom alert texts

    constexpr const char *ARMED_NVS_NAMESPACE = "sqm-alerts";

    // Test sends that only ring paired phones (no push channel enabled).
    constexpr uint8_t BLE_ONLY_TEST = 0x80;
} // namespace SQM::WebShared
