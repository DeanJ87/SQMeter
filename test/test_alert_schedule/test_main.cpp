#include <unity.h>

#include "AlertSchedule.h"
#include "Config.h"
#include "DeviceCore.h"

#include <ArduinoJson.h>

using namespace SQM::Alerts;

// When alerts are sent (specs/021): the send mode, pause/resume and why.

void setUp(void) {}
void tearDown(void) {}

namespace
{
    AlertSchedule started(SendMode mode, bool connected, bool sending = true)
    {
        AlertSchedule schedule;
        schedule.restore(sending, true, 0, 0);
        schedule.update(mode, connected, 1000, 0);
        return schedule;
    }
}

void test_any_mode_pause_and_resume(void)
{
    AlertSchedule schedule = started(SendMode::Any, false);
    TEST_ASSERT_TRUE(schedule.state().sending);
    TEST_ASSERT_TRUE(schedule.command(false, ScheduleReason::UserUi, 2000, 1791500000));
    TEST_ASSERT_FALSE(schedule.state().sending);
    TEST_ASSERT_EQUAL(ScheduleReason::UserUi, schedule.state().reason);
    TEST_ASSERT_TRUE(schedule.state().sinceKnown);
    TEST_ASSERT_EQUAL_UINT32(2000, schedule.state().sinceMs);
    TEST_ASSERT_EQUAL_INT64(1791500000, schedule.state().sinceEpoch);
    TEST_ASSERT_FALSE(schedule.command(false, ScheduleReason::UserUi, 3000, 0)); // no change
    // Connections don't matter in "any time".
    TEST_ASSERT_FALSE(schedule.update(SendMode::Any, true, 4000, 0));
    TEST_ASSERT_FALSE(schedule.update(SendMode::Any, false, 5000, 0));
    TEST_ASSERT_FALSE(schedule.state().sending);
    TEST_ASSERT_TRUE(schedule.command(true, ScheduleReason::UserMqtt, 6000, 0));
    TEST_ASSERT_TRUE(schedule.state().sending);
    TEST_ASSERT_EQUAL(ScheduleReason::UserMqtt, schedule.state().reason);
}

void test_while_connected_follows_connects_and_disconnects(void)
{
    AlertSchedule schedule = started(SendMode::WhileConnected, false, false);
    TEST_ASSERT_TRUE(schedule.update(SendMode::WhileConnected, true, 2000, 0));
    TEST_ASSERT_TRUE(schedule.state().sending);
    TEST_ASSERT_EQUAL(ScheduleReason::ClientConnected, schedule.state().reason);
    TEST_ASSERT_TRUE(schedule.update(SendMode::WhileConnected, false, 3000, 0));
    TEST_ASSERT_FALSE(schedule.state().sending);
    TEST_ASSERT_EQUAL(ScheduleReason::ClientDisconnected, schedule.state().reason);
    TEST_ASSERT_EQUAL_UINT32(3000, schedule.state().sinceMs);
}

void test_while_connected_pause_lasts_until_next_connect_or_disconnect(void)
{
    AlertSchedule schedule = started(SendMode::WhileConnected, true);
    schedule.command(false, ScheduleReason::UserUi, 2000, 0);
    TEST_ASSERT_FALSE(schedule.update(SendMode::WhileConnected, true, 3000, 0)); // still connected: pause stands
    TEST_ASSERT_FALSE(schedule.state().sending);
    schedule.update(SendMode::WhileConnected, false, 4000, 0);
    TEST_ASSERT_EQUAL(ScheduleReason::ClientDisconnected, schedule.state().reason);
    schedule.command(true, ScheduleReason::UserRest, 5000, 0); // resume by hand while disconnected
    TEST_ASSERT_TRUE(schedule.state().sending);
    schedule.update(SendMode::WhileConnected, true, 6000, 0);
    schedule.update(SendMode::WhileConnected, false, 7000, 0);
    TEST_ASSERT_FALSE(schedule.state().sending);
}

void test_silence_never_pauses(void)
{
    // A silent client is still "connected" to the Router; only the flag
    // matters, so nothing changes however long it goes quiet.
    AlertSchedule schedule = started(SendMode::WhileConnected, true);
    for (uint32_t t = 2000; t < 3600000; t += 60000)
        TEST_ASSERT_FALSE(schedule.update(SendMode::WhileConnected, true, t, 0));
    TEST_ASSERT_TRUE(schedule.state().sending);
}

void test_mode_changes(void)
{
    AlertSchedule schedule = started(SendMode::Any, false);
    TEST_ASSERT_TRUE(schedule.update(SendMode::WhileConnected, false, 2000, 0));
    TEST_ASSERT_FALSE(schedule.state().sending);
    TEST_ASSERT_EQUAL(ScheduleReason::WaitingForClient, schedule.state().reason);
    // Back to "any time": the mode's pause ends.
    TEST_ASSERT_TRUE(schedule.update(SendMode::Any, false, 3000, 0));
    TEST_ASSERT_TRUE(schedule.state().sending);
    // A pause by the user survives a mode change.
    schedule.command(false, ScheduleReason::UserUi, 4000, 0);
    schedule.update(SendMode::WhileConnected, false, 5000, 0);
    schedule.update(SendMode::Any, false, 6000, 0);
    TEST_ASSERT_FALSE(schedule.state().sending);
    TEST_ASSERT_EQUAL(ScheduleReason::UserUi, schedule.state().reason);
    // Switching to while-connected with a client already connected sends.
    AlertSchedule connected = started(SendMode::Any, true, false);
    connected.update(SendMode::WhileConnected, true, 2000, 0);
    TEST_ASSERT_TRUE(connected.state().sending);
    TEST_ASSERT_EQUAL(ScheduleReason::ClientConnected, connected.state().reason);
}

