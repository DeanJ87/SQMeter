#pragma once

// The SQMeter firmware's own decision code (lib/), running in the browser for
// the demo at https://demo.sqmeter.dev. One EmulatedDevice holds what the
// device would hold - settings, the latest readings, the safety verdict, the
// alert engine, the safety history and the Alpaca API - and answers the same
// requests with the same documents. No I/O: the demo feeds it simulated
// sensor values and the time (specs/016-demo-device-emulation).

#include <ArduinoJson.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#include "AlertEngine.h"
#include "AlertDelivery.h"
#include "AlpacaRouter.h"
#include "Config.h"
#include "DeviceCore.h"
#include "RainLogic.h"
#include "SafetyHistoryLog.h"
#include "SettingsDeps.h"
#include "sensor_feed.h"
#include "calculations/Dewpoint.h"
#include "calculations/SkyQuality.h"

using namespace SQM;

namespace DemoCore
{
    inline constexpr size_t MAX_RECORDS = 20; // AlertDispatcher::MAX_RECORDS
    inline const char *const CHANNEL_NAMES[] = {"mqtt", "pushover", "ntfy", "webhook"};
    inline const char *const DEMO_DETAIL = "Demo: nothing was sent";

    inline std::string errorJson(const std::string &message)
    {
        StaticJsonDocument<512> doc;
        doc["error"] = message;
        std::string out;
        serializeJson(doc, out);
        return out;
    }

    struct Record
    {
        uint32_t id = 0;
        uint32_t uptimeSeconds = 0;
        int64_t epochSeconds = 0;
        Alerts::Alert alert;
        bool channel[4] = {false, false, false, false};
        uint8_t requested = 0x0F;                                      // channels asked for: all, or the one a per-channel test named
        const char *skipped[4] = {nullptr, nullptr, nullptr, nullptr}; // switched on but inactive: why
    };
} // namespace DemoCore

using DemoCore::Record;

class EmulatedDevice : public Alpaca::Backend
{
public:
    // identityJson: {"version": "...", "mac": "a1b2c3d4e5f6"}
    explicit EmulatedDevice(const std::string &identityJson)
        : router(*this, identity(identityJson))
    {
        cfg = Config::createDefault();
        bootConfig = cfg;
        history.magic = 0; // garbage -> cleared on the first boot
        SafetyHistory::startBoot(history, false);
    }

    // --- Alpaca::Backend ---------------------------------------------------
    bool alpacaEnabled() const override { return cfg.alpaca.enabled; }
    bool isSafe() const override { return safety.isSafe; }
    Alpaca::ObservingConditionsSnapshot observingConditions() const override { return Core::observingConditions(snapshot, cfg, nowMs); }
    std::string serverName() const override { return cfg.deviceName; }
    std::string location() const override { return Core::alpacaLocation(snapshot, cfg); }
    std::string timestampUtc() const override { return Core::isoUtc(epoch); }

    // --- Settings ----------------------------------------------------------
    std::string getConfig(bool redacted) const { return cfg.toJson(redacted); }

    // POST /api/config. Same validation and messages as the device.
    std::string applyConfig(const std::string &json);

    // Restores saved demo state without the "changed by the UI" rules.
    bool loadConfig(const std::string &json);

    // POST /api/restart (applied by the demo after a short pause).
    void restart(double nowMsIn);

    // --- One sensor cycle --------------------------------------------------
    // inputsJson: simulated sensors (web/src/demo/simulator.ts).
    void tick(double nowMsIn, double epochIn, const std::string &inputsJson, const std::string &localTime, const std::string &localDate);

    // --- Documents ---------------------------------------------------------
    // GET /api/sensors and /ws/sensors.
    std::string readings() const;

    // The decision parts of GET /api/status; the demo adds the hardware parts.
    std::string statusParts() const;

    // GET /api/settings/effective: what's actually in effect, from the
    // device's own dependency rules (specs/020-settings-dependencies).
    std::string effective() const;

