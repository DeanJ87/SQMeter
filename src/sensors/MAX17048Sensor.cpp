#include "sensors/MAX17048Sensor.h"

#include <Arduino.h>

#include "Logger.h"

namespace SQM
{
    namespace
    {
        bool pinSet(uint8_t pin)
        {
            return pin != BatteryConfig::PIN_UNSET;
        }
    } // namespace

    void MAX17048Sensor::begin(const BatteryConfig &next)
    {
        settings = next;
        detected = probe();
        Logger::info(TAG, "Fuel gauge %s%s", detected ? "found" : "not found", settings.enabled ? "" : " (monitoring off)");
        if (settings.enabled)
        {
            setupPins();
            initialized = detected;
        }
    }

    void MAX17048Sensor::applySettings(const BatteryConfig &next)
    {
        const bool pinsChanged = next.alertPin != settings.alertPin || next.stat1Pin != settings.stat1Pin ||
                                 next.stat2Pin != settings.stat2Pin || next.pgPin != settings.pgPin;
        const bool wasEnabled = settings.enabled;
        settings = next;
        if (settings.enabled && (pinsChanged || !wasEnabled))
        {
            setupPins();
            charger = Battery::ChargerDecoder{};
        }
        if (!settings.enabled)
        {
            initialized = false;
            reading = BatteryReading{};
        }
    }

    // Open drain, active low: the internal pull-ups beside the board's own.
    void MAX17048Sensor::setupPins()
    {
        for (uint8_t pin : {settings.alertPin, settings.stat1Pin, settings.stat2Pin, settings.pgPin})
            if (pinSet(pin))
                pinMode(pin, INPUT_PULLUP);
    }

    bool MAX17048Sensor::readRegister(uint8_t reg, uint16_t &value)
    {
        wire.beginTransmission(Battery::GAUGE_ADDRESS);
        wire.write(reg);
        if (wire.endTransmission(false) != 0)
            return false;
        if (wire.requestFrom(static_cast<uint8_t>(Battery::GAUGE_ADDRESS), static_cast<uint8_t>(2)) != 2)
            return false;
        const uint8_t msb = static_cast<uint8_t>(wire.read());
        const uint8_t lsb = static_cast<uint8_t>(wire.read());
        value = static_cast<uint16_t>((msb << 8) | lsb);
        return true;
    }

    bool MAX17048Sensor::writeRegister(uint8_t reg, uint16_t value)
    {
        wire.beginTransmission(Battery::GAUGE_ADDRESS);
        wire.write(reg);
        wire.write(static_cast<uint8_t>(value >> 8));
        wire.write(static_cast<uint8_t>(value & 0xFF));
        return wire.endTransmission() == 0;
    }

    // ACK at 0x36 and VERSION 0x001x (research D1).
    bool MAX17048Sensor::probe()
    {
        uint16_t version = 0;
        if (!readRegister(Battery::REG_VERSION, version) || !Battery::isMax17048(version))
            return false;
        reading.version = version;
        return true;
    }

    bool MAX17048Sensor::readGauge(uint32_t nowMs)
    {
        Battery::GaugeSample sample;
        uint16_t crate = 0;
        const bool ok = readRegister(Battery::REG_VCELL, sample.vcell) && readRegister(Battery::REG_SOC, sample.soc) &&
                        readRegister(Battery::REG_CRATE, crate) && readRegister(Battery::REG_STATUS, sample.status);
        if (!ok)
            return false;
        sample.crate = static_cast<int16_t>(crate);
        if (Battery::resetIndicated(sample.status))
        {
            // Seen once; clear it so the next reset shows again.
            writeRegister(Battery::REG_STATUS, static_cast<uint16_t>(sample.status & ~Battery::STATUS_RESET_INDICATOR));
            reading.resetSeen = true;
        }
        const Battery::GaugeDecoder::Output out = gauge.update(sample, nowMs);
        reading.vcellRaw = sample.vcell;
        reading.socRaw = sample.soc;
        reading.crateRaw = sample.crate;
        reading.statusRaw = sample.status;
        reading.voltage = out.voltage;
        reading.percent = out.percent;
        reading.estimating = out.estimating;
        reading.rateValid = out.rateValid;
        reading.ratePercentPerHour = out.rate;
        reading.status = SensorStatus::Ok;
        reading.timestamp = nowMs;
        if (pinSet(settings.alertPin))
            reading.gaugeAlert = digitalRead(settings.alertPin) == LOW;
        else
            reading.gaugeAlert.reset();
        lastUpdate = nowMs;
        return true;
    }

    void MAX17048Sensor::readCharger(uint32_t nowMs)
    {
        reading.chargerPins = settings.chargerPins();
        if (!reading.chargerPins)
        {
            const Battery::ChargerOutput out = Battery::chargerFromRate(reading.rateValid, reading.ratePercentPerHour);
            reading.chargeState = out.state;
            reading.usbPower = out.usbPower;
            reading.fault = out.fault;
            return;
        }
        Battery::ChargerPins pins;
        pins.stat1 = digitalRead(settings.stat1Pin) == HIGH;
        pins.stat2 = digitalRead(settings.stat2Pin) == HIGH;
        pins.pg = digitalRead(settings.pgPin) == HIGH;
        reading.stat1 = pins.stat1;
        reading.stat2 = pins.stat2;
        reading.pg = pins.pg;
        const Battery::ChargerOutput out = charger.update(pins, nowMs);
        reading.chargeState = out.state;
        reading.usbPower = out.usbPower;
        reading.fault = out.fault;
    }

    bool MAX17048Sensor::readNow(uint32_t nowMs)
    {
        if (!detected && !probe())
            return false;
        detected = true;
        const bool ok = readGauge(nowMs);
        readCharger(nowMs);
        return ok;
    }

    void MAX17048Sensor::tick(uint32_t nowMs, const BatteryConfig &current)
    {
        if (reconfigurePending.exchange(false))
            applySettings(current);
        if (!settings.enabled)
            return;

        if (nowMs - lastChargerRead >= CHARGER_POLL_MS || lastChargerRead == 0)
        {
            lastChargerRead = nowMs == 0 ? 1 : nowMs;
            readCharger(nowMs);
        }

        if (!initialized)
        {
            // Not answering: look for it again now and then (a pack put back in).
            if (lastDetectAttempt != 0 && nowMs - lastDetectAttempt < REDETECT_MS)
                return;
            lastDetectAttempt = nowMs == 0 ? 1 : nowMs;
            if (!probe())
                return;
            Logger::info(TAG, "Fuel gauge found");
            detected = initialized = true;
            misses = 0;
            gauge.restart();
            lastGaugeRead = 0;
            everRead = false;
        }

        if (everRead && nowMs - lastGaugeRead < readIntervalMs)
            return;
        everRead = true;
        lastGaugeRead = nowMs;
        if (readGauge(nowMs))
        {
            misses = 0;
            return;
        }
        reading.status = SensorStatus::ReadError;
        if (++misses >= MISSES_BEFORE_MISSING)
        {
            Logger::warn(TAG, "Fuel gauge stopped answering");
            initialized = false;
            reading.status = SensorStatus::NotInitialized;
        }
    }
} // namespace SQM
