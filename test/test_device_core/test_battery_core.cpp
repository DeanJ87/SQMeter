#include <unity.h>

#include <ArduinoJson.h>

#include "DeviceCore.h"

// The battery through DeviceCore (specs/032-battery, contracts/readings.md):
// readings, /api/status, diagnostics, settings facts and the display page.

using namespace SQM;

namespace
{
    constexpr uint32_t NOW = 100000;

    SensorSnapshot withGauge(float percent = 82.4F, float rate = -6.2F)
    {
        SensorSnapshot s;
        s.battery.detected = s.battery.initialized = true;
        s.battery.lastUpdate = NOW - 1200;
        BatteryReading &b = s.battery.reading;
        b.status = SensorStatus::Ok;
        b.timestamp = NOW - 1200;
        b.version = 0x0012;
        b.voltage = 3.912F;
        b.percent = percent;
        b.estimating = false;
        b.rateValid = true;
        b.ratePercentPerHour = rate;
        b.chargeState = Battery::ChargeState::Discharging;
        return s;
    }

    Readings::Snapshot readingsOf(SensorSnapshot &s, const Config &cfg, uint32_t now = NOW)
    {
        Core::derive(s, cfg);
        return Core::buildReadings(s, cfg, now, 1791417600);
    }
} // namespace

void test_battery_readings_with_a_gauge()
{
    const Config cfg = Config::createDefault();
    SensorSnapshot s = withGauge();
    const Readings::Snapshot r = readingsOf(s, cfg);
    TEST_ASSERT_TRUE(r.battery.present);
    TEST_ASSERT_TRUE(r.battery.status == Readings::Status::Ok);
    TEST_ASSERT_EQUAL_UINT32(1200, r.battery.ageMs);
    TEST_ASSERT_FLOAT_WITHIN(1e-4, 82.4, *r.battery.percent);
    TEST_ASSERT_FLOAT_WITHIN(1e-4, -6.2, *r.battery.ratePercentPerHour);
    TEST_ASSERT_EQUAL_STRING("discharging", r.battery.state);
    TEST_ASSERT_FALSE(r.battery.usbPower.has_value());
    TEST_ASSERT_EQUAL_UINT32(797, *r.battery.timeToEmptyMinutes);
    TEST_ASSERT_FALSE(r.battery.timeToFullMinutes.has_value());
    TEST_ASSERT_FALSE(r.battery.powerSaving);

    // Estimating: no percent, no estimate.
    s.battery.reading.estimating = true;
    const Readings::Snapshot est = readingsOf(s, cfg);
    TEST_ASSERT_TRUE(est.battery.estimating);
    TEST_ASSERT_FALSE(est.battery.percent.has_value());
    TEST_ASSERT_FALSE(est.battery.timeToEmptyMinutes.has_value());
}

void test_battery_status_missing_error_stale()
{
    const Config cfg = Config::createDefault();
    SensorSnapshot s = withGauge();
    // Stale after the gauge's 5 s read interval plus the grace.
    TEST_ASSERT_TRUE(readingsOf(s, cfg, NOW - 1200 + 6000).battery.status == Readings::Status::Ok);
    TEST_ASSERT_TRUE(readingsOf(s, cfg, NOW - 1200 + 6001).battery.status == Readings::Status::Stale);
    // Power saving reads every 30 s.
    s.battery.power = Battery::PowerState::Saving;
    TEST_ASSERT_TRUE(readingsOf(s, cfg, NOW - 1200 + 31000).battery.status == Readings::Status::Ok);
    TEST_ASSERT_TRUE(readingsOf(s, cfg, NOW).battery.powerSaving);
    s.battery.power = Battery::PowerState::Normal;

    s.battery.reading.status = SensorStatus::ReadError;
    TEST_ASSERT_TRUE(readingsOf(s, cfg).battery.status == Readings::Status::Error);
    s.battery.initialized = false;
    TEST_ASSERT_TRUE(readingsOf(s, cfg).battery.status == Readings::Status::Missing);
}

void test_battery_monitoring_off_or_no_gauge_reports_absent()
{
    Config cfg = Config::createDefault();
    SensorSnapshot none;
    TEST_ASSERT_FALSE(readingsOf(none, cfg).battery.present);

    SensorSnapshot s = withGauge();
    cfg.battery.enabled = false;
    const Readings::Snapshot r = readingsOf(s, cfg);
    TEST_ASSERT_FALSE(r.battery.present);
    // Settings can still say a gauge is fitted.
    Deps::Facts facts;
    Core::sensorFacts(facts, s, r);
    TEST_ASSERT_TRUE(facts.batteryDetected);
    Deps::Facts noFacts;
    Core::sensorFacts(noFacts, none, readingsOf(none, cfg));
    TEST_ASSERT_FALSE(noFacts.batteryDetected);
}

