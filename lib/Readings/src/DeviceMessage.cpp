#include "Readings.h"

#include <ctime>

// Restart reason and boot time for MQTT <base>/device (specs/038-restart-reason-mqtt).

namespace SQM
{
    namespace Readings
    {
        const char *restartReasonName(int resetReason)
        {
            // In esp_reset_reason_t order (ESP-IDF 5.5).
            static const char *const NAMES[] = {
                "unknown",  // ESP_RST_UNKNOWN
                "poweron",  // ESP_RST_POWERON
                "external", // ESP_RST_EXT
                "software", // ESP_RST_SW
                "panic",    // ESP_RST_PANIC
                "watchdog", // ESP_RST_INT_WDT
                "watchdog", // ESP_RST_TASK_WDT
                "watchdog", // ESP_RST_WDT
                "deepsleep",
                "brownout",
                "sdio",
                "usb",
                "jtag",
                "efuse",
                "powerglitch",
                "cpulockup",
            };
            if (resetReason < 0 || resetReason >= static_cast<int>(sizeof(NAMES) / sizeof(NAMES[0])))
                return "unknown";
            return NAMES[resetReason];
        }

        bool bootTimeKnown(int64_t bootEpoch)
        {
            return bootEpoch >= 1704067200;
        } // 2024-01-01

        void writeDevice(JsonObject root, const DeviceInfo &info)
        {
            root["resetReason"] = restartReasonName(info.resetReason);
            root["resetReasonCode"] = info.resetReason;
            root["bootCount"] = info.bootCount;
            if (bootTimeKnown(info.bootEpoch))
            {
                const time_t t = static_cast<time_t>(info.bootEpoch);
                struct tm utc;
                gmtime_r(&t, &utc);
                char buffer[24];
                std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
                root["bootTime"] = buffer; // copied: a char array, not a const char *
            }
            else
                root["bootTime"] = nullptr;
        }
    } // namespace Readings
} // namespace SQM
