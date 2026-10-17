#include "PowerManager.h"

#include <Arduino.h>
#include <driver/rtc_io.h>
#include <esp_sleep.h>

#include "Logger.h"

namespace SQM
{
    namespace
    {
        // Kept across deep sleep (RTC memory, 8 bytes).
        RTC_DATA_ATTR bool sleptForBattery = false;
        RTC_DATA_ATTR uint8_t batteryWakeCause = 0;

        constexpr const char *TAG = "Power";

        // GPIOs that can wake the ESP32 from deep sleep (ext0), as
        // ConfigValidate's battery check.
        bool rtcCapable(uint8_t pin)
        {
            return rtc_gpio_is_valid_gpio(static_cast<gpio_num_t>(pin));
        }

        bool chargerSaysUsb(const BatteryConfig &settings)
        {
            if (!settings.chargerPins())
                return false;
            pinMode(settings.stat1Pin, INPUT_PULLUP);
            pinMode(settings.stat2Pin, INPUT_PULLUP);
            pinMode(settings.pgPin, INPUT_PULLUP);
            delay(2);
            Battery::ChargerPins pins;
            pins.stat1 = digitalRead(settings.stat1Pin) == HIGH;
            pins.stat2 = digitalRead(settings.stat2Pin) == HIGH;
            pins.pg = digitalRead(settings.pgPin) == HIGH;
            const Battery::ChargerOutput out = Battery::decodeCharger(pins);
            return out.usbPower.value_or(false) || out.state == Battery::ChargeState::Charging || out.state == Battery::ChargeState::Charged;
        }
    } // namespace

    void PowerManager::bootCheck(TwoWire &bus, const BatteryConfig &settings)
    {
        if (!sleptForBattery)
            return;
        const esp_sleep_wakeup_cause_t cause = esp_sleep_get_wakeup_cause();
        batteryWakeCause = static_cast<uint8_t>(cause);
        if (cause == ESP_SLEEP_WAKEUP_TIMER && settings.enabled)
        {
            MAX17048Sensor probe(bus);
            BatteryConfig quiet = settings;
            probe.begin(quiet);
            const bool read = probe.readNow(millis());
            const BatteryReading &r = probe.getReading();
            // The gauge kept running through the sleep, so its percent stands
            // (no estimating window); charging shows in the rate without pins.
            const bool stillEmpty = read && r.status == SensorStatus::Ok && r.percent <= static_cast<float>(settings.shutdownPercent);
            const bool usb = chargerSaysUsb(settings) || (r.rateValid && r.ratePercentPerHour > Battery::MIN_ESTIMATE_RATE);
            if (stillEmpty && !usb)
            {
                Logger::info(TAG, "Battery still at %.0f%%: back to sleep", r.percent);
                if (settings.pgPin != BatteryConfig::PIN_UNSET && rtcCapable(settings.pgPin))
                    esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(settings.pgPin), 0);
                esp_sleep_enable_timer_wakeup(TIMER_WAKE_US);
                esp_deep_sleep_start();
            }
        }
        Logger::info(TAG, "Awake after a battery sleep (wake cause %u)", static_cast<unsigned>(cause));
        sleptForBattery = false;
    }

    uint32_t PowerManager::sensorIntervalMs(const Config &cfg) const
    {
        if (!saving())
            return cfg.sensor.readIntervalMs;
        const uint32_t slow = static_cast<uint32_t>(cfg.battery.saveReadIntervalSeconds) * 1000U;
        return slow > cfg.sensor.readIntervalMs ? slow : cfg.sensor.readIntervalMs;
    }

    void PowerManager::tick(uint32_t nowMs, const BatteryConfig &settings, bool dispatcherIdle, bool wifiConnected)
    {
        if (!bootChecked)
        {
            bootChecked = true;
            // Woken by USB power (or charging at the hourly check): "power restored", later.
            restoredAtBoot = batteryWakeCause != 0;
        }
        if (restoredAtBoot && (wifiConnected || nowMs >= RESTORED_ALERT_AFTER_MS))
        {
            restoredAtBoot = false;
            powerRestored = true;
        }

        if (!settings.enabled || !gauge.isDetected())
        {
            current = Battery::PowerState::Normal;
            secondsLeft = 0;
            gauge.setReadIntervalMs(Battery::READ_INTERVAL_MS);
            return;
        }

        const BatteryReading &r = gauge.getReading();
        Battery::PolicyInput in;
        in.newRead = r.timestamp != 0 && r.timestamp != lastReadAt;
        lastReadAt = r.timestamp;
        in.valid = gauge.isInitialized() && r.status == SensorStatus::Ok && !r.estimating;
        in.percent = r.percent;
        in.state = r.chargeState;
        in.usbPower = r.usbPower;
        in.powerSaving = settings.powerSaving;
        in.criticalPercent = settings.criticalPercent;
        in.shutdownPercent = settings.shutdownPercent;
        in.graceSeconds = settings.shutdownGraceSeconds;
        in.dispatcherIdle = dispatcherIdle;
        in.nowMs = nowMs;
        const Battery::PolicyEvents events = policy.update(in);

        current = policy.state();
        secondsLeft = policy.secondsLeft(nowMs, settings.shutdownGraceSeconds);
        gauge.setReadIntervalMs(saving() ? static_cast<uint32_t>(settings.saveReadIntervalSeconds) * 1000U : Battery::READ_INTERVAL_MS);
        if (events.enteredSaving)
            Logger::warn(TAG, "Battery %.0f%%: saving power", r.percent);
        if (events.shutdownStarted)
        {
            Logger::warn(TAG, "Battery %.0f%%: going to sleep in %u s", r.percent, static_cast<unsigned>(settings.shutdownGraceSeconds));
            shutdownStarted = true;
        }
        if (events.powerRestored)
        {
            Logger::info(TAG, "Power restored");
            powerRestored = true;
        }
        if (events.sleepNow)
            sleep(settings);
    }

    void PowerManager::sleep(const BatteryConfig &settings)
    {
        Logger::warn(TAG, "Deep sleep until USB power returns");
        sleptForBattery = true;
        batteryWakeCause = 0;
        if (settings.pgPin != BatteryConfig::PIN_UNSET && rtcCapable(settings.pgPin))
            esp_sleep_enable_ext0_wakeup(static_cast<gpio_num_t>(settings.pgPin), 0); // PG low = USB power
        esp_sleep_enable_timer_wakeup(TIMER_WAKE_US);
        delay(100); // let the log line out
        esp_deep_sleep_start();
    }
} // namespace SQM
