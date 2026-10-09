#include <unity.h>

#include <ArduinoJson.h>

#include <fstream>
#include <sstream>
#include <string>

#include "Config.h"

using namespace SQM;

// Setting ranges shared with the web UI (test/fixtures/config-ranges.json,
// spec 011 FR-004): the device accepts each bound and rejects just outside it.

namespace
{
    std::string readFile(const char *path)
    {
        std::ifstream in(path);
        std::stringstream text;
        text << in.rdbuf();
        return text.str();
    }

    // Sets "a.b" in doc to value, creating the nested object.
    void setPath(JsonDocument &doc, const std::string &path, JsonVariantConst value)
    {
        const size_t dot = path.find('.');
        JsonObject section = doc[path.substr(0, dot)].isNull() ? doc.createNestedObject(path.substr(0, dot))
                                                                : doc[path.substr(0, dot)].as<JsonObject>();
        section[path.substr(dot + 1)] = value;
    }

    bool accepts(JsonObjectConst range, double value, std::string *error)
    {
        DynamicJsonDocument patch(1024);
        for (JsonPairConst companion : range["with"].as<JsonObjectConst>())
            setPath(patch, companion.key().c_str(), companion.value());
        StaticJsonDocument<16> number;
        if (range["int"] | false)
            number.set(static_cast<long long>(value));
        else
            number.set(value);
        setPath(patch, range["path"].as<std::string>(), number.as<JsonVariantConst>());
        std::string json;
        serializeJson(patch, json);
        const Config base = Config::createDefault();
        return Config::fromJson(json, &base, error).has_value();
    }

    void expect(JsonObjectConst range, double value, bool accepted)
    {
        std::string error;
        const bool result = accepts(range, value, &error);
        if (result == accepted)
            return;
        char message[200];
        snprintf(
            message,
            sizeof(message),
            "%s = %g should be %s (%s)",
            range["path"].as<const char *>(),
            value,
            accepted ? "accepted" : "rejected",
            error.c_str());
        TEST_FAIL_MESSAGE(message);
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_every_shared_range_matches_the_device()
{
    DynamicJsonDocument fixture(16384);
    TEST_ASSERT_FALSE(deserializeJson(fixture, readFile("test/fixtures/config-ranges.json")));
    JsonArrayConst ranges = fixture["ranges"];
    TEST_ASSERT_TRUE(ranges.size() > 30);
    for (JsonObjectConst range : ranges)
    {
        const double step = (range["int"] | false) ? 1.0 : 0.1;
        if (range.containsKey("gt"))
        {
            expect(range, range["gt"].as<double>(), false);
            expect(range, range["gt"].as<double>() + step, true);
        }
        else
        {
            expect(range, range["min"].as<double>(), true);
            expect(range, range["min"].as<double>() - step, false);
        }
        if (range.containsKey("lt"))
        {
            expect(range, range["lt"].as<double>(), false);
            expect(range, range["lt"].as<double>() - step, true);
        }
        else
        {
            expect(range, range["max"].as<double>(), true);
            expect(range, range["max"].as<double>() + step, false);
        }
    }
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_every_shared_range_matches_the_device);
    return UNITY_END();
}
