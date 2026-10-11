#include <unity.h>

#include "LanguageLogic.h"

using namespace SQM;

void setUp() {}
void tearDown() {}

void test_supported_codes()
{
    TEST_ASSERT_TRUE(Language::isSupported("en"));
    TEST_ASSERT_TRUE(Language::isSupported("ar"));
    TEST_ASSERT_TRUE(Language::isSupported("pt-BR"));
    TEST_ASSERT_TRUE(Language::isSupported("zh-Hans"));
    TEST_ASSERT_FALSE(Language::isSupported("pt"));
    TEST_ASSERT_FALSE(Language::isSupported(""));
    TEST_ASSERT_FALSE(Language::isSupported("../etc"));
}

void test_asset_urls_follow_the_release()
{
    TEST_ASSERT_EQUAL_STRING("sqmeter-i18n-es.json.gz", Language::assetName("es").c_str());
    TEST_ASSERT_EQUAL_STRING(
        "https://github.com/DeanJ87/SQMeter/releases/download/v0.2.1/sqmeter-i18n-es.json.gz",
        Language::assetUrl("0.2.1", Language::assetName("es")).c_str());
}

void test_checksum_file()
{
    Language::Checksum sum;
    std::string error;
    TEST_ASSERT_EQUAL_STRING("sqmeter-i18n-es.json.gz.sha256", Language::checksumName(Language::assetName("es")).c_str());
    TEST_ASSERT_TRUE(Language::parseChecksum("ABCDEF0123456789abcdef0123456789abcdef0123456789abcdef0123456789 18000\n", sum, error));
    TEST_ASSERT_EQUAL(18000, sum.size);
    TEST_ASSERT_EQUAL_STRING("abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789", sum.sha256.c_str());
}

void test_damaged_checksum_is_refused()
{
    Language::Checksum sum;
    std::string error;
    TEST_ASSERT_FALSE(Language::parseChecksum("zz 18000", sum, error));
    TEST_ASSERT_FALSE(Language::parseChecksum("abcdef0123456789abcdef0123456789abcdef0123456789abcdef0123456789 999999", sum, error));
    TEST_ASSERT_FALSE(Language::parseChecksum("", sum, error));
    TEST_ASSERT_EQUAL_STRING("The language file's checksum is missing or damaged", error.c_str());
}

void test_file_checks()
{
    const uint8_t gzip[] = {0x1f, 0x8b, 0x08};
    const uint8_t json[] = {'{', '"'};
    TEST_ASSERT_EQUAL(Language::FileCheck::Ok, Language::checkFile(gzip, 3, 20000, 100000));
    TEST_ASSERT_EQUAL(Language::FileCheck::NotGzip, Language::checkFile(json, 2, 20000, 100000));
    TEST_ASSERT_EQUAL(Language::FileCheck::TooBig, Language::checkFile(gzip, 3, 70000, 1000000));
    TEST_ASSERT_EQUAL(Language::FileCheck::NoSpace, Language::checkFile(gzip, 3, 20000, 22000));
    TEST_ASSERT_TRUE(std::string(Language::fileCheckMessage(Language::FileCheck::NoSpace)).size() > 0);
}

void test_boot_action()
{
    using Language::BootAction;
    TEST_ASSERT_EQUAL(BootAction::Nothing, Language::bootAction("en", false, true));
    TEST_ASSERT_EQUAL(BootAction::UseInstalled, Language::bootAction("es", true, false));
    TEST_ASSERT_EQUAL(BootAction::UseInstalled, Language::bootAction("es", true, true));
    TEST_ASSERT_EQUAL(BootAction::Restore, Language::bootAction("es", false, false));
}

// A download that never finished (it crashed the device, or the power went)
// must not start again by itself, or a crash would repeat on every boot.
void test_unfinished_download_is_not_retried_at_boot()
{
    TEST_ASSERT_EQUAL(Language::BootAction::WaitAfterCrash, Language::bootAction("es", false, true));
}

void test_change_action()
{
    using Language::ChangeAction;
    TEST_ASSERT_EQUAL(ChangeAction::RemoveFile, Language::changeAction("en", false, false));
    TEST_ASSERT_EQUAL(ChangeAction::UseInstalled, Language::changeAction("es", true, false));
    TEST_ASSERT_EQUAL(ChangeAction::Download, Language::changeAction("es", false, false));
}

// Choosing a language while another download runs (a boot restore of the old
// one) must not be refused: it waits, then fetches the new choice.
void test_change_during_a_download_waits_for_it()
{
    using Language::ChangeAction;
    TEST_ASSERT_EQUAL(ChangeAction::AfterRunning, Language::changeAction("es", false, true));
    TEST_ASSERT_EQUAL(ChangeAction::AfterRunning, Language::changeAction("en", false, true));
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_boot_action);
    RUN_TEST(test_unfinished_download_is_not_retried_at_boot);
    RUN_TEST(test_change_action);
    RUN_TEST(test_change_during_a_download_waits_for_it);
    RUN_TEST(test_supported_codes);
    RUN_TEST(test_asset_urls_follow_the_release);
    RUN_TEST(test_checksum_file);
    RUN_TEST(test_damaged_checksum_is_refused);
    RUN_TEST(test_file_checks);
    return UNITY_END();
}
