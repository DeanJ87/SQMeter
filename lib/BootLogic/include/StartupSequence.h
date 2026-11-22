#pragma once

#include <cstdint>
#include <optional>

namespace SQM
{
    namespace Boot
    {
        // What the device starts after its core (config, WiFi, web server and
        // the update endpoints) is up, in this order. A new firmware is
        // confirmed first, so a supply that sags while the rest starts can't
        // roll it back; then the sensors (rain first: it is the one safety
        // input that is never held back), then the network services, then
        // Bluetooth, the largest single start-up current, and last the full
        // WiFi transmit power.
        enum class Step : uint8_t
        {
            ConfirmImage,    // mark the running firmware valid (cancels the rollback)
            Rain,            // RG-15 UART
            SkySensors,      // TSL2591 (SQM, and the SQM-L on its own bus)
            Infrared,        // MLX90614
            Environment,     // BME280
            Gps,             // GPS UART
            Wind,            // anemometer interrupt and vane
            Battery,         // fuel gauge probe and the power policy
            Display,         // OLED
            Time,            // NTP
            Mqtt,            // broker connection
            Mdns,            // <hostname>.local
            AlpacaDiscovery, // Alpaca UDP discovery listener
            Bluetooth,       // NimBLE GATT service
            WifiFullPower,   // the normal WiFi transmit power again
            Count
        };

        constexpr uint8_t STEP_COUNT = static_cast<uint8_t>(Step::Count);

        // One bit per step, for the set of steps a device runs.
        constexpr uint32_t stepBit(Step step)
        {
            return 1UL << static_cast<uint8_t>(step);
        }
        constexpr uint32_t ALL_STEPS = (1UL << STEP_COUNT) - 1;

        // Pause after the firmware is confirmed (a flash write) before the
        // first sensor starts.
        constexpr uint32_t SETTLE_AFTER_CONFIRM_MS = 500;
        // Pause between one step and the next, so each start-up current peak
        // is over (and the supply's capacitors recharged) before the next.
        constexpr uint32_t STEP_GAP_MS = 250;
        // Longer pause before Bluetooth: its controller and radio start draw
        // the most, on top of WiFi.
        constexpr uint32_t BLUETOOTH_GAP_MS = 1000;

        // Pause before a step, counted from when the previous step ran.
        uint32_t gapBeforeMs(Step step);

        // Time from the core being up to the last of `steps` starting, from
        // the pauses alone (each step's own run time comes on top).
        uint32_t plannedMs(uint32_t steps);
        // The same up to and including `last` (e.g. the last sensor).
        uint32_t plannedMsUntil(uint32_t steps, Step last);

        // Hands out the start-up steps one at a time, each a gap after the
        // previous one ran. Steps not in the set are passed over without a
        // pause. The firmware confirmation and the full transmit power are
        // always in the set.
        class StartupSequence
        {
        public:
            explicit StartupSequence(uint32_t steps = ALL_STEPS);

            // The core is up: the confirmation is due at once.
            void start(uint32_t nowMs);
            // The step due now, at most one per call and each only once;
            // nothing before start() or once all have run.
            std::optional<Step> next(uint32_t nowMs);
            // The step handed out has finished (some block, e.g. the MQTT
            // connection): the next pause counts from now.
            void stepFinished(uint32_t nowMs) { lastRunAtMs = nowMs; }
            // Whether `step` has run or been passed over.
            bool done(Step step) const;
            bool finished() const { return cursor >= STEP_COUNT; }
            uint32_t steps() const { return enabled; }

        private:
            uint32_t enabled;
            uint8_t cursor = 0;
            bool started = false;
            uint32_t lastRunAtMs = 0;
            bool lastWasConfirm = false;
        };
    } // namespace Boot
} // namespace SQM
