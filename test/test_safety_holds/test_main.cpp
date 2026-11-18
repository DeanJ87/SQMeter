#include <unity.h>

#include <string>

#include "DeviceCore.h"
#include "SafetyEvaluator.h"
#include "SafetyHistoryLog.h"
#include "SafetyHolds.h"

// Safety holds (specs/035-safety-holds).

using namespace SQM;
using namespace SQM::Alpaca;

void setUp() {}
void tearDown() {}

namespace
{
    constexpr int64_t EPOCH = 1800000000; // a valid wall clock
    constexpr uint32_t MINUTE_MS = 60000;
    const char *const SMOKE = "Smoke: neighbour's fire, PM2.5 85 \xC2\xB5g/m\xC2\xB3";

    HoldRequest request(const std::string &id, const std::string &reason, uint32_t expiresMin = 0, const std::string &source = "HA")
    {
        HoldRequest r;
        r.id = id;
        r.reason = reason;
        r.source = source;
        r.expiresMin = expiresMin;
        return r;
    }

    // Fresh, in-range readings: safe by every rule but the holds.
    SafetyInputs safeInputs()
    {
        SafetyInputs in;
        in.hasEverHadGoodData = true;
        in.cloudCoverPercent = 10.0f;
        return in;
    }

    SafetyThresholds noRain()
    {
        SafetyThresholds t;
        t.rainUnsafeEnabled = false;
        t.rainSensorRequired = false;
        return t;
    }
} // namespace

void test_hold_id_rules()
{
    TEST_ASSERT_TRUE(validHoldId("smoke"));
    TEST_ASSERT_TRUE(validHoldId("roof-1_a"));
    TEST_ASSERT_TRUE(validHoldId(std::string(24, 'a')));
    TEST_ASSERT_FALSE(validHoldId(""));
    TEST_ASSERT_FALSE(validHoldId(std::string(25, 'a')));
    TEST_ASSERT_FALSE(validHoldId("Smoke"));
    TEST_ASSERT_FALSE(validHoldId("smo ke"));
    TEST_ASSERT_FALSE(validHoldId("a/b"));
}

void test_bad_requests_are_refused_and_change_nothing()
{
    SafetyHolds holds;
    TEST_ASSERT_EQUAL(HoldError::BadId, holds.set(request("Bad", "x"), 0, EPOCH));
    TEST_ASSERT_EQUAL(HoldError::BadReason, holds.set(request("smoke", ""), 0, EPOCH));
    TEST_ASSERT_EQUAL(HoldError::BadReason, holds.set(request("smoke", std::string(97, 'x')), 0, EPOCH));
    TEST_ASSERT_EQUAL(HoldError::BadReason, holds.set(request("smoke", "line\nbreak"), 0, EPOCH));
    TEST_ASSERT_EQUAL(HoldError::BadSource, holds.set(request("smoke", "x", 0, std::string(33, 's')), 0, EPOCH));
    TEST_ASSERT_EQUAL(HoldError::BadExpiry, holds.set(request("smoke", "x", 10081), 0, EPOCH));
    TEST_ASSERT_EQUAL(0, holds.all().size());
}

void test_reason_limit_counts_characters_not_bytes()
{
    SafetyHolds holds;
    std::string reason;
    for (int i = 0; i < 96; ++i)
        reason += "\xC2\xB5"; // µ: 2 bytes, 1 character
    TEST_ASSERT_EQUAL(HoldError::None, holds.set(request("smoke", reason), 0, EPOCH));
    TEST_ASSERT_EQUAL(HoldError::BadReason, holds.set(request("dust", reason + "x"), 0, EPOCH));
}

void test_at_most_eight_holds()
{
    SafetyHolds holds;
    for (int i = 0; i < 8; ++i)
        TEST_ASSERT_EQUAL(HoldError::None, holds.set(request("h" + std::to_string(i), "x"), 0, EPOCH));
    TEST_ASSERT_EQUAL(HoldError::Full, holds.set(request("h8", "x"), 0, EPOCH));
    // Updating one that exists is not adding one.
    TEST_ASSERT_EQUAL(HoldError::None, holds.set(request("h3", "y"), 0, EPOCH));
    TEST_ASSERT_EQUAL(8, holds.all().size());
}

