#pragma once

#include <atomic>
#include <cstdint>

#include "BatteryLogic.h"
#include "Config.h"
#include "sensors/MAX17048Sensor.h"

// Applies the battery power policy (specs/032-battery research D7): slower
// reads and the display off while saving power, the shutdown warning, then
// deep sleep until USB power returns (with an hourly timer check). The
// decisions are lib/BatteryLogic's PowerPolicy; this does what they say.

namespace SQM
{
    class PowerManager
    {
    public:
        explicit PowerManager(MAX17048Sensor &gauge)
            : gauge(gauge)
        {
        }

        // setup(), right after the I2C bus: after a battery sleep woken by
        // the hourly timer, read the gauge; still empty and no USB power ->
        // back to sleep at once, silently. Otherwise boot normally (a wake by
        // USB power or charging sends "power restored" later).
        static void bootCheck(TwoWire &bus, const BatteryConfig &settings);

        // Every loop pass, after the gauge's tick.
        void tick(uint32_t nowMs, const BatteryConfig &settings, bool dispatcherIdle, bool wifiConnected);

        Battery::PowerState state() const { return current.load(); }
        bool saving() const { return state() != Battery::PowerState::Normal; }
        uint32_t shutdownInSeconds() const { return secondsLeft.load(); }
        // The sensor read interval to use: slower while saving power.
        uint32_t sensorIntervalMs(const Config &cfg) const;

        // One-shots for the next alert pass (consumed).
        bool takeShutdownStarted() { return shutdownStarted.exchange(false); }
        bool takePowerRestored() { return powerRestored.exchange(false); }

    private:
        static constexpr uint64_t TIMER_WAKE_US = 60ULL * 60ULL * 1000000ULL; // hourly check without a PG pin
        static constexpr uint32_t RESTORED_ALERT_AFTER_MS = 120000;           // or once WiFi is joined

        [[noreturn]] void sleep(const BatteryConfig &settings);

        MAX17048Sensor &gauge;
        Battery::PowerPolicy policy;
        uint32_t lastReadAt = 0;
        bool restoredAtBoot = false;
        bool bootChecked = false;
        std::atomic<Battery::PowerState> current{Battery::PowerState::Normal};
        std::atomic<uint32_t> secondsLeft{0};
        std::atomic<bool> shutdownStarted{false};
        std::atomic<bool> powerRestored{false};
    };
} // namespace SQM
