#include <unity.h>

#include "FirmwareImage.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

using namespace SQM;
using FirmwareImage::Build;
using FirmwareImage::Verdict;

namespace
{
    // An "image": filler, the marker somewhere in the middle, more filler.
    std::vector<uint8_t> image(const std::string &marker, size_t before = 5000, size_t after = 3000)
    {
        std::vector<uint8_t> bytes(before, 0x5A);
        bytes.insert(bytes.end(), marker.begin(), marker.end());
        bytes.insert(bytes.end(), after, 0xC3);
        return bytes;
    }

    FirmwareImage::Scanner scan(const std::vector<uint8_t> &bytes, size_t chunk)
    {
        FirmwareImage::Scanner scanner;
        for (size_t at = 0; at < bytes.size(); at += chunk)
            scanner.feed(bytes.data() + at, std::min(chunk, bytes.size() - at));
        return scanner;
    }

    std::string withFields(const std::string &fields)
    {
        const auto magic = FirmwareImage::magic();
        std::string out(magic.begin(), magic.end());
        out += fields;
        out.push_back('\0');
        return out;
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_finds_marker_in_any_chunking()
{
    const auto bytes = image(FirmwareImage::marker(Build::Ble));
    for (size_t chunk : {1u, 7u, 16u, 17u, 1024u, 100000u})
    {
        const auto scanner = scan(bytes, chunk);
        TEST_ASSERT_TRUE(scanner.found());
        TEST_ASSERT_EQUAL_STRING("l2", scanner.layout().c_str());
        TEST_ASSERT_EQUAL_STRING("ble", scanner.build().c_str());
    }
}

void test_no_marker_in_an_old_image()
{
    std::vector<uint8_t> bytes(200000, 0xE9);
    TEST_ASSERT_FALSE(scan(bytes, 4096).found());
    TEST_ASSERT_EQUAL(Verdict::NoMarker, FirmwareImage::check(scan(bytes, 4096), "l2", Build::Standard));
}

void test_partial_magic_then_real_marker()
{
    // A run of the first magic bytes before the marker must not hide it.
    const auto magic = FirmwareImage::magic();
    std::string prefix(magic.begin(), magic.begin() + 9);
    const auto bytes = image(prefix + prefix + FirmwareImage::marker(Build::Standard));
    const auto scanner = scan(bytes, 3);
    TEST_ASSERT_TRUE(scanner.found());
    TEST_ASSERT_EQUAL_STRING("standard", scanner.build().c_str());
}

void test_stray_magic_without_fields_is_skipped()
{
    const auto bytes = image(withFields("") + std::string(40, 'x') + FirmwareImage::marker(Build::Standard));
    const auto scanner = scan(bytes, 64);
    TEST_ASSERT_TRUE(scanner.found());
    TEST_ASSERT_EQUAL_STRING("l2", scanner.layout().c_str());
}

void test_verdicts()
{
    const auto standardL2 = scan(image(FirmwareImage::marker(Build::Standard)), 512);
    const auto bleL2 = scan(image(FirmwareImage::marker(Build::Ble)), 512);
    const auto otherLayout = scan(image(withFields("layout=l3;build=standard;")), 512);

    TEST_ASSERT_EQUAL(Verdict::Ok, FirmwareImage::check(standardL2, "l2", Build::Standard));
    TEST_ASSERT_EQUAL(Verdict::Ok, FirmwareImage::check(bleL2, "l2", Build::Ble));
    TEST_ASSERT_EQUAL(Verdict::OtherBuild, FirmwareImage::check(bleL2, "l2", Build::Standard));
    TEST_ASSERT_EQUAL(Verdict::OtherBuild, FirmwareImage::check(standardL2, "l2", Build::Ble));
    TEST_ASSERT_EQUAL(Verdict::NeedsUsbFlash, FirmwareImage::check(standardL2, "legacy", Build::Standard));
    TEST_ASSERT_EQUAL(Verdict::OtherLayout, FirmwareImage::check(otherLayout, "l2", Build::Standard));
}

void test_messages_are_short()
{
    for (Verdict v : {Verdict::NoMarker, Verdict::NeedsUsbFlash, Verdict::OtherLayout, Verdict::OtherBuild})
        for (Build b : {Build::Standard, Build::Ble})
        {
            const std::string message = FirmwareImage::verdictMessage(v, b);
            TEST_ASSERT_TRUE(!message.empty());
            TEST_ASSERT_TRUE(message.size() <= 90); // DS-22
        }
}

void test_layout_of_partition_table()
{
    TEST_ASSERT_EQUAL_STRING("l2", FirmwareImage::layoutOf(0x1C0000, 0x70000).c_str());
    TEST_ASSERT_EQUAL_STRING("legacy", FirmwareImage::layoutOf(0x180000, 0x80000).c_str());
    TEST_ASSERT_EQUAL_STRING("legacy", FirmwareImage::layoutOf(0x1B0000, 0x80000).c_str());
}

void test_fs_image_must_fill_the_partition()
{
    TEST_ASSERT_TRUE(FirmwareImage::fsImageFits(0x70000 + 300, 0x70000, 4096));
    TEST_ASSERT_FALSE(FirmwareImage::fsImageFits(0x80000 + 300, 0x70000, 4096)); // old 512 KB image
    TEST_ASSERT_FALSE(FirmwareImage::fsImageFits(0x60000, 0x70000, 4096));
}

void test_firmware_embeds_the_same_marker()
{
    // src/FirmwareMarker.cpp spells the magic out; it must match the library's.
    std::ifstream in("src/FirmwareMarker.cpp");
    TEST_ASSERT_TRUE_MESSAGE(in.good(), "run from the project root");
    std::stringstream source;
    source << in.rdbuf();
    const auto magic = FirmwareImage::magic();
    std::string escaped;
    char buf[8];
    for (uint8_t b : magic)
    {
        snprintf(buf, sizeof(buf), "\\x%02X", b);
        escaped += buf;
    }
    TEST_ASSERT_TRUE_MESSAGE(source.str().find(escaped) != std::string::npos, escaped.c_str());
    TEST_ASSERT_TRUE(source.str().find("\"layout=l2;build=ble;\"") != std::string::npos);
    TEST_ASSERT_TRUE(source.str().find("\"layout=l2;build=standard;\"") != std::string::npos);
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_finds_marker_in_any_chunking);
    RUN_TEST(test_no_marker_in_an_old_image);
    RUN_TEST(test_partial_magic_then_real_marker);
    RUN_TEST(test_stray_magic_without_fields_is_skipped);
    RUN_TEST(test_verdicts);
    RUN_TEST(test_messages_are_short);
    RUN_TEST(test_layout_of_partition_table);
    RUN_TEST(test_fs_image_must_fill_the_partition);
    RUN_TEST(test_firmware_embeds_the_same_marker);
    return UNITY_END();
}
