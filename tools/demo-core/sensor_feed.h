#pragma once

// The demo's simulated sensors: the inputs from web/src/demo/simulator.ts
// turned into the readings the device's drivers would produce, into the
// emulated device's snapshot (tools/demo-core/bridge.cpp).

#include <ArduinoJson.h>
#include <cstdint>
#include <deque>
#include <utility>
#include "Config.h"
#include "RainLogic.h"
#include "DeviceCore.h"

namespace SQM
{
    class SensorFeed
    {
    public:
        // The emulated device's snapshot, settings (now and at boot) and clocks.
        SensorFeed(SensorSnapshot &snapshot, const Config &cfg, const Config &bootConfig, const uint32_t &nowMs, const uint32_t &bootMs)
            : snapshot(snapshot),
              cfg(cfg),
              bootConfig(bootConfig),
              nowMs(nowMs),
              bootMs(bootMs)
        {
        }

        // One sensor cycle.
        void read(JsonObjectConst in);
        // After a restart: the drivers start over (rain latch, light window).
        void restart();

        const Rain::Latch &rainLatch() const { return latch; }
        uint16_t lightWindowSeconds() const { return lightWindow; }
        uint32_t lastLightStepMs() const { return lastLightStep; }

    private:
        SensorSnapshot &snapshot;
        const Config &cfg;
        const Config &bootConfig;
        const uint32_t &nowMs;
        const uint32_t &bootMs;

        Rain::Latch latch;
        uint32_t lastRainTickMs = 0;
        std::deque<std::pair<uint32_t, float>> lightSamples; // (ms, lux) within the window
        uint16_t lightWindow = 90;
        uint32_t lastLightStep = 0;
        float lastInputLux = 0.0f;

        float averagedLux(float inputLux, bool nightMode, uint32_t now);
        void readLight(JsonObjectConst in);
        void lightDiagnostics();
        void readEnvironment(JsonObjectConst in);
        void readGps(JsonObjectConst in);
        void readRain(JsonObjectConst in);
        void rainDiagnostics();
        void readWind(JsonObjectConst in);
    };
} // namespace SQM
