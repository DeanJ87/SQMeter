// The SQMeter firmware's own decision code (lib/), running in the browser for
// the demo at https://demo.sqmeter.dev. One EmulatedDevice holds what the
// device would hold - settings, the latest readings, the safety verdict, the
// alert engine, the safety history and the Alpaca API - and answers the same
// requests with the same documents. No I/O: the demo feeds it simulated
// sensor values and the time (specs/016-demo-device-emulation).

#include <emscripten/bind.h>

#include <ArduinoJson.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <deque>
#include <string>
#include <vector>

#include "AlertEngine.h"
#include "AlpacaRouter.h"
#include "Config.h"
#include "DeviceCore.h"
#include "RainLogic.h"
#include "SafetyHistoryLog.h"
#include "calculations/Dewpoint.h"
#include "calculations/SkyQuality.h"

using namespace SQM;

namespace
{
    constexpr size_t MAX_RECORDS = 20; // AlertDispatcher::MAX_RECORDS
    const char *const CHANNEL_NAMES[] = {"mqtt", "pushover", "ntfy", "webhook"};
    const char *const DEMO_DETAIL = "Demo: nothing was sent";

    std::string errorJson(const std::string &message)
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
    };
} // namespace

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
    std::string location() const override { return cfg.deviceName; }
    std::string timestampUtc() const override { return Core::isoUtc(epoch); }

    // --- Settings ----------------------------------------------------------
    std::string getConfig(bool redacted) const { return cfg.toJson(redacted); }

    // POST /api/config. Same validation and messages as the device.
    std::string applyConfig(const std::string &json)
    {
        std::string reason;
        auto next = Config::fromJson(json, &cfg, &reason);
        if (!next)
            return errorJson(reason.empty() ? "Invalid configuration" : reason);
        cfg = *next;
        return "{\"success\":true}";
    }

    // Restores saved demo state without the "changed by the UI" rules.
    bool loadConfig(const std::string &json)
    {
        auto next = Config::fromJson(json);
        if (!next)
            return false;
        cfg = *next;
        bootConfig = cfg;
        return true;
    }

    // POST /api/restart (applied by the demo after a short pause).
    void restart(double nowMsIn)
    {
        nowMs = static_cast<uint32_t>(nowMsIn);
        bootMs = nowMs;
        bootConfig = cfg;
        snapshot = SensorSnapshot{};
        safety = SafetyStatus{};
        delayFilter = Alpaca::SafeDelayFilter{};
        engine = Alerts::AlertEngine{};
        engineSeeded = false;
        records.clear();
        // The device forgets Alpaca connections; the pause state is kept.
        router.resetConnections();
        clientWatch.reset();
        const Alerts::ScheduleState kept = schedule.state();
        schedule.restore(kept.sending, true, static_cast<uint8_t>(kept.reason), kept.sinceEpoch);
        rainLatch = Rain::Latch{};
        SafetyHistory::startBoot(history, true);
        pushHistory(SafetyHistory::Kind::Boot, false, false, 0, 3 /* software reset */);
    }

    // --- One sensor cycle --------------------------------------------------
    // inputsJson: simulated sensors (web/src/demo/simulator.ts).
    void tick(double nowMsIn, double epochIn, const std::string &inputsJson, const std::string &localTime, const std::string &localDate)
    {
        // 0 means "never" in reading timestamps; the device's millis() is
        // never 0 by the time it reads a sensor.
        nowMs = std::max<uint32_t>(1, static_cast<uint32_t>(nowMsIn));
        epoch = static_cast<int64_t>(epochIn);
        clockTime = localTime;
        clockDate = localDate;
        if (bootMs == 0)
        {
            bootMs = nowMs == 0 ? 1 : nowMs;
            pushHistory(SafetyHistory::Kind::Boot, false, false, 0, 1 /* power on */);
        }

        DynamicJsonDocument in(2048);
        if (deserializeJson(in, inputsJson))
            return;
        readSensors(in.as<JsonObjectConst>());
        Core::derive(snapshot, cfg);

        const Alpaca::SafetyResult result = Alpaca::evaluateSafety(Core::safetyInputs(snapshot, cfg, nowMs), Core::safetyThresholds(cfg));
        if (Core::updateSafety(safety, delayFilter, result, cfg, nowMs))
            pushHistory(SafetyHistory::Kind::Change, safety.isSafe, !safety.isSafe && result.isSafe, result.reasonFlags, 0);

        runAlerts();
    }

    // --- Documents ---------------------------------------------------------
    // GET /api/sensors and /ws/sensors.
    std::string readings() const
    {
        DynamicJsonDocument doc(4096);
        Readings::write(doc.to<JsonObject>(), Core::buildReadings(snapshot, cfg, nowMs, epoch));
        Core::writeSafety(doc.createNestedObject("safety"), safety, cfg, nowMs);
        return serialize(doc);
    }

    // The decision parts of GET /api/status; the demo adds the hardware parts.
    std::string statusParts() const
    {
        DynamicJsonDocument doc(4096);
        doc["uptime"] = (nowMs - bootMs) / 1000;
        Core::writeSky(doc.createNestedObject("sky"), Core::night(snapshot, cfg, epoch));
        Core::writeSensorHealth(doc.createNestedObject("sensors"), Core::buildReadings(snapshot, cfg, nowMs, epoch), cfg);
        Core::writeDiagnostics(doc.createNestedObject("diagnostics"), snapshot, cfg, nowMs);
        Core::writeAlertSchedule(doc.createNestedObject("alerts"), schedule.state(), cfg, nowMs);
        Core::writeClientWatch(doc.createNestedObject("alpaca"), clientWatch, cfg, nowMs);
        return serialize(doc);
    }

    // GET /api/safety
    std::string safetyDocument() const
    {
        DynamicJsonDocument doc(1536);
        Core::writeSafety(doc.to<JsonObject>(), safety, cfg, nowMs);
        return serialize(doc);
    }

    // GET /api/safety/history
    std::string safetyHistory() const
    {
        SafetyHistory::Entry entries[SafetyHistory::CAPACITY];
        const size_t n = SafetyHistory::copy(history, entries, SafetyHistory::CAPACITY);
        SafetyHistory::backfillEpochs(entries, n, history.boot, static_cast<uint32_t>(epoch > 0 ? epoch : 0), uptimeSeconds());
        static const char *const KIND[] = {"boot", "change", "alert", "armed"};
        DynamicJsonDocument doc(256 + n * 160);
        doc["boot"] = history.boot;
        doc["uptime"] = uptimeSeconds();
        JsonArray list = doc.createNestedArray("entries");
        for (size_t i = n; i-- > 0;) // newest first
        {
            const SafetyHistory::Entry &e = entries[i];
            JsonObject item = list.createNestedObject();
            item["kind"] = KIND[static_cast<uint8_t>(e.kind) <= 3 ? static_cast<uint8_t>(e.kind) : 1];
            item["boot"] = e.boot;
            item["uptime"] = e.uptimeS;
            if (e.epoch != 0)
                item["timestamp"] = e.epoch;
            if (e.kind == SafetyHistory::Kind::Boot)
                item["resetReason"] = e.resetReason;
            else
                item["safe"] = static_cast<bool>(e.safe);
            if (e.kind == SafetyHistory::Kind::Change)
            {
                item["held"] = static_cast<bool>(e.held);
                item["reasonFlags"] = e.flags;
            }
        }
        return serialize(doc);
    }

    // GET /api/alerts/recent
    std::string recentAlerts() const
    {
        DynamicJsonDocument doc(16384);
        doc["enabled"] = cfg.alerts.enabled;
        doc["armed"] = schedule.state().sending;
        JsonArray arr = doc.createNestedArray("alerts");
        for (auto it = records.rbegin(); it != records.rend(); ++it)
        {
            JsonObject item = arr.createNestedObject();
            item["id"] = it->id;
            item["event"] = Alerts::alertTypeName(it->alert.type);
            item["title"] = it->alert.title;
            item["message"] = it->alert.message;
            item["level"] = Alerts::alertLevelName(it->alert.level);
            item["ageSeconds"] = uptimeSeconds() - it->uptimeSeconds;
            if (it->epochSeconds != 0)
                item["timestamp"] = it->epochSeconds;
            JsonObject channels = item.createNestedObject("channels");
            for (size_t i = 0; i < 4; ++i)
            {
                if (!it->channel[i])
                    continue;
                JsonObject ch = channels.createNestedObject(CHANNEL_NAMES[i]);
                ch["status"] = "sent";
                ch["detail"] = DEMO_DETAIL;
            }
        }
        return serialize(doc);
    }

    void clearAlerts() { records.clear(); }

    // --- Pause / resume (specs/021) ------------------------------------------
    bool isArmed() const { return schedule.state().sending; }
    // GET /api/alerts/armed
    std::string armedDocument() const
    {
        StaticJsonDocument<384> doc;
        Core::writeAlertSchedule(doc.to<JsonObject>(), schedule.state(), cfg, nowMs);
        return serialize(doc);
    }

    // POST /api/alerts/arm|disarm (source "ui" or "rest") and MQTT ("mqtt").
    void setArmed(bool on, const std::string &source)
    {
        const Alerts::ScheduleReason reason = source == "ui"     ? Alerts::ScheduleReason::UserUi
                                              : source == "mqtt" ? Alerts::ScheduleReason::UserMqtt
                                                                 : Alerts::ScheduleReason::UserRest;
        const bool wasSending = schedule.state().sending;
        if (schedule.command(on, reason, nowMs, validEpoch()))
            scheduleChanged(wasSending);
    }

