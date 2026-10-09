#include <unity.h>

#include "ReleaseLogic.h"

using namespace SQM;

namespace
{
    std::string asset(const char *name, int size = 1000)
    {
        return std::string("{\"name\":\"") + name + "\",\"size\":" + std::to_string(size) +
               ",\"browser_download_url\":\"https://example.com/" + name + "\",\"uploader\":{\"login\":\"x\"}}";
    }

    std::string release(const char *tag, bool prerelease, const std::string &assets, bool draft = false)
    {
        return std::string("{\"tag_name\":\"") + tag + "\",\"name\":\"" + tag +
               " title\",\"prerelease\":" + (prerelease ? "true" : "false") + ",\"draft\":" + (draft ? "true" : "false") +
               ",\"published_at\":\"2026-10-01T00:00:00Z\",\"body\":\"long release notes\",\"assets\":[" + assets + "]}";
    }

    const std::string FULL = asset("sqmeter-l2-firmware-v1.bin", 1500000) + "," + asset("sqmeter-l2-ble-firmware-v1.bin", 1700000) + "," +
                             asset("sqmeter-l2-littlefs-v1.bin", 458752);

    // A v0.2.x release: the old layout's files only.
    const std::string OLD_LAYOUT = asset("sqmeter-firmware-v0.bin", 1490000) + "," + asset("sqmeter-ble-firmware-v0.bin", 1719000) + "," +
                                   asset("sqmeter-littlefs-v0.bin", 524288);

    std::vector<GithubRelease> parse(const std::string &json, const char *track, bool ble)
    {
        std::vector<GithubRelease> out;
        std::string error;
        TEST_ASSERT_TRUE(Releases::parse(json, track, ble, out, error));
        return out;
    }
} // namespace

void setUp() {}
void tearDown() {}

void test_track_filter()
{
    const std::string json = "[" + release("v0.3.0", false, FULL) + "," + release("v0.3.1-beta.1", true, FULL) + "]";
    auto stable = parse(json, "stable", false);
    TEST_ASSERT_EQUAL(1, stable.size());
    TEST_ASSERT_EQUAL_STRING("v0.3.0", stable[0].tag.c_str());
    TEST_ASSERT_FALSE(stable[0].prerelease);

    auto beta = parse(json, "beta", false);
    TEST_ASSERT_EQUAL(1, beta.size());
    TEST_ASSERT_EQUAL_STRING("v0.3.1-beta.1", beta[0].tag.c_str());
    TEST_ASSERT_TRUE(beta[0].prerelease);
    TEST_ASSERT_EQUAL_STRING("v0.3.1-beta.1 title", beta[0].name.c_str());
    TEST_ASSERT_EQUAL_STRING("2026-10-01T00:00:00Z", beta[0].publishedAt.c_str());
}

void test_picks_firmware_for_the_build()
{
    const std::string json = "[" + release("v1", false, FULL) + "]";
    auto standard = parse(json, "stable", false);
    TEST_ASSERT_EQUAL(1, standard.size());
    TEST_ASSERT_EQUAL_STRING(
        "https://github.com/DeanJ87/SQMeter/releases/download/v1/sqmeter-l2-firmware-v1.bin", standard[0].firmwareAssetUrl.c_str());
    TEST_ASSERT_EQUAL(1500000, standard[0].firmwareAssetSize);
    TEST_ASSERT_EQUAL_STRING(
        "https://github.com/DeanJ87/SQMeter/releases/download/v1/sqmeter-l2-littlefs-v1.bin", standard[0].fsAssetUrl.c_str());
    TEST_ASSERT_EQUAL(458752, standard[0].fsAssetSize);

    auto ble = parse(json, "stable", true);
    TEST_ASSERT_EQUAL(1, ble.size());
    TEST_ASSERT_EQUAL_STRING(
        "https://github.com/DeanJ87/SQMeter/releases/download/v1/sqmeter-l2-ble-firmware-v1.bin", ble[0].firmwareAssetUrl.c_str());
}

void test_old_layout_releases_are_not_offered()
{
    // Spec 027 FR-020: v0.2.x files are for the old partition layout.
    const std::string json = "[" + release("v0.2.0-beta.3", true, OLD_LAYOUT) + "," + release("v0.3.0-beta.1", true, FULL) + "]";
    auto beta = parse(json, "beta", false);
    TEST_ASSERT_EQUAL(1, beta.size());
    TEST_ASSERT_EQUAL_STRING("v0.3.0-beta.1", beta[0].tag.c_str());
}