void test_first_update_keeps_saved_state(void)
{
    // After a restart the client may still be polling but not "connected":
    // what was saved stands.
    AlertSchedule schedule;
    schedule.restore(true, true, static_cast<uint8_t>(ScheduleReason::ClientConnected), 1791500000);
    TEST_ASSERT_FALSE(schedule.update(SendMode::WhileConnected, false, 100, 0));
    TEST_ASSERT_TRUE(schedule.state().sending);
    TEST_ASSERT_EQUAL(ScheduleReason::ClientConnected, schedule.state().reason);
    TEST_ASSERT_FALSE(schedule.state().sinceKnown);
    TEST_ASSERT_EQUAL_INT64(1791500000, schedule.state().sinceEpoch);
}

void test_restore_migrated_and_bad_reason(void)
{
    AlertSchedule schedule;
    schedule.restore(false, false, 0, 0);
    TEST_ASSERT_EQUAL(ScheduleReason::Migrated, schedule.state().reason);
    schedule.restore(true, false, 0, 0);
    TEST_ASSERT_EQUAL(ScheduleReason::None, schedule.state().reason);
    schedule.restore(false, true, 99, 0);
    TEST_ASSERT_EQUAL(ScheduleReason::None, schedule.state().reason);
    TEST_ASSERT_EQUAL_STRING("migrated", scheduleReasonName(ScheduleReason::Migrated));
    TEST_ASSERT_EQUAL_STRING("waiting-for-client", scheduleReasonName(ScheduleReason::WaitingForClient));
    TEST_ASSERT_EQUAL_STRING("whileConnected", sendModeName(SendMode::WhileConnected));
}

// SC-005: an update from v0.2.0-beta.3 keeps behaviour for every combination
// of the old "armWithAlpaca" setting and the saved armed flag.
void test_upgrade_combinations(void)
{
    for (int arm = 0; arm < 2; ++arm)
    {
        for (int armed = 0; armed < 2; ++armed)
        {
            const std::string old = std::string("{\"alerts\":{\"enabled\":true,\"armWithAlpaca\":") + (arm ? "true" : "false") + "}}";
            auto cfg = SQM::Config::fromJson(old);
            TEST_ASSERT_TRUE(cfg.has_value());
            const SendMode mode = SQM::Core::sendMode(*cfg);
            TEST_ASSERT_EQUAL(arm ? SendMode::WhileConnected : SendMode::Any, mode);

            AlertSchedule schedule;
            schedule.restore(armed != 0, false, 0, 0); // old firmware saved no reason
            schedule.update(mode, false, 1000, 0);
            TEST_ASSERT_EQUAL(armed != 0, schedule.state().sending);
            TEST_ASSERT_EQUAL(armed ? ScheduleReason::None : ScheduleReason::Migrated, schedule.state().reason);
            // The HA/MQTT switch still works.
            schedule.command(armed == 0, ScheduleReason::UserMqtt, 2000, 0);
            TEST_ASSERT_EQUAL(armed == 0, schedule.state().sending);
            // In while-connected mode a connect still resumes.
            if (arm)
            {
                schedule.command(false, ScheduleReason::UserMqtt, 3000, 0);
                schedule.update(mode, true, 4000, 0);
                TEST_ASSERT_TRUE(schedule.state().sending);
            }
        }
    }
}

void test_armed_document(void)
{
    SQM::Config cfg = SQM::Config::createDefault();
    cfg.alerts.sendMode = SQM::AlertsConfig::SendMode::WhileConnected;
    AlertSchedule schedule = started(SendMode::WhileConnected, true);
    schedule.update(SendMode::WhileConnected, false, 5000, 1791500000);
    DynamicJsonDocument doc(512);
    SQM::Core::writeAlertSchedule(doc.to<JsonObject>(), schedule.state(), cfg, 65000);
    TEST_ASSERT_FALSE(doc["armed"].as<bool>());
    TEST_ASSERT_TRUE(doc["armWithAlpaca"].as<bool>());
    TEST_ASSERT_EQUAL_STRING("whileConnected", doc["mode"]);
    TEST_ASSERT_EQUAL_STRING("client-disconnected", doc["reason"]);
    TEST_ASSERT_EQUAL_STRING("2026-10-08T22:53:20Z", doc["since"]);
    TEST_ASSERT_EQUAL_UINT32(60000, doc["sinceAgeMs"].as<uint32_t>());

    AlertSchedule fresh;
    fresh.restore(true, true, 0, 0);
    DynamicJsonDocument doc2(512);
    SQM::Core::writeAlertSchedule(doc2.to<JsonObject>(), fresh.state(), cfg, 65000);
    TEST_ASSERT_TRUE(doc2["since"].isNull());
    TEST_ASSERT_TRUE(doc2["sinceAgeMs"].isNull());
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_any_mode_pause_and_resume);
    RUN_TEST(test_while_connected_follows_connects_and_disconnects);
    RUN_TEST(test_while_connected_pause_lasts_until_next_connect_or_disconnect);
    RUN_TEST(test_silence_never_pauses);
    RUN_TEST(test_mode_changes);
    RUN_TEST(test_first_update_keeps_saved_state);
    RUN_TEST(test_restore_migrated_and_bad_reason);
    RUN_TEST(test_upgrade_combinations);
    RUN_TEST(test_armed_document);
    return UNITY_END();
}