void test_setting_again_updates_reason_and_keeps_since()
{
    SafetyHolds holds;
    bool added = false;
    holds.set(request("smoke", "Smoke"), 1000, EPOCH, &added);
    TEST_ASSERT_TRUE(added);
    holds.set(request("smoke", "Smoke: worse", 0, "Node-RED"), 5000, EPOCH + 600, &added);
    TEST_ASSERT_FALSE(added);
    TEST_ASSERT_EQUAL(1, holds.all().size());
    const SafetyHold *hold = holds.find("smoke");
    TEST_ASSERT_EQUAL_STRING("Smoke: worse", hold->reason.c_str());
    TEST_ASSERT_EQUAL_STRING("Node-RED", hold->source.c_str());
    TEST_ASSERT_EQUAL_INT64(EPOCH, hold->sinceEpoch);
}

void test_clearing_one_hold_keeps_the_others()
{
    SafetyHolds holds;
    holds.set(request("smoke", SMOKE), 0, EPOCH);
    holds.set(request("roof", "Roof not fully open"), 0, EPOCH);
    TEST_ASSERT_TRUE(holds.clear("smoke"));
    TEST_ASSERT_NULL(holds.find("smoke"));
    TEST_ASSERT_NOT_NULL(holds.find("roof"));
}

void test_clearing_an_unknown_hold_changes_nothing()
{
    SafetyHolds holds;
    holds.set(request("roof", "Roof not fully open"), 0, EPOCH);
    TEST_ASSERT_FALSE(holds.clear("smoke"));
    TEST_ASSERT_EQUAL(1, holds.all().size());
}

void test_holds_are_unsafe_and_immediate()
{
    SafetyHolds holds;
    holds.set(request("smoke", SMOKE), 0, EPOCH);
    SafetyInputs in = safeInputs();
    in.holdReasons = holds.externalReasons();
    const SafetyResult result = evaluateSafety(in, noRain());
    TEST_ASSERT_FALSE(result.isSafe);
    TEST_ASSERT_EQUAL_UINT32(UnsafeExternalHold, result.reasonFlags);
    TEST_ASSERT_EQUAL(1, result.unsafeReasons.size());
    TEST_ASSERT_EQUAL_STRING(SMOKE, result.unsafeReasons[0].c_str());

    // Reported safe with a long unsafe delay: a hold still goes unsafe at once.
    SafeDelayFilter filter;
    TEST_ASSERT_TRUE(filter.update(true, 0, 0, 600));
    TEST_ASSERT_FALSE(filter.update(false, 1000, 0, 600, result.reasonFlags));
}

void test_clearing_a_hold_never_forces_safe()
{
    SafetyInputs in = safeInputs();
    in.cloudCoverPercent = 95.0f;
    in.holdReasons = {SMOKE};
    const SafetyResult held = evaluateSafety(in, noRain());
    TEST_ASSERT_EQUAL_UINT32(UnsafeExternalHold | UnsafeCloudCover, held.reasonFlags);
    in.holdReasons.clear();
    const SafetyResult cleared = evaluateSafety(in, noRain());
    TEST_ASSERT_FALSE(cleared.isSafe);
    TEST_ASSERT_EQUAL_UINT32(UnsafeCloudCover, cleared.reasonFlags);
}

void test_cleared_hold_waits_out_the_safe_delay()
{
    SafeDelayFilter filter;
    TEST_ASSERT_FALSE(filter.update(false, 0, 60, 0, UnsafeExternalHold));
    TEST_ASSERT_FALSE(filter.update(true, 1000, 60, 0, 0)); // hold cleared
    TEST_ASSERT_FALSE(filter.update(true, 59000, 60, 0, 0));
    TEST_ASSERT_TRUE(filter.update(true, 61000, 60, 0, 0));
}

void test_manual_hold_keeps_its_reason_and_bit()
{
    SafetyHolds holds;
    TEST_ASSERT_EQUAL(HoldError::None, holds.set(request(MANUAL_HOLD_ID, "anything"), 0, EPOCH));
    TEST_ASSERT_EQUAL_STRING(MANUAL_HOLD_REASON, holds.find(MANUAL_HOLD_ID)->reason.c_str());
    TEST_ASSERT_FALSE(holds.hasExternal());
    TEST_ASSERT_EQUAL(0, holds.externalReasons().size());
}