private:
    int64_t validEpoch() const { return epoch >= Core::CLOCK_VALID_EPOCH ? epoch : 0; }

    void scheduleChanged(bool wasSending)
    {
        const bool sending = schedule.state().sending;
        if (sending == wasSending)
            return;
        pushHistory(SafetyHistory::Kind::Armed, sending, false, 0, 0);
        if (sending && cfg.alerts.enabled)
        {
            // One quiet line so you know where things stand as you start.
            Alerts::Alert alertsOn;
            alertsOn.type = Alerts::AlertType::AlertsOn;
            alertsOn.level = Alerts::AlertLevel::Quiet;
            alertsOn.title = "Alerts resumed";
            if (safety.evaluatedAtMs == 0)
                alertsOn.message = "Safety not evaluated yet.";
            else if (safety.isSafe)
                alertsOn.message = "Observatory safe.";
            else
                alertsOn.message =
                    "Observatory UNSAFE" + (safety.reasons.empty() ? std::string(".") : ":\n" + Alerts::joinReasons(safety.reasons));
            record(alertsOn, 0x0F);
        }
    }

public:
    // POST /api/alerts/test?channel=&event=&level=&sound=&title=&message=
    // Returns {status, body} like the device.
    std::string testAlert(const std::string &paramsJson)
    {
        StaticJsonDocument<1024> p;
        deserializeJson(p, paramsJson);
        const std::string channel = p["channel"] | "all";
        uint8_t mask = 0;
        if (channel == "all")
            mask = 0x0F;
        else if (channel == "mqtt")
            mask = 0x01;
        else if (channel == "pushover")
            mask = 0x02;
        else if (channel == "ntfy")
            mask = 0x04;
        else if (channel == "webhook")
            mask = 0x08;
        else
            return response(400, errorJson("Unknown channel (expected mqtt, pushover, ntfy, webhook or all)"));

        const Core::SampleAlert *sample = nullptr;
        uint8_t level = 2;
        std::string sound, title, message;
        if (p.containsKey("event"))
        {
            sample = Core::sampleAlert(p["event"] | "");
            if (sample == nullptr)
                return response(400, errorJson("Unknown event"));
            level = p["level"] | 2;
            if (level < 1 || level > 4)
                return response(400, errorJson("Level must be 1-4"));
            sound = p["sound"] | "";
            title = p["title"] | "";
            message = p["message"] | "";
            if (sound.size() > 32)
                return response(400, errorJson("Sound name too long"));
            if (title.size() > AlertsConfig::MAX_TEMPLATE_TITLE || message.size() > AlertsConfig::MAX_TEMPLATE_MESSAGE)
                return response(400, errorJson("Title is up to 80 characters and message up to 240"));
        }
        if ((mask & enabledChannels()) == 0)
            return response(400, errorJson("That channel isn't enabled - enable it and save settings first"));

        const Alerts::Alert test = Core::buildTestAlert(
            sample,
            level,
            sound,
            title,
            message,
            safety,
            cfg,
            Core::observingConditions(snapshot, cfg, nowMs),
            Core::night(snapshot, cfg, epoch),
            clockTime,
            clockDate);
        record(test, mask);
        return response(202, "{\"success\":true,\"message\":\"Test notification queued\",\"demo\":true}");
    }

    // POST /api/sensors/tsl2591/calibrate-dark - the device's checks.
    std::string calibrateDark()
    {
        const TSL2591Diagnostics &d = snapshot.tslDiagnostics;
        if (!snapshot.tslInitialized || d.sampleCount == 0)
            return response(409, errorJson("No light-sensor readings to calibrate from"));
        if (!d.nightMode)
            return response(409, errorJson("The sensor is seeing light. Cover it completely and wait for the averaging window to fill."));
        const uint16_t needed = Core::windowSamples(d);
        if (d.sampleCount < needed)
        {
            StaticJsonDocument<384> e;
            e["error"] = "The averaging window isn't full yet (" + std::to_string(d.sampleCount) + " of " + std::to_string(needed) +
                         " samples). Keep the sensor covered and try again.";
            e["sampleCount"] = d.sampleCount;
            e["windowSamples"] = needed;
            return response(409, serialize(e));
        }
        cfg.skyCalibration.darkVisibleOffset = d.rollingVisible;
        cfg.skyCalibration.darkFullOffset = 0.0f;
        cfg.skyCalibration.darkIrOffset = 0.0f;
        cfg.skyCalibration.darkSampleCount = d.sampleCount;
        cfg.skyCalibration.darkCalibratedAt = epoch >= Core::CLOCK_VALID_EPOCH ? epoch : nowMs;
        StaticJsonDocument<256> ok;
        ok["success"] = true;
        ok["darkVisibleOffset"] = cfg.skyCalibration.darkVisibleOffset;
        ok["sampleCount"] = cfg.skyCalibration.darkSampleCount;
        ok["darkCalibratedAt"] = cfg.skyCalibration.darkCalibratedAt;
        return response(200, serialize(ok));
    }

    // --- Alpaca --------------------------------------------------------------
    // method "GET"/"PUT", path, params as [[name, value], ...].
    std::string alpaca(const std::string &method, const std::string &path, const std::string &paramsJson)
    {
        Alpaca::Request request;
        request.get = method == "GET";
        request.put = method == "PUT";
        request.path = path;
        DynamicJsonDocument p(2048);
        if (!deserializeJson(p, paramsJson))
            for (JsonArrayConst pair : p.as<JsonArrayConst>())
                request.params.emplace_back(pair[0] | "", pair[1] | "");
        Alpaca::Response out;
        if (!router.handle(request, out))
            return response(404, errorJson("Not found"));
        DynamicJsonDocument doc(out.body.size() + 256);
        doc["status"] = out.status;
        doc["contentType"] = out.contentType;
        doc["body"] = out.body;
        return serialize(doc);
    }

    // --- Persistence (sessionStorage) -----------------------------------------
    std::string saveState() const
    {
        DynamicJsonDocument doc(24576);
        doc["config"] = serialized(cfg.toJson(false));
        const Alerts::ScheduleState &state = schedule.state();
        doc["armed"] = state.sending;
        doc["armedReason"] = static_cast<uint8_t>(state.reason);
        doc["armedSince"] = static_cast<double>(state.sinceEpoch);
        JsonArray list = doc.createNestedArray("history");
        SafetyHistory::Entry entries[SafetyHistory::CAPACITY];
        const size_t n = SafetyHistory::copy(history, entries, SafetyHistory::CAPACITY);
        for (size_t i = 0; i < n; ++i)
        {
            JsonArray e = list.createNestedArray();
            e.add(entries[i].epoch);
            e.add(entries[i].uptimeS);
            e.add(entries[i].flags);
            e.add(entries[i].boot);
            e.add(static_cast<uint8_t>(entries[i].kind));
            e.add(entries[i].safe);
            e.add(entries[i].held);
            e.add(entries[i].resetReason);
        }
        doc["boot"] = history.boot;
        return serialize(doc);
    }

    bool loadState(const std::string &json)
    {
        DynamicJsonDocument doc(json.size() * 2 + 4096);
        if (deserializeJson(doc, json))
            return false;
        std::string configJson;
        serializeJson(doc["config"], configJson);
        if (!loadConfig(configJson))
            return false;
        schedule = Alerts::AlertSchedule{};
        schedule.restore(
            doc["armed"] | true, doc.containsKey("armedReason"), doc["armedReason"] | 0, static_cast<int64_t>(doc["armedSince"] | 0.0));
        history = SafetyHistory::Log{};
        history.magic = SafetyHistory::MAGIC;
        for (JsonArrayConst e : doc["history"].as<JsonArrayConst>())
        {
            SafetyHistory::Entry entry{};
            entry.epoch = e[0] | 0u;
            entry.uptimeS = e[1] | 0u;
            entry.flags = e[2] | 0u;
            entry.boot = e[3] | 0;
            entry.kind = static_cast<SafetyHistory::Kind>(e[4] | 0);
            entry.safe = e[5] | 0;
            entry.held = e[6] | 0;
            entry.resetReason = e[7] | 0;
            SafetyHistory::push(history, entry);
        }
        history.boot = doc["boot"] | 1;
        return true;
    }

