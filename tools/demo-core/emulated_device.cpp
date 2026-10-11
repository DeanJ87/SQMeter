// EmulatedDevice: settings, the sensor cycle, alerts and Alpaca (see emulated_device.h).

#include "emulated_device.h"

using namespace DemoCore;

std::string EmulatedDevice::applyConfig(const std::string &json)
{
    std::string reason;
    auto next = Config::fromJson(json, &cfg, &reason);
    if (!next)
        return errorJson(reason.empty() ? "Invalid configuration" : reason);
    cfg = *next;
    return "{\"success\":true}";
}

bool EmulatedDevice::loadConfig(const std::string &json)
{
    auto next = Config::fromJson(json);
    if (!next)
        return false;
    cfg = *next;
    bootConfig = cfg;
    return true;
}

void EmulatedDevice::restart(double nowMsIn)
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
    feed.restart();
    SafetyHistory::startBoot(history, true);
    pushHistory(SafetyHistory::Kind::Boot, false, false, 0, 3 /* software reset */);
}

void EmulatedDevice::tick(
    double nowMsIn, double epochIn, const std::string &inputsJson, const std::string &localTime, const std::string &localDate)
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
    feed.read(in.as<JsonObjectConst>());
    Core::derive(snapshot, cfg);

    const Alpaca::SafetyResult result = Alpaca::evaluateSafety(Core::safetyInputs(snapshot, cfg, nowMs), Core::safetyThresholds(cfg));
    if (Core::updateSafety(safety, delayFilter, result, cfg, nowMs))
        pushHistory(SafetyHistory::Kind::Change, safety.isSafe, !safety.isSafe && result.isSafe, result.reasonFlags, 0);

    runAlerts();
}

void EmulatedDevice::setArmed(bool on, const std::string &source)
{
    const Alerts::ScheduleReason reason = source == "ui"     ? Alerts::ScheduleReason::UserUi
                                          : source == "mqtt" ? Alerts::ScheduleReason::UserMqtt
                                                             : Alerts::ScheduleReason::UserRest;
    const bool wasSending = schedule.state().sending;
    if (schedule.command(on, reason, nowMs, validEpoch()))
        scheduleChanged(wasSending);
}

void EmulatedDevice::scheduleChanged(bool wasSending)
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

std::string EmulatedDevice::testAlert(const std::string &paramsJson)
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

    const Alpaca::ObservingConditionsSnapshot obs = Core::observingConditions(snapshot, cfg, nowMs);
    const Core::NightState night = Core::night(snapshot, cfg, epoch);
    const Alerts::Alert test = Core::buildTestAlert(
        sample,
        Core::TestWording{level, sound, title, message},
        Core::AlertSources{cfg, obs, night, safety},
        Core::LocalClock{clockTime, clockDate});
    record(test, mask);
    return response(202, "{\"success\":true,\"message\":\"Test notification queued\",\"demo\":true}");
}

void EmulatedDevice::realTestAlert()
{
    const Alpaca::ObservingConditionsSnapshot obs = Core::observingConditions(snapshot, cfg, nowMs);
    const Core::NightState night = Core::night(snapshot, cfg, epoch);
    const Alerts::Alert test = Core::buildTestAlert(
        nullptr, Core::TestWording{2, "", "", ""}, Core::AlertSources{cfg, obs, night, safety}, Core::LocalClock{clockTime, clockDate});
    record(test, 0x0F);
}

std::string EmulatedDevice::calibrateDark()
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

std::string EmulatedDevice::alpaca(const std::string &method, const std::string &path, const std::string &paramsJson)
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

Alpaca::ServerIdentity EmulatedDevice::identity(const std::string &json)
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

std::string EmulatedDevice::serialize(const JsonDocument &doc)
{
    std::string out;
    serializeJson(doc, out);
    return out;
}

std::string EmulatedDevice::response(int status, const std::string &body)
{
    DynamicJsonDocument doc(body.size() + 128);
    doc["status"] = status;
    doc["body"] = serialized(body);
    return serialize(doc);
}