void test_skips_incomplete_releases()
{
    const std::string noFs = release("v2", false, asset("sqmeter-l2-firmware-v2.bin") + "," + asset("sqmeter-l2-ble-firmware-v2.bin"));
    const std::string noBle = release("v3", false, asset("sqmeter-l2-firmware-v3.bin") + "," + asset("sqmeter-l2-littlefs-v3.bin"));
    const std::string json = "[" + noFs + "," + noBle + "]";
    auto standard = parse(json, "stable", false);
    TEST_ASSERT_EQUAL(1, standard.size());
    TEST_ASSERT_EQUAL_STRING("v3", standard[0].tag.c_str());
    TEST_ASSERT_EQUAL(0, parse(json, "stable", true).size());
}

void test_ignores_non_bin_and_drafts()
{
    const std::string sums = asset("sqmeter-l2-firmware-v4.bin.sha256") + "," + asset("sqmeter-l2-littlefs-v4.bin");
    const std::string json = "[" + release("v4", false, sums) + "," + release("v5", false, FULL, true) + "]";
    TEST_ASSERT_EQUAL(0, parse(json, "stable", false).size());
}

void test_full_page_of_releases_fits()
{
    // A full page of v0.3-style releases: the firmware, web UI and USB-flash
    // files plus 13 languages (file + checksum) and the manifest, with
    // real-length URLs and the release notes and uploader details the filter
    // drops.
    std::vector<std::string> kinds = {
        "sqmeter-l2-firmware-", "sqmeter-l2-ble-firmware-", "sqmeter-l2-littlefs-", "sqmeter-l2-usb-standard-", "sqmeter-l2-usb-ble-",
        "sqmeter-i18n-manifest-", "sqmeter-checksums-"};
    for (const char *code : {"ar", "de", "es", "fr", "id", "it", "ja", "ko", "nl", "pl", "pt-BR", "tr", "zh-Hans"})
    {
        kinds.push_back(std::string("sqmeter-i18n-") + code + ".json.gz.");
        kinds.push_back(std::string("sqmeter-i18n-") + code + ".json.gz.sha256.");
    }
    std::string json = "[";
    for (int i = 0; i < Releases::PER_PAGE; ++i)
    {
        const std::string tag = "v1.2." + std::to_string(i) + "-beta.10";
        std::string assets;
        for (const std::string &kind : kinds)
        {
            if (!assets.empty())
                assets += ",";
            assets += "{\"name\":\"" + kind + tag +
                      ".bin\",\"size\":1700000,"
                      "\"browser_download_url\":\"https://github.com/DeanJ87/SQMeter/releases/download/" +
                      tag + "/" + kind + tag + ".bin\",\"uploader\":{\"login\":\"github-actions[bot]\",\"id\":41898282}}";
        }
        if (i > 0)
            json += ",";
        json += "{\"tag_name\":\"" + tag + "\",\"name\":\"SQMeter " + tag +
                "\",\"prerelease\":true,\"draft\":false,"
                "\"published_at\":\"2026-10-08T16:20:00Z\",\"body\":\"" +
                std::string(3000, 'x') + "\",\"assets\":[" + assets + "]}";
    }
    json += "]";

    std::vector<GithubRelease> out;
    std::string error;
    TEST_ASSERT_TRUE_MESSAGE(Releases::parse(json, "beta", true, out, error), error.c_str());
    TEST_ASSERT_EQUAL(Releases::PER_PAGE, out.size());
    TEST_ASSERT_EQUAL_STRING(
        "https://github.com/DeanJ87/SQMeter/releases/download/v1.2.7-beta.10/sqmeter-l2-ble-firmware-v1.2.7-beta.10.bin",
        out.back().firmwareAssetUrl.c_str());
}

void test_bad_json()
{
    std::vector<GithubRelease> out;
    std::string error;
    TEST_ASSERT_FALSE(Releases::parse("{not json", "stable", false, out, error));
    TEST_ASSERT_TRUE(error.find("GitHub") != std::string::npos);
    TEST_ASSERT_EQUAL(0, out.size());
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_track_filter);
    RUN_TEST(test_picks_firmware_for_the_build);
    RUN_TEST(test_old_layout_releases_are_not_offered);
    RUN_TEST(test_skips_incomplete_releases);
    RUN_TEST(test_ignores_non_bin_and_drafts);
    RUN_TEST(test_full_page_of_releases_fits);
    RUN_TEST(test_bad_json);
    return UNITY_END();
}
