// The demo's device core, exported to JavaScript: EmulatedDevice
// (emulated_device.h) holds what an SQMeter would hold and answers the same
// requests with the same documents (specs/016-demo-device-emulation).

#include <emscripten/bind.h>

#include "emulated_device.h"

EMSCRIPTEN_BINDINGS(sqmeter_core)
{
    emscripten::class_<EmulatedDevice>("EmulatedDevice")
        .constructor<std::string>()
        .function("getConfig", &EmulatedDevice::getConfig)
        .function("applyConfig", &EmulatedDevice::applyConfig)
        .function("loadConfig", &EmulatedDevice::loadConfig)
        .function("restart", &EmulatedDevice::restart)
        .function("tick", &EmulatedDevice::tick)
        .function("readings", &EmulatedDevice::readings)
        .function("statusParts", &EmulatedDevice::statusParts)
        .function("safety", &EmulatedDevice::safetyDocument)
        .function("effective", &EmulatedDevice::effective)
        .function("pending", &EmulatedDevice::pending)
        .function("safetyHistory", &EmulatedDevice::safetyHistory)
        .function("recentAlerts", &EmulatedDevice::recentAlerts)
        .function("clearAlerts", &EmulatedDevice::clearAlerts)
        .function("isArmed", &EmulatedDevice::isArmed)
        .function("armedDocument", &EmulatedDevice::armedDocument)
        .function("setArmed", &EmulatedDevice::setArmed)
        .function("testAlert", &EmulatedDevice::testAlert)
        .function("realTestAlert", &EmulatedDevice::realTestAlert)
        .function("deliveryRequests", &EmulatedDevice::deliveryRequests)
        .function("calibrateDark", &EmulatedDevice::calibrateDark)
        .function("alpaca", &EmulatedDevice::alpaca)
        .function("saveState", &EmulatedDevice::saveState)
        .function("loadState", &EmulatedDevice::loadState);
}
