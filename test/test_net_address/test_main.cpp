#include <unity.h>

#include <ArduinoJson.h>

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "NetAddress.h"

using namespace SQM;

// IPv6 addresses, hosts and URLs (specs/015-ipv6-dual-stack). The fixture
// cases are shared with web/src/lib/__tests__/netAddress.test.ts.

namespace
{
    DynamicJsonDocument fixtures(32768);

    std::string readFile(const char *path)
    {
        std::ifstream in(path);
        std::stringstream text;
        text << in.rdbuf();
        return text.str();
    }

    Net::Ipv6 address(const char *text)
    {
        Net::Ipv6 out{};
        TEST_ASSERT_TRUE_MESSAGE(Net::parseIpv6(text, out), text);
        return out;
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_addresses_parse_format_and_scope()
{
    for (JsonObjectConst c : fixtures["addresses"].as<JsonArrayConst>())
    {
        const std::string in = c["in"].as<std::string>();
        Net::Ipv6 parsed{};
        const bool ok = Net::parseIpv6(in, parsed);
        if (c["invalid"] | false)
        {
            TEST_ASSERT_FALSE_MESSAGE(ok, in.c_str());
            continue;
        }
        TEST_ASSERT_TRUE_MESSAGE(ok, in.c_str());
        TEST_ASSERT_EQUAL_STRING_MESSAGE(c["out"].as<const char *>(), Net::formatIpv6(parsed).c_str(), in.c_str());
        TEST_ASSERT_EQUAL_STRING_MESSAGE(c["scope"].as<const char *>(), Net::scopeName(Net::scopeOf(parsed)), in.c_str());
        TEST_ASSERT_EQUAL_MESSAGE(c["mapped"] | false, Net::isIpv4Mapped(parsed), in.c_str());
    }
}

void test_ipv6_peers_only_from_the_local_network()
{
    for (JsonObjectConst c : fixtures["peers"].as<JsonArrayConst>())
    {
        std::vector<Net::Ipv6> own;
        for (JsonVariantConst mine : c["own"].as<JsonArrayConst>())
            own.push_back(address(mine.as<const char *>()));
        const char *peer = c["peer"].as<const char *>();
        TEST_ASSERT_EQUAL_MESSAGE(c["allowed"].as<bool>(), Net::allowedPeer(address(peer), own), peer);
    }
}

void test_hosts_as_typed_in_settings()
{
    for (JsonObjectConst c : fixtures["hosts"].as<JsonArrayConst>())
    {
        const std::string in = c["in"].as<std::string>();
        Net::Host host;
        const Net::HostError error = Net::parseHost(in, host);
        if (c.containsKey("error"))
        {
            TEST_ASSERT_EQUAL_STRING_MESSAGE(c["error"].as<const char *>(), Net::hostErrorText(error), in.c_str());
            continue;
        }
        TEST_ASSERT_TRUE_MESSAGE(error == Net::HostError::None, in.c_str());
        TEST_ASSERT_EQUAL_STRING_MESSAGE(c["name"].as<const char *>(), host.name.c_str(), in.c_str());
        TEST_ASSERT_EQUAL_MESSAGE(c["ipv6"].as<bool>(), host.ipv6, in.c_str());
        TEST_ASSERT_EQUAL_MESSAGE(c["port"].as<unsigned>(), host.port, in.c_str());
    }
}

void test_http_urls_with_bracketed_ipv6()
{
    for (JsonObjectConst c : fixtures["urls"].as<JsonArrayConst>())
    {
        const std::string in = c["in"].as<std::string>();
        Net::HttpUrl url;
        Net::HostError hostError = Net::HostError::None;
        const Net::UrlError error = Net::parseHttpUrl(in, url, &hostError);
        if (c.containsKey("error"))
        {
            TEST_ASSERT_EQUAL_STRING_MESSAGE(c["error"].as<const char *>(), Net::urlErrorText(error, hostError).c_str(), in.c_str());
            continue;
        }
        TEST_ASSERT_TRUE_MESSAGE(error == Net::UrlError::None, in.c_str());
        TEST_ASSERT_EQUAL_MESSAGE(c["https"].as<bool>(), url.https, in.c_str());
        TEST_ASSERT_EQUAL_STRING_MESSAGE(c["host"].as<const char *>(), url.host.name.c_str(), in.c_str());
        TEST_ASSERT_EQUAL_MESSAGE(c["ipv6"].as<bool>(), url.host.ipv6, in.c_str());
        TEST_ASSERT_EQUAL_MESSAGE(c["port"].as<unsigned>(), url.port, in.c_str());
        TEST_ASSERT_EQUAL_STRING_MESSAGE(c["path"].as<const char *>(), url.path.c_str(), in.c_str());
    }
}

void test_host_for_url_brackets_ipv6_only()
{
    Net::Host v6;
    Net::parseHost("fd00::10", v6);
    TEST_ASSERT_EQUAL_STRING("[fd00::10]", Net::hostForUrl(v6).c_str());
    Net::Host name;
    Net::parseHost("broker.local", name);
    TEST_ASSERT_EQUAL_STRING("broker.local", Net::hostForUrl(name).c_str());
}

int main(int, char **)
{
    TEST_ASSERT_FALSE(deserializeJson(fixtures, readFile("test/fixtures/net-address/cases.json")));
    UNITY_BEGIN();
    RUN_TEST(test_addresses_parse_format_and_scope);
    RUN_TEST(test_ipv6_peers_only_from_the_local_network);
    RUN_TEST(test_hosts_as_typed_in_settings);
    RUN_TEST(test_http_urls_with_bracketed_ipv6);
    RUN_TEST(test_host_for_url_brackets_ipv6_only);
    return UNITY_END();
}
