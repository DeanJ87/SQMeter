#include "sensors/WindSensor.h"
#include "Logger.h"
#include <Arduino.h>
#include <ArduinoJson.h>
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
                ++pulseCount;
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
    }

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
        if (settings.directionEnabled)
            analogSetPinAttenuation(settings.directionPin, ADC_11db);
        attached = true;
        lastSampleAt = millis();
        reading.status = SensorStatus::OK;
        Logger::info(TAG, "Anemometer on GPIO%u (%.3f km/h per Hz)%s", settings.speedPin, settings.kmhPerHz,
                     settings.directionEnabled ? ", vane enabled" : "");
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
        if (!settings.enabled || !attached)
        {
            reading.status = SensorStatus::NOT_INITIALIZED;
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

        float direction = -1.0f;
        if (settings.directionEnabled)
        {
            const float ratio = static_cast<float>(analogReadMilliVolts(settings.directionPin)) / ADC_REFERENCE_MV;
            float vane = 0.0f;
            if (Wind::vaneDirectionFromRatio(ratio, settings.vanePullupOhms, vane))
            {
                direction = std::fmod(vane + settings.directionOffsetDeg + 360.0f, 360.0f);
                vaneMisses = 0;
            }
            else if (vaneMisses < VANE_FAULT_SECONDS)
            {
                ++vaneMisses;
            }
        }

        aggregator.addSample(speedMs, direction);

        reading.instantMs = speedMs;
        reading.speedMs = aggregator.speedMs();
        reading.gustMs = aggregator.gustMs();
        reading.samples = static_cast<uint32_t>(aggregator.sampleCount());
        reading.vaneFault = settings.directionEnabled && vaneMisses >= VANE_FAULT_SECONDS;
        float avgDirection = 0.0f;
        reading.directionValid = settings.directionEnabled && !reading.vaneFault && aggregator.directionDeg(avgDirection);
        reading.directionDeg = reading.directionValid ? avgDirection : 0.0f;
        reading.timestamp = now;
        reading.status = SensorStatus::OK;
        lastUpdateTime = now;
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
