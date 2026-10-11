// EmulatedDevice: the documents it serves and its saved state (see emulated_device.h).

#include "emulated_device.h"

using namespace DemoCore;

std::string EmulatedDevice::readings() const
{
    DynamicJsonDocument doc(4096);
    Readings::write(doc.to<JsonObject>(), Core::buildReadings(snapshot, cfg, nowMs, epoch));
    Core::writeSafety(doc.createNestedObject("safety"), safety, cfg, nowMs);
    return serialize(doc);
}

std::string EmulatedDevice::statusParts() const
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

std::string EmulatedDevice::effective() const
{
    const Deps::Facts f = facts();
    const std::vector<Deps::Entry> entries = Deps::evaluate(cfg, f);
    DynamicJsonDocument doc(Deps::reportCapacity(entries));
    Deps::writeReport(doc.to<JsonObject>(), entries, f);
    return serialize(doc);
}

std::string EmulatedDevice::pending() const
{
    DynamicJsonDocument doc(2048);
    JsonObject sky = doc.createNestedObject("skyAveraging");
    const bool nightMode = snapshot.tsl.nightMode;
    sky["nightMode"] = nightMode;
    const uint32_t windowSeconds = feed.lightWindowSeconds();
    const uint32_t lastStepMs = feed.lastLightStepMs();
    sky["windowSeconds"] = windowSeconds;
    uint32_t settlingMs = 0;
    if (nightMode && lastStepMs != 0 && nowMs - lastStepMs < windowSeconds * 1000UL)
        settlingMs = windowSeconds * 1000UL - (nowMs - lastStepMs);
    sky["settlingSeconds"] = (settlingMs + 999) / 1000;

    JsonObject rainClear = doc.createNestedObject("rainClear");
    const bool rainingNow = snapshot.rg15.online && snapshot.rg15.isRaining;
    const Rain::Latch &rainLatch = feed.rainLatch();
    rainClear["latched"] = rainLatch.latched;
    rainClear["rainingNow"] = rainingNow;
    if (rainLatch.latched && !rainingNow && rainLatch.lastRainMs != 0)
        rainClear["remainingSeconds"] = (Rain::clearRemainingMs(rainLatch, nowMs, cfg.rain.rainClearDelayMs) + 999) / 1000;

    JsonArray alerts = doc.createNestedArray("alerts");
    if (cfg.alerts.enabled)
        for (const Alerts::Wait &w : engine.waits(uptimeSeconds(), Core::alertRules(cfg)))
        {
            JsonObject item = alerts.createNestedObject();
            item["condition"] = w.condition;
            item["kind"] = Alerts::waitKindName(w.kind);
            item["remainingSeconds"] = w.remainingSeconds;
        }
    return serialize(doc);
}

std::string EmulatedDevice::safetyDocument() const
{
    DynamicJsonDocument doc(1536);
    Core::writeSafety(doc.to<JsonObject>(), safety, cfg, nowMs);
    return serialize(doc);
}

std::string EmulatedDevice::safetyHistory() const
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

std::string EmulatedDevice::recentAlerts() const
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
            if (it->skipped[i] != nullptr)
            {
                JsonObject ch = channels.createNestedObject(CHANNEL_NAMES[i]);
                ch["status"] = "skipped";
                ch["detail"] = it->skipped[i];
                continue;
            }
            if (!it->channel[i])
                continue;
            JsonObject ch = channels.createNestedObject(CHANNEL_NAMES[i]);
            ch["status"] = "sent";
            ch["detail"] = DEMO_DETAIL;
        }
    }
    return serialize(doc);
}

std::string EmulatedDevice::armedDocument() const
{
    StaticJsonDocument<384> doc;
    Core::writeAlertSchedule(doc.to<JsonObject>(), schedule.state(), cfg, nowMs);
    return serialize(doc);
}

std::string EmulatedDevice::deliveryRequests(uint32_t id, const std::string &credentialsJson) const
{
    const Record *found = nullptr;
    for (const Record &r : records)
        if (r.id == id)
            found = &r;
    DynamicJsonDocument out(4096);
    out.to<JsonObject>();
    StaticJsonDocument<1024> creds;
    if (found == nullptr || deserializeJson(creds, credentialsJson))
        return serialize(out);

    Alerts::Alert alert = found->alert;
    if (alert.level == Alerts::AlertLevel::Wake)
        alert.level = Alerts::AlertLevel::Urgent;
    const std::string &device = cfg.deviceName;
    auto wanted = [&](uint8_t bit, const char *name) { return (found->requested & bit) != 0 && creds.containsKey(name); };

    if (wanted(0x01, "mqtt"))
    {
        JsonObject mqtt = out.createNestedObject("mqtt");
        mqtt["topic"] = std::string(creds["mqtt"]["topic"] | "sqmeter") + "/alerts";
        mqtt["payload"] = Delivery::alertJson(alert, device, found->epochSeconds);
    }
    if (wanted(0x02, "pushover"))
        writeRequest(
            out.createNestedObject("pushover"),
            Delivery::pushoverRequest(
                alert, {creds["pushover"]["userKey"] | "", creds["pushover"]["appToken"] | "", cfg.alerts.pushoverSound}, device));
    if (wanted(0x04, "ntfy"))
        writeRequest(
            out.createNestedObject("ntfy"),
            Delivery::ntfyRequest(alert, {"https://ntfy.sh", creds["ntfy"]["topic"] | "", creds["ntfy"]["token"] | ""}, device));
    return serialize(out);
}

std::string EmulatedDevice::saveState() const
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

bool EmulatedDevice::loadState(const std::string &json)
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
