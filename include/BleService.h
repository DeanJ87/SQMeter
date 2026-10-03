#pragma once

#include "AlertEngine.h"
#include "BlePayloads.h"
#include <cstdint>
#include <string>

#ifndef SQM_ENABLE_BLE
#define SQM_ENABLE_BLE 0
#endif

namespace SQM
{

    // Read-only BLE GATT service mirroring the safety verdict, rain state,
    // latest alert and a sensor summary, plus the safe/rain bits in the
    // advertisement. Only compiled into the esp32dev-ble build (NimBLE
    // doesn't fit the standard partition layout); elsewhere every method is
    // a no-op and available() is false.
    class BleService
    {
    public:
        static constexpr bool available() { return SQM_ENABLE_BLE != 0; }

        // Starts advertising. Safe to call once; later calls are ignored.
        void begin(const std::string &deviceName);
        bool isActive() const { return active; }
        uint8_t connectedClients() const;

        // Called once a second from the loop task.
        void update(const Ble::State &state, const std::string &summaryJson);
        void publishAlert(const Alerts::Alert &alert);

    private:
        bool active = false;
#if SQM_ENABLE_BLE
        void *server = nullptr; // NimBLEServer*, kept opaque so this header doesn't pull in NimBLE
        void *safetyChar = nullptr;
        void *rainChar = nullptr;
        void *alertChar = nullptr;
        void *summaryChar = nullptr;
        std::string lastSafety;
        std::string lastRain;
        std::string lastAdvert;
        uint32_t lastSummaryAt = 0;
        uint32_t lastAdvertAt = 0;
#endif
    };

} // namespace SQM