void test_expiry_with_a_valid_clock()
{
    SafetyHolds holds;
    holds.set(request("smoke", SMOKE, 30), 0, EPOCH);
    TEST_ASSERT_EQUAL_INT64(EPOCH + 1800, holds.find("smoke")->expiresEpoch);
    TEST_ASSERT_EQUAL(0, holds.update(29 * MINUTE_MS, EPOCH + 29 * 60).size());
    const std::vector<SafetyHold> lifted = holds.update(30 * MINUTE_MS, EPOCH + 30 * 60);
    TEST_ASSERT_EQUAL(1, lifted.size());
    TEST_ASSERT_EQUAL_STRING("smoke", lifted[0].id.c_str());
    TEST_ASSERT_EQUAL(0, holds.all().size());
}

void test_expiry_without_a_clock_counts_device_time()
{
    SafetyHolds holds;
    holds.set(request("smoke", SMOKE, 10), 5000, 0);
    TEST_ASSERT_EQUAL_INT64(0, holds.find("smoke")->expiresEpoch);
    TEST_ASSERT_EQUAL(0, holds.update(5000 + 10 * MINUTE_MS - 1, 0).size());
    TEST_ASSERT_EQUAL(1, holds.update(5000 + 10 * MINUTE_MS, 0).size());
}

void test_holds_survive_a_restart()
{
    SafetyHolds before;
    before.set(request("smoke", SMOKE, 0, "Home Assistant"), 0, EPOCH);
    before.set(request("roof", "Roof not fully open", 60), 0, EPOCH);
    SafetyHolds after;
    TEST_ASSERT_TRUE(after.restore(before.serialize(), 1000));
    TEST_ASSERT_EQUAL(2, after.all().size());
    TEST_ASSERT_EQUAL_STRING(SMOKE, after.find("smoke")->reason.c_str());
    TEST_ASSERT_EQUAL_STRING("Home Assistant", after.find("smoke")->source.c_str());
    TEST_ASSERT_EQUAL_INT64(EPOCH, after.find("smoke")->sinceEpoch);
    TEST_ASSERT_EQUAL_INT64(EPOCH + 3600, after.find("roof")->expiresEpoch);
}

void test_restored_expiry_lifts_at_its_clock_time()
{
    SafetyHolds before;
    before.set(request("roof", "Roof", 60), 0, EPOCH);
    SafetyHolds after;
    after.restore(before.serialize(), 0);                              // restarted 50 minutes later
    TEST_ASSERT_EQUAL(0, after.update(1000, 0).size());                // no clock yet: kept
    TEST_ASSERT_EQUAL(0, after.update(20000, EPOCH + 59 * 60).size()); // clock set, 1 min to go
    TEST_ASSERT_EQUAL(1, after.update(80000, EPOCH + 60 * 60).size()); // its time
}

void test_restored_expiry_without_a_clock_never_lifts_early()
{
    SafetyHolds before;
    before.set(request("roof", "Roof", 60), 0, EPOCH);
    SafetyHolds after;
    after.restore(before.serialize(), 2000);
    // The clock is never set: the full hour from boot.
    TEST_ASSERT_EQUAL(0, after.update(2000 + 60 * MINUTE_MS - 1, 0).size());
    TEST_ASSERT_EQUAL(1, after.update(2000 + 60 * MINUTE_MS, 0).size());
}

void test_restore_drops_bad_entries()
{
    SafetyHolds holds;
    TEST_ASSERT_TRUE(holds.restore("{\"h\":[{\"i\":\"BAD\",\"r\":\"x\",\"s\":\"a\"},{\"i\":\"ok\",\"r\":\"x\",\"s\":\"a\"}]}", 0));
    TEST_ASSERT_EQUAL(1, holds.all().size());
    TEST_ASSERT_FALSE(holds.restore("not json", 0));
    TEST_ASSERT_EQUAL(0, holds.all().size());
}

void test_since_is_dated_once_the_clock_is_set()
{
    SafetyHolds holds;
    holds.set(request("smoke", SMOKE), 10000, 0);
    holds.update(70000, EPOCH);
    TEST_ASSERT_EQUAL_INT64(EPOCH - 60, holds.find("smoke")->sinceEpoch);
}

