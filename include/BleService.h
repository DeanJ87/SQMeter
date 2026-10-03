#pragma once

#include "AlertEngine.h"
#include "BleAlarm.h"
#include "BlePayloads.h"
#include <atomic>
#include <cstdint>
#include <string>

#ifndef SQM_ENABLE_BLE
#define SQM_ENABLE_BLE 0
#endif

namespace SQM
{

    struct BleAlarmStatus
    {
        bool serviceActive = false; // alarm/ack/heartbeat offered (passkey set)
        bool alarmActive = false;   // unacknowledged alarm
        uint32_t sequence = 0;
        uint32_t acknowledgedSequence = 0;
        int bondedPhones = 0;
    };

    // BLE GATT service mirroring the safety verdict, rain state, latest alert
    // and a sensor summary, plus the safe/rain bits in the advertisement.
    // With a pairing passkey set it also offers a phone alarm service:
    // alarm (indicate, repeats until acknowledged), ack (write) and heartbeat
    // - all requiring a bonded, encrypted, passkey-authenticated link.
    //
    // Only compiled into the esp32dev-ble build (NimBLE doesn't fit the
    // standard partition layout); elsewhere every method is a no-op and
    // available() is false.
    class BleService
    {
    public:
        static constexpr bool available() { return SQM_ENABLE_BLE != 0; }

        // BLE build with Bluetooth switched off: hand the controller's
        // reserved RAM back to the heap. Bluetooth can't start again until a
        // restart (turning it on already needs one).
        static void releaseControllerMemory();

        // Starts advertising. `passkey` empty = no alarm service. Safe to call
        // once; later calls are ignored.
        void begin(const std::string &deviceName, const std::string &passkey);
        bool isActive() const { return active; }
        uint8_t connectedClients() const;
        BleAlarmStatus alarmStatus() const;

        // Loop task only, once a second.
        void update(const Ble::State &state, const std::string &summaryJson);
        void publishAlert(const Alerts::Alert &alert);
        void raiseAlarm(uint32_t reasonFlags, uint32_t epochSeconds);
        void raiseInfo(uint32_t reasonFlags, uint32_t epochSeconds);

        // Loop task: applies acks received from a phone (or requested by the
        // web UI). Returns true, with the sequence number, when an active
        // alarm was acknowledged.
        bool processAcks(uint32_t &acknowledgedSeq, bool &fromPhone);

        // Any task: queue requests for the loop.
        void requestLocalAck() { localAckRequested.store(true); }
        void requestForgetBonds() { forgetBondsRequested.store(true); }

        // Called by the NimBLE host task when a phone writes the ack characteristic.
        void onPhoneAck(uint32_t seq) { pendingPhoneAck.store(seq); }

    private:
        bool active = false;
        bool alarmService = false;
        Ble::AlarmTracker alarm;
        std::atomic<uint32_t> pendingPhoneAck{0};
        std::atomic<bool> localAckRequested{false};
        std::atomic<bool> forgetBondsRequested{false};
#if SQM_ENABLE_BLE
        void sendAlarm(uint32_t nowMs);
        void *server = nullptr; // NimBLEServer*, kept opaque so this header doesn't pull in NimBLE
        void *safetyChar = nullptr;
        void *rainChar = nullptr;
        void *alertChar = nullptr;
        void *summaryChar = nullptr;
        void *alarmChar = nullptr;
        void *heartbeatChar = nullptr;
        std::string lastSafety;
        std::string lastRain;
        std::string lastAdvert;
        uint32_t lastSummaryAt = 0;
        uint32_t lastAdvertAt = 0;
        uint32_t lastHeartbeatAt = 0;
        uint32_t heartbeatSeq = 0;
#endif
    };

} // namespace SQM
