#include "sensors/WindSensor.h"
#include "Logger.h"
#include <Arduino.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <cmath>

namespace SQM
{
    namespace
    {
        // 2 ms debounce: fastest accepted pulse rate is 500 Hz (~1200 km/h on
        // a Misol cup) - far beyond anything real, far longer than reed bounce.
        constexpr uint32_t DEBOUNCE_US = 2000;

        volatile uint32_t pulseCount = 0;
        volatile uint32_t lastPulseUs = 0;
        portMUX_TYPE pulseMux = portMUX_INITIALIZER_UNLOCKED;

        void IRAM_ATTR onAnemometerPulse()
        {
            const uint32_t now = micros();
            portENTER_CRITICAL_ISR(&pulseMux);
            if (now - lastPulseUs >= DEBOUNCE_US)
            {
                pulseCount = pulseCount + 1; // C++20: no ++ on volatile
                lastPulseUs = now;
            }
            portEXIT_CRITICAL_ISR(&pulseMux);
        }

        uint32_t takePulses()
        {
            portENTER_CRITICAL(&pulseMux);
            const uint32_t count = pulseCount;
            pulseCount = 0;
            portEXIT_CRITICAL(&pulseMux);
            return count;
        }
    } // namespace

    WindSensor::~WindSensor()
    {
        detach();
    }

    void WindSensor::configure(const WindSensorSettings &newSettings)
    {
        const bool pinsChanged = newSettings.speedPin != settings.speedPin || newSettings.enabled != settings.enabled;
        settings = newSettings;
        if (pinsChanged)
        {
            detach();
            aggregator.reset();
            reading = WindReading{};
            if (settings.enabled)
                aggregator = std::make_unique<Wind::WindAggregator>();
            if (settings.enabled && initialized)
                attach();
        }
    }

    bool WindSensor::begin()
    {
        initialized = true;
        if (settings.enabled)
            attach();
        return true;
    }

    void WindSensor::attach()
    {
        pinMode(settings.speedPin, INPUT_PULLUP);
        takePulses();
        attachInterrupt(digitalPinToInterrupt(settings.speedPin), onAnemometerPulse, FALLING);
        attached = true;
        lastSampleAt = millis();
        reading.status = SensorStatus::Ok;
        Logger::info(
            TAG,
            "Anemometer on GPIO%u (%.3f km/h per Hz)%s",
            settings.speedPin,
            settings.kmhPerHz,
            settings.directionEnabled ? ", AS5600 vane enabled" : "");
    }

    void WindSensor::detach()
    {
        if (!attached)
            return;
        detachInterrupt(digitalPinToInterrupt(settings.speedPin));
        attached = false;
    }

    void WindSensor::update()
    {
        if (!settings.enabled || !attached || !aggregator)
        {
            reading.status = SensorStatus::NotInitialized;
            return;
        }

        const uint32_t now = millis();
        const uint32_t elapsed = now - lastSampleAt;
        if (elapsed < SAMPLE_INTERVAL_MS)
            return;
        lastSampleAt = now;

        const uint32_t pulses = takePulses();
        const float hz = static_cast<float>(pulses) * 1000.0f / static_cast<float>(elapsed);
        const float speedMs = hz * settings.kmhPerHz / 3.6f;

        const float direction = settings.directionEnabled ? readVane() : -1.0f;
        if (!settings.directionEnabled)
            reading.vaneMagnet = As5600::Magnet::Off;
        aggregator->addSample(speedMs, direction);

        reading.instantMs = speedMs;
        reading.speedMs = aggregator->speedMs();
        reading.gustMs = aggregator->gustMs();
        reading.samples = static_cast<uint32_t>(aggregator->sampleCount());
        reading.vaneFault = settings.directionEnabled && vaneMisses >= VANE_FAULT_SECONDS;
        float avgDirection = 0.0f;
        reading.directionValid = settings.directionEnabled && !reading.vaneFault && aggregator->directionDeg(avgDirection);
        reading.directionDeg = reading.directionValid ? avgDirection : 0.0f;
        reading.timestamp = now;
        reading.status = SensorStatus::Ok;
        lastUpdateTime = now;
    }

    float WindSensor::readVane()
    {
        uint8_t bytes[3] = {}; // STATUS, RAW ANGLE high, low
        wire.beginTransmission(As5600::ADDRESS);
        wire.write(As5600::REG_STATUS);
        bool answered = wire.endTransmission(false) == 0 && wire.requestFrom(As5600::ADDRESS, static_cast<uint8_t>(3)) == 3;
        for (uint8_t &b : bytes)
            b = answered ? static_cast<uint8_t>(wire.read()) : 0;

        reading.vaneMagnet = answered ? As5600::magnetFromStatus(bytes[0]) : As5600::Magnet::NoAnswer;
        if (!As5600::hasAngle(reading.vaneMagnet))
        {
            if (vaneMisses < VANE_FAULT_SECONDS)
                ++vaneMisses;
            return -1.0f;
        }
        vaneMisses = 0;

        wire.beginTransmission(As5600::ADDRESS);
        wire.write(As5600::REG_AGC);
        if (wire.endTransmission(false) == 0 && wire.requestFrom(As5600::ADDRESS, static_cast<uint8_t>(1)) == 1)
            reading.vaneAgc = static_cast<uint8_t>(wire.read());
        return std::fmod(As5600::angleDeg(bytes[1], bytes[2]) + settings.directionOffsetDeg + 360.0f, 360.0f);
    }

    std::string WindSensor::toJson() const
    {
        StaticJsonDocument<256> doc;
        doc["sensor"] = "Wind";
        doc["timestamp"] = reading.timestamp;
        doc["status"] = static_cast<int>(reading.status);
        doc["speedMs"] = reading.speedMs;
        doc["gustMs"] = reading.gustMs;
        doc["directionDeg"] = reading.directionDeg;
        doc["directionValid"] = reading.directionValid;
        std::string output;
        serializeJson(doc, output);
        return output;
    }

} // namespace SQM