void test_hold_json_shapes()
{
    SafetyHolds holds;
    holds.set(request("smoke", SMOKE, 0, "Home Assistant"), 1000, 0);
    holds.set(request("roof", "Roof", 60), 1000, EPOCH);
    DynamicJsonDocument doc(2048);
    holds.writeJson(doc.to<JsonArray>(), 4000, EPOCH);
    TEST_ASSERT_EQUAL_STRING("smoke", doc[0]["id"]);
    TEST_ASSERT_EQUAL_STRING("Home Assistant", doc[0]["source"]);
    TEST_ASSERT_EQUAL(3000, doc[0]["sinceAgeMs"].as<uint32_t>());
    TEST_ASSERT_FALSE(doc[0].containsKey("expiresEpoch"));
    TEST_ASSERT_EQUAL_INT64(EPOCH, doc[1]["sinceEpoch"].as<int64_t>());
    TEST_ASSERT_EQUAL_INT64(EPOCH + 3600, doc[1]["expiresEpoch"].as<int64_t>());
    TEST_ASSERT_EQUAL(3600000 - 3000, doc[1]["expiresInMs"].as<uint32_t>());
}

void test_hold_label_is_the_reason_up_to_its_colon()
{
    TEST_ASSERT_EQUAL_STRING("Smoke", holdLabel(SMOKE, 39).c_str());
    TEST_ASSERT_EQUAL_STRING("Roof not fully open", holdLabel("Roof not fully open", 39).c_str());
    TEST_ASSERT_EQUAL_STRING(": x", holdLabel(": x", 39).c_str());
    // Cut on a character boundary.
    TEST_ASSERT_EQUAL_STRING("a", holdLabel("a\xC2\xB5", 2).c_str());
}

void test_parse_request_defaults_and_clear()
{
    HoldRequest r;
    TEST_ASSERT_EQUAL(HoldError::None, parseHoldRequest("{\"id\":\"smoke\",\"reason\":\"Smoke\",\"expiresMin\":30}", "API", r));
    TEST_ASSERT_EQUAL_STRING("API", r.source.c_str());
    TEST_ASSERT_EQUAL(30, r.expiresMin);
    TEST_ASSERT_EQUAL(HoldError::BadExpiry, parseHoldRequest("{\"id\":\"smoke\",\"reason\":\"x\",\"expiresMin\":0}", "API", r));
    TEST_ASSERT_EQUAL(HoldError::BadExpiry, parseHoldRequest("{\"id\":\"smoke\",\"reason\":\"x\",\"expiresMin\":\"5\"}", "API", r));
    TEST_ASSERT_EQUAL(HoldError::BadJson, parseHoldRequest("smoke", "API", r));
    TEST_ASSERT_EQUAL(HoldError::BadId, parseHoldRequest("{\"reason\":\"x\"}", "API", r));
    TEST_ASSERT_EQUAL(HoldError::None, parseHoldRequest("{\"id\":\"smoke\",\"clear\":true}", "MQTT", r));
    TEST_ASSERT_TRUE(r.clear);
}

void test_http_routes()
{
    Core::HoldCommand c;
    TEST_ASSERT_TRUE(Core::holdCommandFromHttp(false, "/api/safety/hold", "{\"id\":\"smoke\",\"reason\":\"x\"}", "ha", c));
    TEST_ASSERT_EQUAL(Core::HoldCommand::Kind::Set, c.kind);
    TEST_ASSERT_EQUAL_STRING("ha", c.request.source.c_str());
    TEST_ASSERT_TRUE(Core::holdCommandFromHttp(true, "/api/safety/hold/smoke", "", "", c));
    TEST_ASSERT_EQUAL(Core::HoldCommand::Kind::Clear, c.kind);
    TEST_ASSERT_EQUAL_STRING("smoke", c.request.id.c_str());
    TEST_ASSERT_TRUE(Core::holdCommandFromHttp(false, "/api/safety/hold/roof/clear", "", "", c));
    TEST_ASSERT_EQUAL(Core::HoldCommand::Kind::Clear, c.kind);
    TEST_ASSERT_EQUAL_STRING("roof", c.request.id.c_str());
    TEST_ASSERT_FALSE(Core::holdCommandFromHttp(false, "/api/safety/hold/roof", "", "", c));
    TEST_ASSERT_FALSE(Core::holdCommandFromHttp(true, "/api/safety/hold", "", "", c));
    TEST_ASSERT_TRUE(Core::holdCommandFromHttp(true, "/api/safety/hold/Bad", "", "", c));
    TEST_ASSERT_EQUAL(HoldError::BadId, c.error);
}