void test_battery_charger_without_a_gauge()
{
    // Pack pulled out on USB: the gauge is gone, the charger says no battery.
    Config cfg = Config::createDefault();
    cfg.battery.stat1Pin = 13;
    cfg.battery.stat2Pin = 14;
    cfg.battery.pgPin = 32;
    SensorSnapshot s;
    s.battery.reading.chargerPins = true;
    s.battery.reading.chargeState = Battery::ChargeState::NoBattery;
    s.battery.reading.usbPower = true;
    const Readings::Snapshot r = readingsOf(s, cfg);
    TEST_ASSERT_TRUE(r.battery.present);
    TEST_ASSERT_TRUE(r.battery.status == Readings::Status::Missing);

    DynamicJsonDocument doc(1024);
    Readings::write(doc.to<JsonObject>(), r);
    JsonObject b = doc["battery"];
    TEST_ASSERT_EQUAL_STRING("missing", b["status"]);
    TEST_ASSERT_EQUAL_STRING("no-battery", b["state"]);
    TEST_ASSERT_TRUE(b["usbPower"].as<bool>());
}

void test_battery_status_health_and_diagnostics()
{
    Config cfg = Config::createDefault();
    cfg.battery.capacityMah = 7000;
    SensorSnapshot s = withGauge();
    s.battery.reading.gaugeAlert = false;
    s.battery.power = Battery::PowerState::ShutdownPending;
    s.battery.shutdownInSeconds = 25;
    const Readings::Snapshot r = readingsOf(s, cfg);

    DynamicJsonDocument doc(4096);
    JsonObject root = doc.to<JsonObject>();
    Core::writeBatteryStatus(root, s, r);
    Core::writeSensorHealth(root.createNestedObject("sensors"), r, cfg);
    Core::writeDiagnostics(root.createNestedObject("diagnostics"), s, cfg, NOW);
    TEST_ASSERT_TRUE(root["battery"]["present"].as<bool>());
    TEST_ASSERT_TRUE(root["battery"]["powerSaving"].as<bool>());
    TEST_ASSERT_EQUAL(25, root["battery"]["shutdownInSeconds"].as<int>());
    TEST_ASSERT_EQUAL_STRING("ok", root["sensors"]["battery"]["status"]);
    TEST_ASSERT_EQUAL(1200, root["sensors"]["battery"]["ageMs"].as<int>());
    JsonObject diag = root["diagnostics"]["battery"];
    TEST_ASSERT_EQUAL_STRING("0x0012", diag["version"]);
    TEST_ASSERT_TRUE(diag["charger"].isNull());
    TEST_ASSERT_FALSE(diag["gaugeAlert"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("shutdown", diag["policy"]);
    TEST_ASSERT_EQUAL(-434, diag["currentMa"].as<int>()); // -6.2 %/h of 7000 mAh

    // Not pending: no countdown. No gauge: no health, no diagnostics.
    s.battery.power = Battery::PowerState::Normal;
    DynamicJsonDocument normal(4096);
    Core::writeBatteryStatus(normal.to<JsonObject>(), s, readingsOf(s, cfg));
    TEST_ASSERT_FALSE(normal["battery"].containsKey("shutdownInSeconds"));
    SensorSnapshot none;
    const Readings::Snapshot nr = readingsOf(none, cfg);
    DynamicJsonDocument empty(2048);
    JsonObject e = empty.to<JsonObject>();
    Core::writeBatteryStatus(e, none, nr);
    Core::writeSensorHealth(e.createNestedObject("sensors"), nr, cfg);
    Core::writeDiagnostics(e.createNestedObject("diagnostics"), none, cfg, NOW);
    TEST_ASSERT_EQUAL(1, e["battery"].size());
    TEST_ASSERT_FALSE(e["sensors"].containsKey("battery"));
    TEST_ASSERT_FALSE(e["diagnostics"].containsKey("battery"));
}

void test_display_battery_page()
{
    const Config cfg = Config::createDefault();
    SensorSnapshot s = withGauge(82.6F);
    s.battery.reading.chargeState = Battery::ChargeState::Charging;
    const Readings::Snapshot r = readingsOf(s, cfg);
    const Display::View v = Core::displayView(s, r, SafetyStatus{}, cfg, Core::DisplayNetwork{});
    TEST_ASSERT_TRUE(v.battery.available);
    TEST_ASSERT_TRUE(v.battery.valid);
    TEST_ASSERT_EQUAL_UINT8(83, v.battery.percent);
    TEST_ASSERT_EQUAL_STRING("Chg", v.battery.state);
    TEST_ASSERT_TRUE(v.available(Display::Page::Battery));

    s.battery.reading.estimating = true;
    const Display::View est = Core::displayView(s, readingsOf(s, cfg), SafetyStatus{}, cfg, Core::DisplayNetwork{});
    TEST_ASSERT_TRUE(est.battery.available);
    TEST_ASSERT_FALSE(est.battery.valid);

    SensorSnapshot none;
    const Display::View nv = Core::displayView(none, readingsOf(none, cfg), SafetyStatus{}, cfg, Core::DisplayNetwork{});
    TEST_ASSERT_FALSE(nv.battery.available);
    TEST_ASSERT_FALSE(nv.available(Display::Page::Battery));
}

// Safety and alerts through DeviceCore (spec 032 US2).

void test_battery_safety_mapping()
{
    Config cfg = Config::createDefault();
    cfg.alpaca.batteryEnabled = true;
    cfg.alpaca.batteryMinPercent = 25;
    cfg.battery.shutdownPercent = 6;
    const Alpaca::SafetyThresholds t = Core::safetyThresholds(cfg);
    TEST_ASSERT_TRUE(t.batteryEnabled);
    TEST_ASSERT_EQUAL_FLOAT(25.0F, t.batteryMinPercent);

    SensorSnapshot s = withGauge(12.0F);
    Core::derive(s, cfg);
    Alpaca::SafetyInputs in = Core::safetyInputs(s, cfg, NOW);
    TEST_ASSERT_TRUE(in.batteryValid);
    TEST_ASSERT_EQUAL_FLOAT(12.0F, in.batteryPercent);
    TEST_ASSERT_FALSE(in.shutdownPending);
    TEST_ASSERT_EQUAL_FLOAT(6.0F, in.shutdownPercent);
    TEST_ASSERT_TRUE((Alpaca::evaluateSafety(in, t).reasonFlags & Alpaca::UnsafeBatteryLow) != 0);

    // Stale past alpaca.staleAfterSeconds, estimating, or monitoring off: not valid.
    TEST_ASSERT_FALSE(Core::safetyInputs(s, cfg, NOW - 1200 + 30001).batteryValid);
    s.battery.reading.estimating = true;
    TEST_ASSERT_FALSE(Core::safetyInputs(s, cfg, NOW).batteryValid);
    s.battery.reading.estimating = false;
    Config off = cfg;
    off.battery.enabled = false;
    Core::derive(s, off);
    TEST_ASSERT_FALSE(Core::safetyInputs(s, off, NOW).batteryValid);

    Core::derive(s, cfg);
    s.battery.power = Battery::PowerState::ShutdownPending;
    TEST_ASSERT_TRUE(Core::safetyInputs(s, cfg, NOW).shutdownPending);
}

namespace
{
    struct AlertRig
    {
        Config cfg;
        SensorSnapshot snapshot;
        SafetyStatus status;
        Alpaca::ObservingConditionsSnapshot obs;
        Core::NightState night;
        Alerts::AlertEngine engine;

        Core::AlertStep pass(uint32_t now)
        {
            Core::derive(snapshot, cfg);
            obs = Core::observingConditions(snapshot, cfg, now);
            const Core::AlertSources sources{cfg, obs, night, status};
            return Core::runAlerts(engine, Core::alertInputs(sources, snapshot, now), Core::alertRules(cfg), sources, Core::LocalClock{});
        }
    };

    const Alerts::Alert *find(const Core::AlertStep &step, Alerts::AlertType type)
    {
        for (const Alerts::Alert &a : step.outgoing)
            if (a.type == type)
                return &a;
        return nullptr;
    }
} // namespace

void test_battery_events_take_their_levels_and_ring_phones()
{
    AlertRig rig;
    rig.cfg = Config::createDefault();
    rig.cfg.alerts.enabled = true;
    rig.status.evaluatedAtMs = NOW;
    rig.status.isSafe = rig.status.rawSafe = true;
    rig.snapshot = withGauge(50.0F);
    rig.pass(NOW);
    // Past the 60 s start-up grace, the charge drops below low (Normal) and critical (Wake).
    rig.snapshot = withGauge(8.0F);
    rig.snapshot.battery.lastUpdate = rig.snapshot.battery.reading.timestamp = NOW + 70000;
    const Core::AlertStep step = rig.pass(NOW + 70000);
    const Alerts::Alert *low = find(step, Alerts::AlertType::BatteryLow);
    const Alerts::Alert *critical = find(step, Alerts::AlertType::BatteryCritical);
    TEST_ASSERT_NOT_NULL(low);
    TEST_ASSERT_NOT_NULL(critical);
    TEST_ASSERT_EQUAL(static_cast<int>(Alerts::AlertLevel::Normal), static_cast<int>(low->level));
    TEST_ASSERT_EQUAL(static_cast<int>(Alerts::AlertLevel::Wake), static_cast<int>(critical->level));
    TEST_ASSERT_TRUE((step.alarmFlags & Alpaca::UnsafeBatteryLow) != 0);
}

void test_battery_rule_alert_and_phone_alarm()
{
    AlertRig rig;
    rig.cfg = Config::createDefault();
    rig.cfg.alerts.enabled = true;
    rig.cfg.alpaca.batteryEnabled = true;
    rig.status.evaluatedAtMs = NOW;
    rig.status.isSafe = rig.status.rawSafe = true;
    rig.snapshot = withGauge(50.0F);
    rig.pass(NOW);
    rig.status.isSafe = rig.status.rawSafe = false;
    rig.status.reasonFlags = Alpaca::UnsafeBatteryData;
    rig.status.reasons = {"Battery data missing"};
    Core::AlertStep step = rig.pass(NOW + 70000);
    const Alerts::Alert *rule = find(step, Alerts::AlertType::BatteryRule);
    TEST_ASSERT_NOT_NULL(rule);
    TEST_ASSERT_EQUAL(static_cast<int>(Alerts::AlertLevel::Urgent), static_cast<int>(rule->level));
    TEST_ASSERT_EQUAL_STRING("Battery data missing", rule->message.c_str());
    TEST_ASSERT_NOT_NULL(find(step, Alerts::AlertType::Unsafe)); // stacked with the generic one by the dispatcher
    TEST_ASSERT_TRUE((step.alarmFlags & Alpaca::UnsafeBatteryData) != 0);
    TEST_ASSERT_TRUE(step.ringPhones);

    // Options off: neither the alert nor the ring.
    AlertRig quiet;
    quiet.cfg = rig.cfg;
    quiet.cfg.alpaca.batteryAlert = false;
    quiet.cfg.alpaca.batteryPhoneAlarm = false;
    quiet.status.evaluatedAtMs = NOW;
    quiet.status.isSafe = quiet.status.rawSafe = true;
    quiet.snapshot = withGauge(50.0F);
    quiet.pass(NOW);
    quiet.status = rig.status;
    step = quiet.pass(NOW + 70000);
    TEST_ASSERT_NULL(find(step, Alerts::AlertType::BatteryRule));
    TEST_ASSERT_EQUAL_UINT32(0, step.alarmFlags & Alpaca::UnsafeBatteryData);
}

void test_battery_event_settings_and_samples()
{
    const Config cfg = Config::createDefault();
    const char *keys[] = {"battery_low", "battery_critical", "battery_shutdown", "power_restored", "charge_complete", "charge_fault"};
    const AlertsConfig::EventSetting *settings[] = {
        &cfg.alerts.batteryLow,
        &cfg.alerts.batteryCritical,
        &cfg.alerts.batteryShutdown,
        &cfg.alerts.powerRestored,
        &cfg.alerts.chargeComplete,
        &cfg.alerts.chargeFault};
    for (size_t i = 0; i < 6; ++i)
    {
        const Core::SampleAlert *sample = Core::sampleAlert(keys[i]);
        TEST_ASSERT_NOT_NULL_MESSAGE(sample, keys[i]);
        TEST_ASSERT_EQUAL_STRING(keys[i], Alerts::alertTypeName(sample->type));
        TEST_ASSERT_TRUE(Core::eventSettingFor(cfg.alerts, sample->type) == settings[i]);
    }
    TEST_ASSERT_EQUAL_UINT32(Alpaca::UnsafeShutdown, Core::sampleAlert("battery_shutdown")->bleFlags);
    TEST_ASSERT_NULL(Core::eventSettingFor(cfg.alerts, Alerts::AlertType::BatteryRule));
}