void EmulatedDevice::writeRequest(JsonObject target, const Delivery::HttpRequest &request)
{
    target["url"] = request.url;
    target["contentType"] = request.contentType;
    JsonArray headers = target.createNestedArray("headers");
    for (const auto &h : request.headers)
    {
        JsonArray pair = headers.createNestedArray();
        pair.add(h.first);
        pair.add(h.second);
    }
    target["body"] = request.body;
}

Deps::Facts EmulatedDevice::facts() const
{
    Deps::Facts f;
    Core::sensorFacts(f, snapshot, Core::buildReadings(snapshot, cfg, nowMs, epoch));
    f.wifiConnected = true;
    f.mqttConnected = cfg.mqtt.enabled;
    f.clockSet = epoch >= Core::CLOCK_VALID_EPOCH;
    return f;
}

uint8_t EmulatedDevice::enabledChannels() const
{
    return (cfg.alerts.mqttEnabled ? 0x01 : 0) | (cfg.alerts.pushoverEnabled ? 0x02 : 0) | (cfg.alerts.ntfyEnabled ? 0x04 : 0) |
           (cfg.alerts.webhookEnabled ? 0x08 : 0);
}

void EmulatedDevice::record(const Alerts::Alert &alert, uint8_t mask)
{
    Record r;
    r.id = nextId++;
    r.uptimeSeconds = uptimeSeconds();
    r.epochSeconds = epoch >= Core::CLOCK_VALID_EPOCH ? epoch : 0;
    r.alert = alert;
    r.requested = mask;
    const uint8_t send = mask & enabledChannels();
    // Switched on but inactive (e.g. MQTT alerts with MQTT off): skipped
    // with the reason, like the device. Alerts being off doesn't block.
    const std::vector<Deps::Entry> entries = Deps::evaluate(cfg, facts());
    static const char *const SETTINGS[] = {
        "alerts.mqtt.enabled", "alerts.pushover.enabled", "alerts.ntfy.enabled", "alerts.webhook.enabled"};
    for (size_t i = 0; i < 4; ++i)
    {
        if ((send & (1u << i)) == 0)
            continue;
        const Deps::Reason *reason = Deps::reasonFor(entries, SETTINGS[i]); // dep: D-01 D-02 D-03
        if (reason != nullptr && std::string(reason->code) != "alerts-off")
            r.skipped[i] = reason->text;
        else
            r.channel[i] = true;
    }
    records.push_back(r);
    if (records.size() > MAX_RECORDS)
        records.pop_front();
}

void EmulatedDevice::pushHistory(SafetyHistory::Kind kind, bool safe, bool held, uint32_t flags, uint8_t resetReason)
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

void EmulatedDevice::runAlerts()
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
    const Core::AlertSources sources{cfg, obs, night, safety};
    Alerts::AlertInputs in = Core::alertInputs(sources, snapshot, nowMs);
    in.nowSeconds = uptimeSeconds();

    // The imaging app: is each Alpaca device still being checked?
    const Alpaca::DeviceActivity activity[Alpaca::DEVICE_COUNT] = {
        router.activity(Alpaca::Device::SafetyMonitor), router.activity(Alpaca::Device::ObservingConditions)};
    uint32_t silenceMs[Alpaca::DEVICE_COUNT];
    Core::clientSilenceMs(cfg, silenceMs);
    clientWatch.update(activity, silenceMs, cfg.alpaca.enabled, nowMs);
    Core::addClientInputs(in, clientWatch, cfg, nowMs, clockTime);

    Core::AlertStep step = Core::runAlerts(engine, in, Core::alertRules(cfg), sources, Core::LocalClock{clockTime, clockDate});
    if (!step.outgoing.empty() && schedule.state().sending && cfg.alerts.enabled)
    {
        record(Alerts::stackAlerts(step.outgoing), 0x0F);
        for (const Alerts::Alert &sent : step.outgoing)
            if (sent.type == Alerts::AlertType::Unsafe || sent.type == Alerts::AlertType::Safe)
                pushHistory(SafetyHistory::Kind::Alert, sent.type == Alerts::AlertType::Safe, false, 0, 0);
    }

    // After this pass's alerts: "disconnected" goes out before it pauses.
    const bool wasSending = schedule.state().sending;
    if (schedule.update(Core::effectiveSendMode(cfg), router.anyConnected(), nowMs, validEpoch()))
        scheduleChanged(wasSending);
}