void test_apply_answers()
{
    SafetyHolds holds;
    Core::HoldCommand set = Core::holdCommandFromMqtt("{\"id\":\"smoke\",\"reason\":\"Smoke\"}");
    Core::HoldOutcome outcome = Core::applyHold(holds, set, 0, EPOCH);
    TEST_ASSERT_EQUAL(200, outcome.status);
    TEST_ASSERT_TRUE(outcome.changed);
    TEST_ASSERT_TRUE(outcome.body.find("\"source\":\"MQTT\"") != std::string::npos);

    Core::HoldCommand clear = Core::holdCommandFromMqtt("{\"id\":\"smoke\",\"clear\":true}");
    outcome = Core::applyHold(holds, clear, 0, EPOCH);
    TEST_ASSERT_EQUAL(200, outcome.status);
    TEST_ASSERT_EQUAL_STRING("{\"cleared\":true}", outcome.body.c_str());
    TEST_ASSERT_TRUE(outcome.cleared);

    outcome = Core::applyHold(holds, clear, 0, EPOCH);
    TEST_ASSERT_EQUAL(404, outcome.status);
    TEST_ASSERT_FALSE(outcome.changed);
    TEST_ASSERT_EQUAL_STRING("{\"cleared\":false,\"reason\":\"no such hold\"}", outcome.body.c_str());

    outcome = Core::applyHold(holds, Core::holdCommandFromMqtt("{\"id\":\"smoke\"}"), 0, EPOCH);
    TEST_ASSERT_EQUAL(400, outcome.status);
    TEST_ASSERT_FALSE(outcome.changed);
}

void test_manual_hold_follows_force_unsafe()
{
    SafetyHolds holds;
    TEST_ASSERT_TRUE(Core::syncManualHold(holds, true, 0, EPOCH));
    TEST_ASSERT_EQUAL_STRING("Settings", holds.find(MANUAL_HOLD_ID)->source.c_str());
    TEST_ASSERT_FALSE(Core::syncManualHold(holds, true, 0, EPOCH));
    TEST_ASSERT_TRUE(Core::syncManualHold(holds, false, 0, EPOCH));
    TEST_ASSERT_NULL(holds.find(MANUAL_HOLD_ID));

    // Set or cleared over the API: the setting has to follow.
    Core::HoldCommand set = Core::holdCommandFromMqtt("{\"id\":\"manual\",\"reason\":\"x\"}");
    TEST_ASSERT_TRUE(Core::applyHold(holds, set, 0, EPOCH).manualChanged);
    TEST_ASSERT_FALSE(Core::applyHold(holds, set, 0, EPOCH).manualChanged); // already there
    Core::HoldCommand clear;
    Core::holdCommandFromHttp(true, "/api/safety/hold/manual", "", "", clear);
    TEST_ASSERT_TRUE(Core::applyHold(holds, clear, 0, EPOCH).manualChanged);
}

void test_history_hold_cleared_still_unsafe_for_cloud()
{
    SafetyHolds holds;
    SafetyStatus status;
    status.evaluatedAtMs = 1;
    status.reasonFlags = UnsafeCloudCover | UnsafeExternalHold;
    SafetyHold cleared;
    cleared.id = "smoke";
    cleared.reason = SMOKE;
    SafetyHistory::Entry e{};
    Core::holdClearedEntry(e, cleared, status, holds);
    TEST_ASSERT_EQUAL(SafetyHistory::Kind::HoldCleared, e.kind);
    TEST_ASSERT_EQUAL_UINT32(UnsafeCloudCover, e.flags);
    TEST_ASSERT_EQUAL_STRING("smoke", SafetyHistory::holdId(e).c_str());
    TEST_ASSERT_EQUAL_STRING("Smoke", SafetyHistory::holdLabels(e)[0].c_str());

    // Another hold left: still unsafe for a hold.
    holds.set(request("roof", "Roof not fully open"), 0, EPOCH);
    status.reasonFlags = UnsafeExternalHold;
    Core::holdClearedEntry(e, cleared, status, holds);
    TEST_ASSERT_EQUAL_UINT32(UnsafeExternalHold, e.flags);
}

