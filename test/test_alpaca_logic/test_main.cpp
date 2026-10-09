#include "AlpacaRouter.h"
#include <cstring>
#include <unity.h>
#include <cstring>
#include "SafetyEvaluator.h"
#include "ObservingConditionsMapper.h"
#include "AlpacaDiscovery.h"
#include "AlpacaProtocol.h"
#include "ClientWatch.h"

using namespace SQM::Alpaca;

void setUp(void) {}
void tearDown(void) {}

namespace
{
    ObservingConditionsSnapshot allValidSnapshot()
    {
        ObservingConditionsSnapshot snap;
        for (SourceState *source : {&snap.skyLight, &snap.irSky, &snap.environment})
        {
            source->present = true;
            source->valid = true;
        }
        return snap;
    }
} // namespace

void test_observing_conditions_maps_known_property(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.skyQualityMagArcsec2 = 21.3f;

    PropertyResult r = getObservingConditionsProperty("SkyQuality", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(21.3f, r.value);
}

void test_observing_conditions_case_insensitive(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.temperatureC = 12.5f;

    PropertyResult r = getObservingConditionsProperty("TEMPERATURE", snap);
    TEST_ASSERT_TRUE(r.ok);
}

// D-22 (specs/020-settings-dependencies): wind properties need the anemometer.
void test_observing_conditions_not_implemented_property(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot(); // wind not present

    PropertyResult r = getObservingConditionsProperty("windspeed", snap);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, r.errorNumber);

    r = getObservingConditionsProperty("starfwhm", snap);
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, r.errorNumber);
}