private:
    static Alpaca::ServerIdentity identity(const std::string &json)
    {
        StaticJsonDocument<256> doc;
        deserializeJson(doc, json);
        Alpaca::ServerIdentity id;
        id.serverName = "SQMeter";
        id.manufacturer = "SQMeter";
        id.version = doc["version"] | "demo";
        id.mac = std::strtoull(doc["mac"] | "a1b2c3d4e5f6", nullptr, 16);
        return id;
    }

    static std::string serialize(const JsonDocument &doc)
    {
        std::string out;
        serializeJson(doc, out);
        return out;
    }

    static std::string response(int status, const std::string &body)
    {
        DynamicJsonDocument doc(body.size() + 128);
        doc["status"] = status;
        doc["body"] = serialized(body);
        return serialize(doc);
    }

    uint32_t uptimeSeconds() const { return (nowMs - bootMs) / 1000; }

    uint8_t enabledChannels() const
    {
        return (cfg.alerts.mqttEnabled ? 0x01 : 0) | (cfg.alerts.pushoverEnabled ? 0x02 : 0) | (cfg.alerts.ntfyEnabled ? 0x04 : 0) |
               (cfg.alerts.webhookEnabled ? 0x08 : 0);
    }

    void record(const Alerts::Alert &alert, uint8_t mask)
    {
        Record r;
        r.id = nextId++;
        r.uptimeSeconds = uptimeSeconds();
        r.epochSeconds = epoch >= Core::CLOCK_VALID_EPOCH ? epoch : 0;
        r.alert = alert;
        const uint8_t send = mask & enabledChannels();
        for (size_t i = 0; i < 4; ++i)
            r.channel[i] = (send & (1u << i)) != 0;
        records.push_back(r);
        if (records.size() > MAX_RECORDS)
            records.pop_front();
    }

    void pushHistory(SafetyHistory::Kind kind, bool safe, bool held, uint32_t flags, uint8_t resetReason)
    {
        SafetyHistory::Entry e{};
        e.epoch = epoch >= Core::CLOCK_VALID_EPOCH ? static_cast<uint32_t>(epoch) : 0;
        e.uptimeS = uptimeSeconds();
        e.flags = flags;
        e.boot = history.boot;
        e.kind = kind;
        e.safe = safe;
        e.held = held;
        e.resetReason = resetReason & 0x3F;
        SafetyHistory::push(history, e);
    }

    void runAlerts()
    {
        if (!engineSeeded)
        {
            engineSeeded = true;
            bool toldSafe = false;
            if (SafetyHistory::lastAlert(history, toldSafe))
                engine.seedSafety(!toldSafe);
        }
        const Alpaca::ObservingConditionsSnapshot obs = Core::observingConditions(snapshot, cfg, nowMs);
        const Core::NightState night = Core::night(snapshot, cfg, epoch);
        Alerts::AlertInputs in = Core::alertInputs(safety, snapshot, obs, cfg, night, nowMs);
        in.nowSeconds = uptimeSeconds();

        // The imaging app: is each Alpaca device still being checked?
        const Alpaca::DeviceActivity activity[Alpaca::DEVICE_COUNT] = {
            router.activity(Alpaca::Device::SafetyMonitor), router.activity(Alpaca::Device::ObservingConditions)};
        uint32_t silenceMs[Alpaca::DEVICE_COUNT];
        Core::clientSilenceMs(cfg, silenceMs);
        clientWatch.update(activity, silenceMs, cfg.alpaca.enabled, nowMs);
        Core::addClientInputs(in, clientWatch, cfg, nowMs, clockTime);

        Core::AlertStep step = Core::runAlerts(engine, in, Core::alertRules(cfg), cfg, obs, night, safety, clockTime, clockDate);
        if (!step.outgoing.empty() && schedule.state().sending && cfg.alerts.enabled)
        {
            record(Alerts::stackAlerts(step.outgoing), 0x0F);
            for (const Alerts::Alert &sent : step.outgoing)
                if (sent.type == Alerts::AlertType::Unsafe || sent.type == Alerts::AlertType::Safe)
                    pushHistory(SafetyHistory::Kind::Alert, sent.type == Alerts::AlertType::Safe, false, 0, 0);
        }

        // After this pass's alerts: "disconnected" goes out before it pauses.
        const bool wasSending = schedule.state().sending;
        if (schedule.update(Core::sendMode(cfg), router.anyConnected(), nowMs, validEpoch()))
            scheduleChanged(wasSending);
    }

    // Simulated sensors -> the readings the drivers would produce.
    void readSensors(JsonObjectConst in)
    {
        const uint32_t now = nowMs;
        snapshot.dataTimestamp = now;
        snapshot.capturedAt = now;

        // TSL2591: detected at boot unless the demo says it's missing.
        JsonObjectConst light = in["light"];
        snapshot.tslInitialized = light["present"] | true;
        TSL2591Reading &tsl = snapshot.tsl;
        if (snapshot.tslInitialized && !(light["failed"] | false))
        {
            tsl.status = SensorStatus::OK;
            tsl.timestamp = now;
            snapshot.tslLastUpdate = now;
            tsl.rawLux = light["lux"] | 0.001f;
            tsl.rawSqm = SkyQuality::luxToSQM(tsl.rawLux);
            tsl.calibrated = cfg.skyCalibration.enabled;
            tsl.calibratedSqm = tsl.rawSqm + (cfg.skyCalibration.enabled ? cfg.skyCalibration.sqmOffset : 0.0f);
            tsl.lux = std::pow(10.0f, (12.6f - tsl.calibratedSqm) / 2.5f);
            tsl.visible = light["visible"] | 0;
            tsl.infrared = light["infrared"] | 0;
            tsl.full = light["full"] | 0;
            tsl.nightMode = light["nightMode"] | false;
        }
        else if (snapshot.tslInitialized)
        {
            tsl.status = SensorStatus::TIMEOUT;
        }
        TSL2591Diagnostics &d = snapshot.tslDiagnostics;
        d.gainName = tsl.nightMode ? "MAX" : "HIGH";
        d.gainFactor = tsl.nightMode ? 9876.0f : 428.0f;
        d.integrationMs = 600;
        d.averagingWindowSeconds = static_cast<uint16_t>(cfg.skyAveraging.windowSeconds);
        const uint32_t sinceBoot = now - bootMs;
        d.sampleCount = static_cast<uint16_t>(std::min<uint32_t>(sinceBoot / 600, Core::windowSamples(d)));
        d.rollingVisible = tsl.visible;
        d.darkVisibleOffset = cfg.skyCalibration.darkVisibleOffset;
        d.correctedVisible = std::max(0.0f, d.rollingVisible - d.darkVisibleOffset);
        d.rawSqm = tsl.rawSqm;
        d.calibratedSqm = tsl.calibratedSqm;
        d.nightMode = tsl.nightMode;
        d.calibrated = tsl.calibrated;
        d.saturated = false;

        // BME280
        JsonObjectConst env = in["environment"];
        snapshot.bmeInitialized = env["present"] | true;
        if (snapshot.bmeInitialized && !(env["failed"] | false))
        {
            BME280Reading &bme = snapshot.bme;
            bme.status = SensorStatus::OK;
            bme.timestamp = now;
            snapshot.bmeLastUpdate = now;
            bme.temperature = env["temperature"] | 10.0f;
            bme.humidity = env["humidity"] | 60.0f;
            bme.pressure = env["pressure"] | 1013.0f;
            bme.dewpoint = dewpointMagnus(bme.temperature, bme.humidity);
        }
        else if (snapshot.bmeInitialized)
        {
            snapshot.bme.status = SensorStatus::TIMEOUT;
        }

        // MLX90614
        JsonObjectConst ir = in["infrared"];
        snapshot.mlxInitialized = ir["present"] | true;
        if (snapshot.mlxInitialized && !(ir["failed"] | false))
        {
            MLX90614Reading &mlx = snapshot.mlx;
            mlx.status = SensorStatus::OK;
            mlx.timestamp = now;
            snapshot.mlxLastUpdate = now;
            mlx.objectTemp = ir["sky"] | -20.0f;
            mlx.ambientTemp = ir["ambient"] | 10.0f;
        }
        else if (snapshot.mlxInitialized)
        {
            snapshot.mlx.status = SensorStatus::TIMEOUT;
        }

        // GPS: the driver starts at boot, so it follows the settings at the last restart.
        snapshot.gpsInitialized = bootConfig.gps.enabled;
        GPSReading &gps = snapshot.gps;
        JsonObjectConst g = in["gps"];
        if (snapshot.gpsInitialized)
        {
            gps.status = SensorStatus::OK;
            gps.timestamp = now;
            snapshot.gpsLastUpdate = now;
            gps.hasFix = g["fix"] | true;
            gps.latitude = g["latitude"] | 0.0;
            gps.longitude = g["longitude"] | 0.0;
            gps.altitude = g["altitude"] | 0.0;
            gps.satellites = g["satellites"] | 0u;
            gps.hdop = 110;
            gps.age = 800;
        }
        else
        {
            gps = GPSReading{};
        }

        // RG-15: started and stopped with the setting.
        snapshot.rg15Initialized = cfg.rain.enabled;
        RG15Reading &rain = snapshot.rg15;
        JsonObjectConst r = in["rain"];
        if (cfg.rain.enabled && !(r["failed"] | false))
        {
            const float rateMm = r["rate"] | 0.0f;
            const bool imperial = cfg.rain.units == "imperial";
            const float scale = imperial ? 1.0f / 25.4f : 1.0f;
            const float dtHours = lastRainTickMs == 0 ? 0.0f : (now - lastRainTickMs) / 3600000.0f;
            lastRainTickMs = now;
            rain.status = SensorStatus::OK;
            rain.online = true;
            rain.stale = false;
            rain.timestamp = now;
            snapshot.rg15LastUpdate = now;
            rain.imperial = imperial;
            rain.rInt = rateMm * scale;
            rain.acc = rateMm * dtHours * scale;
            rain.eventAcc = rateMm > 0 ? rain.eventAcc + rain.acc : rain.eventAcc;
            rain.totalAcc += rain.acc;
            rain.isRaining = rateMm > 0;
            rain.lensBad = r["lensFault"] | false;
            rain.emSat = false;
            Rain::observe(rainLatch, rain.rInt, rain.acc, now, cfg.rain.rainClearDelayMs);
            rain.rainLatched = rainLatch.latched;
            rain.localEventAcc = rainLatch.eventAccumulation;
        }
        else
        {
            rain.online = false;
            rain.stale = cfg.rain.enabled;
            rain.status = cfg.rain.enabled ? SensorStatus::TIMEOUT : SensorStatus::NOT_INITIALIZED;
            lastRainTickMs = 0;
        }
        RG15Diagnostics &rd = snapshot.rg15Diagnostics;
        rd.state = cfg.rain.enabled ? (rain.online ? RG15State::RG15_ONLINE : RG15State::RG15_TIMEOUT) : RG15State::RG15_DISABLED;
        rd.uartOpened = cfg.rain.enabled;
        rd.rxPin = cfg.rain.rxPin;
        rd.txPin = cfg.rain.txPin;
        rd.baudRate = cfg.rain.baudRate;
        rd.uartPort = 1;
        rd.lastCommand = std::string("R");
        if (rain.online)
        {
            rd.lastPollMs = rd.lastResponseMs = rd.lastSuccessfulReadMs = now;
            rd.successfulReads++;
            char line[96];
            std::snprintf(
                line,
                sizeof(line),
                "Acc %.2f mm, EventAcc %.2f mm, TotalAcc %.2f mm, RInt %.2f mmph",
                rain.acc / (rain.imperial ? 1 / 25.4f : 1.0f),
                rain.eventAcc,
                rain.totalAcc,
                rain.rInt);
            rd.lastRawResponse = std::string(line);
        }
        rd.lastRainDetectedMs = rainLatch.lastRainMs;

        // Anemometer + vane
        WindReading &wind = snapshot.wind;
        JsonObjectConst w = in["wind"];
        if (cfg.wind.enabled && !(w["failed"] | false))
        {
            wind.status = SensorStatus::OK;
            wind.timestamp = now;
            wind.speedMs = w["speed"] | 0.0f;
            wind.gustMs = w["gust"] | wind.speedMs;
            wind.instantMs = wind.speedMs;
            wind.directionValid = cfg.wind.directionEnabled && wind.speedMs > 0.2f;
            wind.directionDeg = w["direction"] | 0.0f;
            wind.vaneFault = false;
            wind.samples = std::min<uint32_t>(600, (now - bootMs) / 1000);
        }
        else
        {
            wind = WindReading{};
            if (cfg.wind.enabled)
                wind.status = SensorStatus::TIMEOUT;
        }
    }

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
    Rain::Latch rainLatch;
    uint32_t lastRainTickMs = 0;
    uint32_t nowMs = 0;
    uint32_t bootMs = 0;
    int64_t epoch = 0;
    std::string clockTime = "--:--";
    std::string clockDate = "--";
    Alpaca::Router router;
};

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
        .function("safetyHistory", &EmulatedDevice::safetyHistory)
        .function("recentAlerts", &EmulatedDevice::recentAlerts)
        .function("clearAlerts", &EmulatedDevice::clearAlerts)
        .function("isArmed", &EmulatedDevice::isArmed)
        .function("armedDocument", &EmulatedDevice::armedDocument)
        .function("setArmed", &EmulatedDevice::setArmed)
        .function("testAlert", &EmulatedDevice::testAlert)
        .function("calibrateDark", &EmulatedDevice::calibrateDark)
        .function("alpaca", &EmulatedDevice::alpaca)
        .function("saveState", &EmulatedDevice::saveState)
        .function("loadState", &EmulatedDevice::loadState);
}