void test_history_hold_set_only_while_unsafe()
{
    SafetyHolds holds;
    holds.set(request("smoke", SMOKE), 0, EPOCH);
    holds.set(request("roof", "Roof not fully open"), 0, EPOCH);
    SafetyStatus status;
    status.evaluatedAtMs = 1;
    status.isSafe = true;
    SafetyHistory::Entry e{};
    TEST_ASSERT_FALSE(Core::holdSetEntry(e, "roof", status, holds));
    status.isSafe = false;
    status.reasonFlags = UnsafeExternalHold;
    TEST_ASSERT_TRUE(Core::holdSetEntry(e, "roof", status, holds));
    TEST_ASSERT_EQUAL_UINT32(0, e.flags);
    const std::vector<std::string> labels = SafetyHistory::holdLabels(e);
    TEST_ASSERT_EQUAL(2, labels.size());
    TEST_ASSERT_EQUAL_STRING("Smoke", labels[0].c_str());
    TEST_ASSERT_EQUAL_STRING("Roof not fully open", labels[1].c_str());
}

void test_history_labels_keep_whole_labels_that_fit()
{
    SafetyHistory::Entry e{};
    SafetyHistory::setHoldLabels(e, {"Smoke", std::string(36, 'x'), "Roof"});
    const std::vector<std::string> labels = SafetyHistory::holdLabels(e);
    TEST_ASSERT_EQUAL(2, labels.size());
    TEST_ASSERT_EQUAL_STRING("Smoke", labels[0].c_str());
    TEST_ASSERT_EQUAL_STRING("Roof", labels[1].c_str());
    SafetyHistory::setHoldId(e, std::string(24, 'a'));
    TEST_ASSERT_EQUAL(24, SafetyHistory::holdId(e).size());
}

void test_history_entry_json()
{
    SafetyHistory::Entry e{};
    e.kind = SafetyHistory::Kind::HoldCleared;
    e.flags = UnsafeCloudCover;
    SafetyHistory::setHoldId(e, "smoke");
    SafetyHistory::setHoldLabels(e, {"Smoke"});
    DynamicJsonDocument doc(512);
    SafetyHistory::writeEntry(doc.to<JsonObject>(), e);
    TEST_ASSERT_EQUAL_STRING("holdCleared", doc["kind"]);
    TEST_ASSERT_EQUAL_STRING("smoke", doc["holdId"]);
    TEST_ASSERT_EQUAL_STRING("Smoke", doc["holds"][0]);
    TEST_ASSERT_EQUAL_UINT32(UnsafeCloudCover, doc["reasonFlags"].as<uint32_t>());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_hold_id_rules);
    RUN_TEST(test_bad_requests_are_refused_and_change_nothing);
    RUN_TEST(test_reason_limit_counts_characters_not_bytes);
    RUN_TEST(test_at_most_eight_holds);
    RUN_TEST(test_setting_again_updates_reason_and_keeps_since);
    RUN_TEST(test_clearing_one_hold_keeps_the_others);
    RUN_TEST(test_clearing_an_unknown_hold_changes_nothing);
    RUN_TEST(test_holds_are_unsafe_and_immediate);
    RUN_TEST(test_clearing_a_hold_never_forces_safe);
    RUN_TEST(test_cleared_hold_waits_out_the_safe_delay);
    RUN_TEST(test_manual_hold_keeps_its_reason_and_bit);
    RUN_TEST(test_expiry_with_a_valid_clock);
    RUN_TEST(test_expiry_without_a_clock_counts_device_time);
    RUN_TEST(test_holds_survive_a_restart);
    RUN_TEST(test_restored_expiry_lifts_at_its_clock_time);
    RUN_TEST(test_restored_expiry_without_a_clock_never_lifts_early);
    RUN_TEST(test_restore_drops_bad_entries);
    RUN_TEST(test_since_is_dated_once_the_clock_is_set);
    RUN_TEST(test_hold_json_shapes);
    RUN_TEST(test_hold_label_is_the_reason_up_to_its_colon);
    RUN_TEST(test_parse_request_defaults_and_clear);
    RUN_TEST(test_http_routes);
    RUN_TEST(test_apply_answers);
    RUN_TEST(test_manual_hold_follows_force_unsafe);
    RUN_TEST(test_history_hold_cleared_still_unsafe_for_cloud);
    RUN_TEST(test_history_hold_set_only_while_unsafe);
    RUN_TEST(test_history_labels_keep_whole_labels_that_fit);
    RUN_TEST(test_history_entry_json);
    return UNITY_END();
}