    // What the device is still waiting on: the light average catching up,
    // the rain clear delay and held-back alerts (the demo panel's waits;
    // specs/019-demo-conditions/contracts/core-pending.md).
    std::string pending() const;

    // GET /api/safety
    std::string safetyDocument() const;

    // GET /api/safety/history
    std::string safetyHistory() const;

    // GET /api/alerts/recent
    std::string recentAlerts() const;

    void clearAlerts() { records.clear(); }

    // --- Pause / resume (specs/021) ------------------------------------------
    bool isArmed() const { return schedule.state().sending; }
    // GET /api/alerts/armed
    std::string armedDocument() const;

    // POST /api/alerts/arm|disarm (source "ui" or "rest") and MQTT ("mqtt").
    void setArmed(bool on, const std::string &source);

private:
    int64_t validEpoch() const { return epoch >= Core::CLOCK_VALID_EPOCH ? epoch : 0; }

    void scheduleChanged(bool wasSending);

public:
    // POST /api/alerts/test?channel=&event=&level=&sound=&title=&message=
    // Returns {status, body} like the device.
    std::string testAlert(const std::string &paramsJson);

    // Demo panel "Send a test" for real notifications (specs/018): a test
    // alert recorded whatever the device's own channel switches say.
    void realTestAlert();

    // The requests a real SQMeter would send for alert `id`, for the
    // channels the visitor set up (specs/018 contracts/delivery-requests.md).
    // Wake is sent as Urgent: Pushover's emergency level needs acknowledging.
    std::string deliveryRequests(uint32_t id, const std::string &credentialsJson) const;

    // POST /api/sensors/tsl2591/calibrate-dark - the device's checks.
    std::string calibrateDark();

    // --- Alpaca --------------------------------------------------------------
    // method "GET"/"PUT", path, params as [[name, value], ...].
    std::string alpaca(const std::string &method, const std::string &path, const std::string &paramsJson);

    // --- Persistence (sessionStorage) -----------------------------------------
    std::string saveState() const;

    bool loadState(const std::string &json);

private:
    static Alpaca::ServerIdentity identity(const std::string &json);

    static std::string serialize(const JsonDocument &doc);

    static std::string response(int status, const std::string &body);

    uint32_t uptimeSeconds() const { return (nowMs - bootMs) / 1000; }

    static void writeRequest(JsonObject target, const Delivery::HttpRequest &request);

    // The demo's network is simulated: WiFi is up, and the broker is
    // "connected" whenever MQTT is on.
    Deps::Facts facts() const;

    uint8_t enabledChannels() const;

    void record(const Alerts::Alert &alert, uint8_t mask);

    void pushHistory(SafetyHistory::Kind kind, bool safe, bool held, uint32_t flags, uint8_t resetReason);

    void runAlerts();

    // The TSL2591 driver averages its samples over the sky averaging window
    // and, at night, reports the average (src/sensors/TSL2591Sensor.cpp);
    // emulated here so light changes take the time they take on the device.
    Config cfg;
    Config bootConfig; // what the hardware was started with (restart-only settings)
    SensorSnapshot snapshot;
    SafetyStatus safety;
    Alpaca::SafeDelayFilter delayFilter;
    Alerts::AlertEngine engine;
    bool engineSeeded = false;
    Alerts::AlertSchedule schedule;
    Alpaca::ClientWatch clientWatch;
    SafetyHistory::Log history{};
    std::deque<Record> records;
    uint32_t nextId = 1;
    uint32_t nowMs = 0;
    uint32_t bootMs = 0;
    int64_t epoch = 0;
    std::string clockTime = "--:--";
    std::string clockDate = "--";
    Alpaca::Router router;
    // Simulated sensors -> the readings the drivers would produce (sensor_feed.cpp).
    SensorFeed feed{snapshot, cfg, bootConfig, nowMs, bootMs};
};
