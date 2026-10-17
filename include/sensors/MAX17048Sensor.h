#pragma once

#include <Wire.h>

#include <atomic>
#include <cstdint>

#include "BatteryLogic.h"
#include "Config.h"
#include "SensorTypes.h"

// The travel board's MAX17048 fuel gauge at 0x36 on the main I2C bus and the
// MCP73871 charger's STAT1/STAT2/PG lines (specs/032-battery, research D1, D3).
// Register decoding and the charger table are in lib/BatteryLogic; this reads
// the bus and the pins, on the loop task like every other I2C transaction.

namespace SQM
{
    class MAX17048Sensor
    {
    public:
        explicit MAX17048Sensor(TwoWire &bus)
            : wire(bus)
        {
        }

        // At boot: the settings, and one presence probe even with monitoring
        // off, so Settings can say "Detected" (research D8).
        void begin(const BatteryConfig &settings);
        // After a save (web server task): the loop applies it on its next tick.
        void requestReconfigure() { reconfigurePending = true; }
        // Every loop pass. Reads the gauge every `readIntervalMs` and the
        // charger lines every 500 ms; nothing while monitoring is off.
        void tick(uint32_t nowMs, const BatteryConfig &current);
        // Power saving reads the gauge less often (spec 032 research D7).
        void setReadIntervalMs(uint32_t intervalMs) { readIntervalMs = intervalMs; }

        // One read straight away, for the boot check after a battery sleep.
        bool readNow(uint32_t nowMs);

        const BatteryReading &getReading() const { return reading; }
        bool isDetected() const { return detected.load(); }
        bool isInitialized() const { return initialized; }
        uint32_t getLastUpdateTime() const { return lastUpdate; }

    private:
        static constexpr const char *TAG = "Battery";
        static constexpr uint32_t CHARGER_POLL_MS = 500;
        static constexpr uint32_t REDETECT_MS = 10000;
        static constexpr uint8_t MISSES_BEFORE_MISSING = 3;

        void applySettings(const BatteryConfig &next);
        void setupPins();
        bool probe();
        bool readRegister(uint8_t reg, uint16_t &value);
        bool writeRegister(uint8_t reg, uint16_t value);
        bool readGauge(uint32_t nowMs);
        void readCharger(uint32_t nowMs);

        TwoWire &wire;
        BatteryConfig settings;
        std::atomic<bool> reconfigurePending{false};
        std::atomic<bool> detected{false};
        bool initialized = false;
        uint32_t readIntervalMs = Battery::READ_INTERVAL_MS;
        uint32_t lastGaugeRead = 0;
        uint32_t lastChargerRead = 0;
        uint32_t lastDetectAttempt = 0;
        uint32_t lastUpdate = 0;
        uint8_t misses = 0;
        bool everRead = false;
        Battery::GaugeDecoder gauge;
        Battery::ChargerDecoder charger;
        BatteryReading reading;
    };
} // namespace SQM
