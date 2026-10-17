#pragma once

// Li-ion battery logic (specs/032-battery): MAX17048 fuel gauge decoding, the
// MCP73871 charger's status lines, time estimates and the power policy. Pure:
// no Arduino headers, so the firmware, the native tests and the browser demo
// share it.

#include <cstddef>
#include <cstdint>
#include <optional>

namespace SQM
{
    namespace Battery
    {
        // What the charger (or, without its pins, the gauge's rate) says.
        enum class ChargeState : uint8_t
        {
            Discharging,
            Charging,
            Charged,
            Fault,
            NoBattery,
            Unknown, // nothing known yet (no charger pins and no rate)
        };

        // JSON name ("discharging", "charging", "charged", "fault",
        // "no-battery"); nullptr for Unknown.
        const char *chargeStateName(ChargeState state);

        enum class ChargeFault : uint8_t
        {
            None,
            Temperature, // the charger's NTC window (the safety timer is off on Rev A)
        };

        // "temperature", or nullptr for None.
        const char *chargeFaultName(ChargeFault fault);

        // research D7: Normal -> Saving -> ShutdownPending -> deep sleep.
        enum class PowerState : uint8_t
        {
            Normal,
            Saving,
            ShutdownPending,
        };

        // "normal", "saving" or "shutdown" (status diagnostics).
        const char *powerStateName(PowerState state);

        // How often the gauge is read (FR-002: at most 10 s); power saving
        // slows it to battery.saveReadIntervalSeconds.
        constexpr uint32_t READ_INTERVAL_MS = 5000;

        // MAX17048 registers (research D1).
        constexpr uint8_t GAUGE_ADDRESS = 0x36;
        constexpr uint8_t REG_VCELL = 0x02;
        constexpr uint8_t REG_SOC = 0x04;
        constexpr uint8_t REG_VERSION = 0x08;
        constexpr uint8_t REG_CRATE = 0x16;
        constexpr uint8_t REG_STATUS = 0x1A;
        constexpr uint16_t STATUS_RESET_INDICATOR = 0x0100; // RI: the gauge has just powered up

        float voltageFromVcell(uint16_t vcell); // 78.125 uV per LSB
        float percentFromSoc(uint16_t soc);     // 1/256 % per LSB, clamped to 0..100
        float rateFromCrate(int16_t crate);     // 0.208 %/h per LSB, positive when charging
        bool isMax17048(uint16_t version);      // VERSION reads 0x001x
        bool resetIndicated(uint16_t status);

        // One read of the gauge's registers.
        struct GaugeSample
        {
            uint16_t vcell = 0;
            uint16_t soc = 0;
            int16_t crate = 0;
            uint16_t status = 0;
        };

        // Turns register reads into values: "estimating" for 2 s after the
        // gauge resets or is first found, and while it reads 0 % above 3.5 V
        // (research D2); the rate as a 10-minute moving average, valid after
        // 60 s of samples (research D4).
        class GaugeDecoder
        {
        public:
            static constexpr uint32_t ESTIMATING_MS = 2000;
            static constexpr uint32_t RATE_VALID_MS = 60000;
            static constexpr uint32_t RATE_TIME_CONSTANT_MS = 600000;
            static constexpr float EMPTY_IMPOSSIBLE_VOLTS = 3.5f;

            struct Output
            {
                float voltage = 0.0f;
                float percent = 0.0f;
                bool estimating = true;
                bool rateValid = false;
                float rate = 0.0f; // %/h, smoothed
                bool resetSeen = false;
            };

            // A gauge found (again): starts the estimating window.
            void restart() { started = false; }
            Output update(const GaugeSample &sample, uint32_t nowMs);

        private:
            bool started = false;
            uint32_t estimatingFrom = 0;
            bool rateStarted = false;
            uint32_t rateFrom = 0;
            uint32_t lastRateMs = 0;
            float smoothed = 0.0f;
        };

        // The MCP73871's STAT1, STAT2 and PG lines (open drain, active low);
        // true = high.
        struct ChargerPins
        {
            bool stat1 = true;
            bool stat2 = true;
            bool pg = true;
        };

        struct ChargerOutput
        {
            ChargeState state = ChargeState::Unknown;
            std::optional<bool> usbPower; // unknown without the PG line
            ChargeFault fault = ChargeFault::None;
        };

        // The charger's truth table (research D3), one pattern at a time.
        ChargerOutput decodeCharger(const ChargerPins &pins);

        // The charger's state with the 2 s stability rule: a pattern is
        // reported once it has held 2 s; lines that keep toggling for more
        // than 6 s (no pack) read as no battery.
        class ChargerDecoder
        {
        public:
            static constexpr uint32_t STABLE_MS = 2000;
            static constexpr uint32_t TOGGLING_MS = 6000;

            ChargerOutput update(const ChargerPins &pins, uint32_t nowMs);

        private:
            bool seen = false;
            ChargerPins candidate;
            uint32_t candidateSince = 0;
            uint32_t togglingSince = 0;
            ChargerOutput reported;
        };

        // Without the charger lines: charging while the smoothed rate is
        // above +0.5 %/h, else discharging; unknown until the rate is valid.
        ChargerOutput chargerFromRate(bool rateValid, float rate);

        // Time to empty or full (research D4).
        constexpr uint32_t MAX_ESTIMATE_MINUTES = 5940; // 99 h; the UI shows "> 99 h"
        constexpr float MIN_ESTIMATE_RATE = 0.5f;       // %/h; slower gives no estimate
        constexpr float CHARGE_CURRENT_MA = 833.0f;     // the Rev A charger's set current

        struct EstimateInput
        {
            bool valid = false; // a percent, not estimating
            float percent = 0.0f;
            bool rateValid = false;
            float rate = 0.0f; // %/h, smoothed
            ChargeState state = ChargeState::Unknown;
            uint16_t capacityMah = 0; // 0 = unknown
        };

        struct Estimate
        {
            std::optional<uint32_t> timeToEmptyMinutes;
            std::optional<uint32_t> timeToFullMinutes;
            std::optional<float> currentMa; // positive when charging; needs the capacity
        };

        Estimate estimate(const EstimateInput &input);
    } // namespace Battery
} // namespace SQM
