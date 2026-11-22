#pragma once

#include "Config.h"
#include "MQTTClient.h"
#include "StartupSequence.h"
#include "TimeManager.h"
#include "WebServer.h"
#include "WiFiManager.h"
#include "sensors/BME280Sensor.h"
#include "sensors/GPSSensor.h"
#include "sensors/MAX17048Sensor.h"
#include "sensors/MLX90614Sensor.h"
#include "sensors/RG15Sensor.h"
#include "sensors/TSL2591Sensor.h"
#include "sensors/WindSensor.h"

namespace SQM
{
    // What the staged start-up (lib/BootLogic) starts, created by setup().
    struct StartupDevices
    {
        const Config &config;
        WiFiManager &wifi;
        WebServer &web;
        TimeManager &time;
        MQTTClient &mqtt;
        TSL2591Sensor &sqm;
        TSL2591Sensor &sqml;
        bool (*skySensorActive)(SkyRole role); // on in the settings and its bus running
        BME280Sensor &bme;
        MLX90614Sensor &mlx;
        GPSSensor &gps;
        RG15Sensor &rain;
        WindSensor &wind;
        MAX17048Sensor &battery;
        bool arduinoOtaEnabled;
    };

    namespace StartupSteps
    {
        // The steps this device runs: those for what is switched off are left out.
        uint32_t enabledSteps(const StartupDevices &devices);
        // Carries out one step.
        void run(Boot::Step step, const StartupDevices &devices);
    } // namespace StartupSteps
} // namespace SQM