void test_observing_conditions_average_period_always_zero(void)
{
    ObservingConditionsSnapshot snap; // nothing valid - shouldn't matter
    PropertyResult r = getObservingConditionsProperty("averageperiod", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(0.0, r.value);
}

void test_observing_conditions_no_data_error(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.environment.valid = false;

    PropertyResult r = getObservingConditionsProperty("humidity", snap);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(ALPACA_ERR_DRIVER_BASE, r.errorNumber);
}

void test_observing_conditions_one_sensor_down_others_ok(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.environment.valid = false;
    snap.skyTemperatureC = -20.0f;

    TEST_ASSERT_FALSE(getObservingConditionsProperty("pressure", snap).ok);
    PropertyResult r = getObservingConditionsProperty("skytemperature", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(-20.0f, r.value);
}

void test_observing_conditions_unknown_property(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();

    PropertyResult r = getObservingConditionsProperty("bogus", snap);
    TEST_ASSERT_FALSE(r.ok);
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, r.errorNumber);
}

void test_observing_conditions_pressure(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.pressureHPa = 1013.2f;

    PropertyResult r = getObservingConditionsProperty("pressure", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(1013.2f, r.value);
}

// D-21 (specs/020-settings-dependencies): RainRate needs the rain sensor.
void test_observing_conditions_rainrate_depends_on_rain_sensor(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.rainRateMmPerHour = 2.5f;

    // Rain sensor disabled -> NotImplemented
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, getObservingConditionsProperty("rainrate", snap).errorNumber);

    // Enabled but offline -> driver error
    snap.rain.present = true;
    TEST_ASSERT_EQUAL(ALPACA_ERR_DRIVER_BASE, getObservingConditionsProperty("rainrate", snap).errorNumber);

    snap.rain.valid = true;
    PropertyResult r = getObservingConditionsProperty("rainrate", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(2.5f, r.value);
}

void test_rain_rate_imperial_conversion(void)
{
    TEST_ASSERT_EQUAL_FLOAT(25.4f, rainRateToMmPerHour(1.0f, true));
    TEST_ASSERT_EQUAL_FLOAT(3.0f, rainRateToMmPerHour(3.0f, false));
}

void test_wind_direction_zero_when_calm(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.wind.present = true;
    snap.wind.valid = true;
    snap.windVane.present = true;
    snap.windVane.valid = true;
    snap.windDirectionDeg = 270.0f;
    snap.windSpeedMs = 0.0f;
    TEST_ASSERT_EQUAL_FLOAT(0.0f, getObservingConditionsProperty("winddirection", snap).value);
    snap.windSpeedMs = 3.0f;
    TEST_ASSERT_EQUAL_FLOAT(270.0f, getObservingConditionsProperty("winddirection", snap).value);
}

void test_sensor_description(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();

    StringResult r = getSensorDescription("Pressure", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_TRUE(r.value.find("BME280") != std::string::npos);

    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, getSensorDescription("StarFWHM", snap).errorNumber);
    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, getSensorDescription("RainRate", snap).errorNumber);
    TEST_ASSERT_EQUAL(ALPACA_ERR_INVALID_VALUE, getSensorDescription("bogus", snap).errorNumber);
}

void test_time_since_last_update(void)
{
    ObservingConditionsSnapshot snap = allValidSnapshot();
    snap.skyLight.ageSeconds = 0.4;
    snap.irSky.ageSeconds = 3.0;
    snap.environment.ageSeconds = 2.0;

    PropertyResult r = getTimeSinceLastUpdate("", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(0.4, r.value);

    r = getTimeSinceLastUpdate("humidity", snap);
    TEST_ASSERT_TRUE(r.ok);
    TEST_ASSERT_EQUAL_FLOAT(2.0, r.value);

    TEST_ASSERT_EQUAL(ALPACA_ERR_NOT_IMPLEMENTED, getTimeSinceLastUpdate("windspeed", snap).errorNumber);
    TEST_ASSERT_EQUAL(ALPACA_ERR_INVALID_VALUE, getTimeSinceLastUpdate("bogus", snap).errorNumber);

    ObservingConditionsSnapshot empty;
    TEST_ASSERT_EQUAL(ALPACA_ERR_DRIVER_BASE, getTimeSinceLastUpdate("", empty).errorNumber);
}

void test_average_period_validation(void)
{
    TEST_ASSERT_TRUE(validateAveragePeriod(0.0).ok);
    TEST_ASSERT_EQUAL(ALPACA_ERR_INVALID_VALUE, validateAveragePeriod(1.0).errorNumber);
    TEST_ASSERT_EQUAL(ALPACA_ERR_INVALID_VALUE, validateAveragePeriod(-1.0).errorNumber);
}

void test_alpaca_double_parsing(void)
{
    double v = -1.0;
    TEST_ASSERT_TRUE(parseAlpacaDouble("0", v));
    TEST_ASSERT_EQUAL_FLOAT(0.0, v);
    TEST_ASSERT_TRUE(parseAlpacaDouble("1.5", v));
    TEST_ASSERT_EQUAL_FLOAT(1.5, v);
    TEST_ASSERT_FALSE(parseAlpacaDouble("", v));
    TEST_ASSERT_FALSE(parseAlpacaDouble("1.5x", v));
    TEST_ASSERT_FALSE(parseAlpacaDouble("nan", v));
}

// --- AlpacaDiscovery ---

void test_discovery_valid_packet(void)
{
    const char *payload = "alpacadiscovery1";
    TEST_ASSERT_TRUE(isValidDiscoveryRequest(reinterpret_cast<const uint8_t *>(payload), strlen(payload)));
}

void test_discovery_rejects_wrong_payload(void)
{
    const char *payload = "not-alpaca-at-all";
    TEST_ASSERT_FALSE(isValidDiscoveryRequest(reinterpret_cast<const uint8_t *>(payload), strlen(payload)));
}

void test_discovery_rejects_short_payload(void)
{
    const char *payload = "alpaca";
    TEST_ASSERT_FALSE(isValidDiscoveryRequest(reinterpret_cast<const uint8_t *>(payload), strlen(payload)));
}

void test_discovery_rejects_null(void)
{
    TEST_ASSERT_FALSE(isValidDiscoveryRequest(nullptr, 0));
}

void test_discovery_response_body(void)
{
    std::string body = buildDiscoveryResponse(80);
    TEST_ASSERT_EQUAL_STRING("{\"AlpacaPort\":80}", body.c_str());
}

// --- AlpacaProtocol ---

void test_param_name_case_insensitive(void)
{
    TEST_ASSERT_TRUE(paramNameEquals("ClientTransactionID", "clienttransactionid"));
    TEST_ASSERT_TRUE(paramNameEquals("CONNECTED", "Connected"));
    TEST_ASSERT_FALSE(paramNameEquals("ClientID", "ClientTransactionID"));
    TEST_ASSERT_FALSE(paramNameEquals("Connected", "Connecte"));
}

void test_put_param_names_are_case_sensitive(void)
{
    // GET query strings: any casing.
    TEST_ASSERT_TRUE(paramNameMatches("averageperiod", "AveragePeriod", false));
    TEST_ASSERT_TRUE(paramNameMatches("ClientTransactionID", "ClientTransactionID", false));
    // PUT form bodies: exact casing only (ConformU "Bad casing" checks).
    TEST_ASSERT_TRUE(paramNameMatches("AveragePeriod", "AveragePeriod", true));
    TEST_ASSERT_FALSE(paramNameMatches("averageperiod", "AveragePeriod", true));
    TEST_ASSERT_FALSE(paramNameMatches("clienttransactionid", "ClientTransactionID", true));
}

void test_client_transaction_id_parsing(void)
{
    TEST_ASSERT_EQUAL_UINT32(42, parseClientTransactionId("42"));
    TEST_ASSERT_EQUAL_UINT32(4294967295u, parseClientTransactionId("4294967295"));
    TEST_ASSERT_EQUAL_UINT32(0, parseClientTransactionId("4294967296"));
    TEST_ASSERT_EQUAL_UINT32(0, parseClientTransactionId("-1"));
    TEST_ASSERT_EQUAL_UINT32(0, parseClientTransactionId("abc"));
    TEST_ASSERT_EQUAL_UINT32(0, parseClientTransactionId(""));
}

void test_alpaca_bool_parsing(void)
{
    bool value = false;
    TEST_ASSERT_TRUE(parseAlpacaBool("True", value));
    TEST_ASSERT_TRUE(value);
    TEST_ASSERT_TRUE(parseAlpacaBool("false", value));
    TEST_ASSERT_FALSE(value);
    TEST_ASSERT_FALSE(parseAlpacaBool("1", value));
    TEST_ASSERT_FALSE(parseAlpacaBool("", value));
}

void test_unique_id_includes_mac(void)
{
    TEST_ASSERT_EQUAL_STRING(
        "sqmeter-a1b2c3d4e5f6-observingconditions-0", buildUniqueId(0xA1B2C3D4E5F6ULL, "observingconditions", 0).c_str());
    // Upper 16 bits of the efuse value are ignored; short MACs are zero-padded.
    TEST_ASSERT_EQUAL_STRING("sqmeter-000000000001-safetymonitor-0", buildUniqueId(0xFFFF000000000001ULL, "safetymonitor", 0).c_str());
}

// --- Rain safety ---

namespace
{
    class FakeBackend : public SQM::Alpaca::Backend
    {
    public:
        bool enabled = true;
        bool safe = true;
        ObservingConditionsSnapshot snapshot;
        bool alpacaEnabled() const override { return enabled; }
        bool isSafe() const override { return safe; }
        ObservingConditionsSnapshot observingConditions() const override { return snapshot; }
        std::string location() const override { return "Roof"; }
        std::string timestampUtc() const override { return ""; }
    };

    SQM::Alpaca::Response route(
        SQM::Alpaca::Router &router, bool put, const std::string &path, std::vector<std::pair<std::string, std::string>> params = {})
    {
        SQM::Alpaca::Request request;
        request.get = !put;
        request.put = put;
        request.path = path;
        request.params = std::move(params);
        SQM::Alpaca::Response response;
        TEST_ASSERT_TRUE(router.handle(request, response));
        return response;
    }
} // namespace

void test_router_issafe_and_transaction_ids(void)
{
    FakeBackend backend;
    SQM::Alpaca::Router router(backend, {"SQMeter", "SQMeter", "1.2.3", 0x1234});
    SQM::Alpaca::Response r = route(router, false, "/api/v1/safetymonitor/0/issafe", {{"clienttransactionid", "7"}});
    TEST_ASSERT_EQUAL(200, r.status);
    TEST_ASSERT_EQUAL_STRING(
        "{\"Value\":true,\"ClientTransactionID\":7,\"ServerTransactionID\":1,\"ErrorNumber\":0,\"ErrorMessage\":\"\"}", r.body.c_str());
    backend.safe = false;
    r = route(router, false, "/api/v1/safetymonitor/0/issafe");
    TEST_ASSERT_NOT_NULL(strstr(r.body.c_str(), "\"Value\":false"));
    TEST_ASSERT_NOT_NULL(strstr(r.body.c_str(), "\"ServerTransactionID\":2"));
}

void test_router_bad_requests_are_plain_400(void)
{
    FakeBackend backend;
    SQM::Alpaca::Router router(backend, {"SQMeter", "SQMeter", "1.2.3", 0x1234});
    TEST_ASSERT_EQUAL(400, route(router, false, "/api/v1/telescope/0/name").status);
    TEST_ASSERT_EQUAL(400, route(router, false, "/api/v1/safetymonitor/1/name").status);
    TEST_ASSERT_EQUAL(400, route(router, true, "/api/v1/safetymonitor/0/issafe").status);
    const SQM::Alpaca::Response r = route(router, false, "/api/v1/safetymonitor/0/bogus");
    TEST_ASSERT_EQUAL_STRING("text/plain", r.contentType);
    SQM::Alpaca::Request other;
    other.get = true;
    other.path = "/api/sensors";
    SQM::Alpaca::Response unused;
    TEST_ASSERT_FALSE(router.handle(other, unused));
}

void test_router_put_parameter_casing(void)
{
    FakeBackend backend;
    SQM::Alpaca::Router router(backend, {"SQMeter", "SQMeter", "1.2.3", 0x1234});
    TEST_ASSERT_EQUAL(200, route(router, true, "/api/v1/observingconditions/0/averageperiod", {{"AveragePeriod", "0"}}).status);
    TEST_ASSERT_EQUAL(400, route(router, true, "/api/v1/observingconditions/0/averageperiod", {{"averageperiod", "0"}}).status);
    const SQM::Alpaca::Response r = route(router, true, "/api/v1/observingconditions/0/refresh", {{"clienttransactionid", "9"}});
    TEST_ASSERT_NOT_NULL(strstr(r.body.c_str(), "\"ClientTransactionID\":0"));
}

// D-20: with Alpaca off, nothing is served.
void test_router_connected_and_disabled(void)
{
    FakeBackend backend;
    SQM::Alpaca::Router router(backend, {"SQMeter", "SQMeter", "1.2.3", 0x1234});
    route(router, true, "/api/v1/observingconditions/0/connected", {{"Connected", "True"}});
    TEST_ASSERT_NOT_NULL(strstr(route(router, false, "/api/v1/observingconditions/0/connected").body.c_str(), "\"Value\":true"));
    TEST_ASSERT_NOT_NULL(strstr(route(router, false, "/api/v1/safetymonitor/0/connected").body.c_str(), "\"Value\":false"));
    backend.enabled = false;
    TEST_ASSERT_NOT_NULL(strstr(route(router, false, "/api/v1/safetymonitor/0/issafe").body.c_str(), "\"ErrorNumber\":1031"));
    TEST_ASSERT_NOT_NULL(strstr(route(router, false, "/management/v1/configureddevices").body.c_str(), "\"Value\":[]"));
}

// --- Client activity and ClientWatch (specs/021) ---

void test_router_counts_requests_disconnects_and_client_id(void)
{
    FakeBackend backend;
    SQM::Alpaca::Router router(backend, {"SQMeter", "SQMeter", "1.2.3", 0x1234});
    route(router, true, "/api/v1/safetymonitor/0/connected", {{"Connected", "True"}, {"ClientID", "42"}});
    route(router, false, "/api/v1/safetymonitor/0/issafe", {{"ClientID", "42"}});
    route(router, false, "/management/v1/description");                              // management requests don't count
    route(router, false, "/api/v1/safetymonitor/0/devicestate", {{"source", "ui"}}); // nor the web UI's own
    DeviceActivity sm = router.activity(Device::SafetyMonitor);
    TEST_ASSERT_TRUE(sm.connected);
    TEST_ASSERT_EQUAL_UINT32(2, sm.requests);
    TEST_ASSERT_EQUAL_UINT32(0, sm.disconnects);
    TEST_ASSERT_TRUE(sm.hasClientId);
    TEST_ASSERT_EQUAL_UINT32(42, sm.clientId);
    TEST_ASSERT_EQUAL_UINT32(0, router.activity(Device::ObservingConditions).requests);

    route(router, true, "/api/v1/safetymonitor/0/disconnect");
    route(router, true, "/api/v1/safetymonitor/0/connected", {{"Connected", "False"}}); // already disconnected
    sm = router.activity(Device::SafetyMonitor);
    TEST_ASSERT_FALSE(sm.connected);
    TEST_ASSERT_EQUAL_UINT32(1, sm.disconnects);

    route(router, true, "/api/v1/observingconditions/0/connect");
    router.resetConnections();
    TEST_ASSERT_FALSE(router.anyConnected());
    TEST_ASSERT_EQUAL_UINT32(0, router.activity(Device::ObservingConditions).disconnects);
}

namespace
{
    struct WatchRig
    {
        FakeBackend backend;
        SQM::Alpaca::Router router{backend, {"SQMeter", "SQMeter", "1.2.3", 0x1234}};
        ClientWatch watch;
        uint32_t silence[DEVICE_COUNT] = {120000, 600000};
        bool enabled = true;

        void tick(uint32_t nowMs)
        {
            const DeviceActivity activity[DEVICE_COUNT] = {
                router.activity(Device::SafetyMonitor), router.activity(Device::ObservingConditions)};
            watch.update(activity, silence, enabled, nowMs);
        }
        const ClientState &sm() const { return watch.state(Device::SafetyMonitor); }
        const ClientState &oc() const { return watch.state(Device::ObservingConditions); }
        void poll(const char *device = "safetymonitor") { route(router, false, std::string("/api/v1/") + device + "/0/issafe"); }
        void connect(const char *device = "safetymonitor")
        {
            route(router, true, std::string("/api/v1/") + device + "/0/connected", {{"Connected", "True"}});
        }
        void disconnect(const char *device = "safetymonitor")
        {
            route(router, true, std::string("/api/v1/") + device + "/0/connected", {{"Connected", "False"}});
        }
    };
} // namespace

void test_watch_silence_and_recovery(void)
{
    WatchRig rig;
    rig.tick(1000);
    TEST_ASSERT_FALSE(rig.sm().watching); // nothing since the restart
    rig.connect();
    rig.tick(2000);
    TEST_ASSERT_TRUE(rig.sm().watching);
    rig.poll();
    rig.tick(5000);
    rig.tick(5000 + 120000);
    TEST_ASSERT_FALSE(rig.sm().silent); // exactly the silence time: not yet
    rig.tick(5000 + 120001);
    TEST_ASSERT_TRUE(rig.sm().silent);
    TEST_ASSERT_TRUE(rig.sm().connected); // silence doesn't disconnect
    rig.poll();
    rig.tick(200000);
    TEST_ASSERT_FALSE(rig.sm().silent);
    TEST_ASSERT_EQUAL_UINT32(200000, rig.sm().lastRequestMs);
}

void test_watch_clean_disconnect_is_not_silence(void)
{
    WatchRig rig;
    rig.tick(0);
    rig.connect();
    rig.tick(1000);
    rig.disconnect();
    rig.tick(2000);
    TEST_ASSERT_TRUE(rig.sm().disconnectedNow);
    TEST_ASSERT_FALSE(rig.sm().watching);
    rig.tick(3000);
    TEST_ASSERT_FALSE(rig.sm().disconnectedNow); // an edge, once
    // A tool polling after the session ended doesn't restart watching...
    rig.poll();
    rig.tick(4000);
    rig.tick(4000 + 600000);
    TEST_ASSERT_FALSE(rig.sm().watching);
    TEST_ASSERT_FALSE(rig.sm().silent);
    // ...a new connect does.
    rig.connect();
    rig.tick(700000);
    TEST_ASSERT_TRUE(rig.sm().watching);
}

void test_watch_after_restart_polling_counts(void)
{
    // The device restarted; the client keeps polling without connecting.
    WatchRig rig;
    rig.tick(500);
    rig.poll();
    rig.tick(1000);
    TEST_ASSERT_TRUE(rig.sm().watching);
    TEST_ASSERT_FALSE(rig.sm().connected);
    rig.tick(1000 + 120001);
    TEST_ASSERT_TRUE(rig.sm().silent);
}

void test_watch_devices_and_clients_are_separate(void)
{
    WatchRig rig;
    rig.tick(0);
    rig.connect("safetymonitor");
    rig.connect("observingconditions");
    rig.tick(1000);
    // Two clients on the safety monitor: one polling is enough.
    for (uint32_t t = 2000; t < 600000; t += 5000)
    {
        rig.poll("safetymonitor");
        rig.tick(t);
    }
    TEST_ASSERT_FALSE(rig.sm().silent);
    TEST_ASSERT_FALSE(rig.oc().silent); // weather has 10 minutes
    rig.poll("safetymonitor");
    rig.tick(1000 + 600001);
    TEST_ASSERT_TRUE(rig.oc().silent);
    TEST_ASSERT_FALSE(rig.sm().silent);
}

void test_watch_off_when_alpaca_disabled(void)
{
    WatchRig rig;
    rig.tick(0);
    rig.connect();
    rig.tick(1000);
    rig.enabled = false;
    rig.tick(500000);
    TEST_ASSERT_FALSE(rig.sm().watching);
    TEST_ASSERT_FALSE(rig.sm().silent);
}

void test_watch_reset_forgets_everything(void)
{
    WatchRig rig;
    rig.tick(0);
    rig.connect();
    rig.tick(1000);
    rig.router.resetConnections();
    rig.watch.reset();
    rig.tick(2000);
    TEST_ASSERT_FALSE(rig.sm().watching);
    rig.tick(500000);
    TEST_ASSERT_FALSE(rig.sm().silent);
}

int main(int argc, char **argv)
{
    UNITY_BEGIN();

    RUN_TEST(test_observing_conditions_maps_known_property);
    RUN_TEST(test_observing_conditions_case_insensitive);
    RUN_TEST(test_observing_conditions_not_implemented_property);
    RUN_TEST(test_observing_conditions_average_period_always_zero);
    RUN_TEST(test_observing_conditions_no_data_error);
    RUN_TEST(test_observing_conditions_unknown_property);
    RUN_TEST(test_observing_conditions_one_sensor_down_others_ok);
    RUN_TEST(test_observing_conditions_pressure);
    RUN_TEST(test_observing_conditions_rainrate_depends_on_rain_sensor);
    RUN_TEST(test_rain_rate_imperial_conversion);
    RUN_TEST(test_wind_direction_zero_when_calm);
    RUN_TEST(test_sensor_description);
    RUN_TEST(test_time_since_last_update);
    RUN_TEST(test_average_period_validation);
    RUN_TEST(test_alpaca_double_parsing);

    RUN_TEST(test_discovery_valid_packet);
    RUN_TEST(test_discovery_rejects_wrong_payload);
    RUN_TEST(test_discovery_rejects_short_payload);
    RUN_TEST(test_discovery_rejects_null);
    RUN_TEST(test_discovery_response_body);

    RUN_TEST(test_param_name_case_insensitive);
    RUN_TEST(test_put_param_names_are_case_sensitive);
    RUN_TEST(test_router_issafe_and_transaction_ids);
    RUN_TEST(test_router_bad_requests_are_plain_400);
    RUN_TEST(test_router_put_parameter_casing);
    RUN_TEST(test_router_connected_and_disabled);
    RUN_TEST(test_client_transaction_id_parsing);
    RUN_TEST(test_alpaca_bool_parsing);
    RUN_TEST(test_unique_id_includes_mac);

    RUN_TEST(test_router_counts_requests_disconnects_and_client_id);
    RUN_TEST(test_watch_silence_and_recovery);
    RUN_TEST(test_watch_clean_disconnect_is_not_silence);
    RUN_TEST(test_watch_after_restart_polling_counts);
    RUN_TEST(test_watch_devices_and_clients_are_separate);
    RUN_TEST(test_watch_off_when_alpaca_disabled);
    RUN_TEST(test_watch_reset_forgets_everything);

    return UNITY_END();
}
