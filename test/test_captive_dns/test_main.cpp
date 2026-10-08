#include <unity.h>

#include <string>
#include <vector>

#include "CaptiveDns.h"

using namespace SQM;

namespace
{
    const uint8_t IP[4] = {192, 168, 4, 1};

    std::vector<uint8_t> query(const std::string &name, uint16_t type, uint16_t id = 0x1234)
    {
        std::vector<uint8_t> q = {static_cast<uint8_t>(id >> 8), static_cast<uint8_t>(id), 0x01, 0x00, 0, 1, 0, 0, 0, 0, 0, 0};
        size_t start = 0;
        while (start <= name.size())
        {
            size_t dot = name.find('.', start);
            if (dot == std::string::npos)
                dot = name.size();
            q.push_back(static_cast<uint8_t>(dot - start));
            q.insert(q.end(), name.begin() + start, name.begin() + dot);
            start = dot + 1;
        }
        q.push_back(0);
        q.push_back(static_cast<uint8_t>(type >> 8));
        q.push_back(static_cast<uint8_t>(type));
        q.push_back(0);
        q.push_back(1);
        return q;
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_a_query_gets_device_ip()
{
    const auto q = query("captive.apple.com", 1);
    std::vector<uint8_t> r;
    TEST_ASSERT_TRUE(CaptiveDns::buildResponse(q.data(), q.size(), IP, r));
    TEST_ASSERT_EQUAL_HEX8(0x12, r[0]);
    TEST_ASSERT_EQUAL_HEX8(0x34, r[1]);
    TEST_ASSERT_EQUAL_HEX8(0x85, r[2]); // response, authoritative, RD copied
    TEST_ASSERT_EQUAL_HEX8(0x00, r[3]); // no error
    TEST_ASSERT_EQUAL(1, r[7]);         // one answer
    // Question echoed, then the answer ending in the address.
    TEST_ASSERT_EQUAL_MEMORY(q.data() + 12, r.data() + 12, q.size() - 12);
    TEST_ASSERT_EQUAL(q.size() + 16, r.size());
    TEST_ASSERT_EQUAL_MEMORY(IP, r.data() + r.size() - 4, 4);
}

void test_other_types_get_empty_answer()
{
    for (uint16_t type : {28, 65, 16})
    {
        const auto q = query("captive.apple.com", type);
        std::vector<uint8_t> r;
        TEST_ASSERT_TRUE(CaptiveDns::buildResponse(q.data(), q.size(), IP, r));
        TEST_ASSERT_EQUAL(0, r[7]);         // no answers
        TEST_ASSERT_EQUAL_HEX8(0x00, r[3]); // but no error either
        TEST_ASSERT_EQUAL(q.size(), r.size());
    }
}

void test_rejects_non_queries_and_garbage()
{
    std::vector<uint8_t> r;
    auto q = query("example.com", 1);
    q[2] |= 0x80; // a response, not a query
    TEST_ASSERT_FALSE(CaptiveDns::buildResponse(q.data(), q.size(), IP, r));

    q = query("example.com", 1);
    q[5] = 2; // two questions
    TEST_ASSERT_FALSE(CaptiveDns::buildResponse(q.data(), q.size(), IP, r));

    q = query("example.com", 1);
    q.resize(q.size() - 3); // truncated
    TEST_ASSERT_FALSE(CaptiveDns::buildResponse(q.data(), q.size(), IP, r));

    const uint8_t tiny[5] = {1, 2, 3, 4, 5};
    TEST_ASSERT_FALSE(CaptiveDns::buildResponse(tiny, sizeof(tiny), IP, r));
    TEST_ASSERT_FALSE(CaptiveDns::buildResponse(nullptr, 0, IP, r));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_a_query_gets_device_ip);
    RUN_TEST(test_other_types_get_empty_answer);
    RUN_TEST(test_rejects_non_queries_and_garbage);
    return UNITY_END();
}
