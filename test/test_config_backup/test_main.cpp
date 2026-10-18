#include <unity.h>

#include <ArduinoJson.h>

#include <algorithm>
#include <fstream>
#include <functional>
#include <sstream>
#include <string>

#include "ConfigBackup.h"

using namespace SQM;

// Settings backup and restore (specs/033-config-backup): the file, its
// secrets, the preview and the rules behind it.

namespace
{
    std::string readFile(const char *path)
    {
        std::ifstream in(path);
        std::stringstream text;
        text << in.rdbuf();
        return text.str();
    }

    Backup::DeviceInfo device(bool bluetooth = true)
    {
        Backup::DeviceInfo info;
        info.firmware = "0.3.4-beta.1";
        info.board = "esp32";
        info.bluetoothBuild = bluetooth;
        info.pins = Backup::classicEsp32Pins();
        return info;
    }

    // A configured device: every kind of secret set.
    Config configured()
    {
        Config cfg = Config::createDefault();
        cfg.deviceName = "backyard-sqm";
        cfg.wifi.ssid = "Observatory";
        cfg.wifi.password = "wifi-secret-1";
        cfg.mqtt.enabled = true;
        cfg.mqtt.broker = "broker.lan";
        cfg.mqtt.password = "mqtt-secret-2";
        cfg.auth.enabled = true;
        cfg.auth.password = "admin-secret-3";
        cfg.ble.passkey = "123456";
        cfg.alerts.ntfyToken = "ntfy-secret-4";
        cfg.alerts.webhookAuthHeader = "Bearer webhook-secret-5";
        cfg.secondaryTimeSource = TimeSource::NTP; // as any save leaves it with GPS off
        return cfg;
    }

    Backup::SealParams sealWith(const char *passphrase, uint32_t iterations = Backup::MIN_ITERATIONS)
    {
        Backup::SealParams seal;
        seal.passphrase = passphrase;
        seal.iterations = iterations;
        for (size_t i = 0; i < Backup::SALT_BYTES; ++i)
            seal.salt[i] = static_cast<uint8_t>(i * 13 + 1);
        for (size_t i = 0; i < Backup::NONCE_BYTES; ++i)
            seal.nonce[i] = static_cast<uint8_t>(i * 7 + 5);
        return seal;
    }

    std::string backupOf(const Config &cfg, const char *passphrase = nullptr)
    {
        const Backup::SealParams seal = sealWith(passphrase ? passphrase : "");
        return Backup::makeBackup(cfg, device(), "2026-10-18T21:04:05Z", passphrase ? &seal : nullptr);
    }

    // The file with `edit` applied to its JSON.
    std::string edited(const std::string &file, const std::function<void(JsonObject)> &edit)
    {
        DynamicJsonDocument doc(file.size() + 8192);
        deserializeJson(doc, file);
        edit(doc.as<JsonObject>());
        std::string out;
        serializeJson(doc, out);
        return out;
    }

    struct Restore
    {
        Backup::Error error = Backup::Error::None;
        std::string reason;
        Backup::Parsed parsed;
        Backup::Preview preview;
    };

    Restore restore(const Config &current, const std::string &file, const char *passphrase = nullptr,
                    const Backup::DeviceInfo &info = device(), bool imagingApp = false)
    {
        Restore r;
        r.error = Backup::parseBackup(file, r.parsed);
        if (r.error != Backup::Error::None)
            return r;
        Backup::Secrets secrets;
        const bool open = passphrase && r.parsed.hasSecrets;
        if (open && (r.error = Backup::openSecrets(r.parsed, passphrase, secrets)) != Backup::Error::None)
            return r;
        Backup::PreviewInput input;
        input.current = &current;
        input.parsed = &r.parsed;
        input.secrets = open ? &secrets : nullptr;
        input.device = info;
        input.imagingAppConnected = imagingApp;
        r.error = Backup::buildPreview(input, r.preview, &r.reason);
        return r;
    }

    bool contains(const std::vector<std::string> &list, const char *item) { return std::find(list.begin(), list.end(), item) != list.end(); }

    const Backup::Change *changeAt(const Backup::Preview &p, const char *path)
    {
        for (const Backup::Change &c : p.changes)
            if (c.path == path)
                return &c;
        return nullptr;
    }

    bool hasWarning(const Backup::Preview &p, Backup::Warning w) { return std::find(p.warnings.begin(), p.warnings.end(), w) != p.warnings.end(); }

    const char *const SECRET_VALUES[] = {"wifi-secret-1", "mqtt-secret-2", "admin-secret-3", "123456", "ntfy-secret-4", "webhook-secret-5"};
} // namespace

// --- Backup (US1) -------------------------------------------------------------

void test_backup_without_passphrase_holds_no_secret()
{
    const std::string file = backupOf(configured());
    for (const char *secret : SECRET_VALUES)
        TEST_ASSERT_TRUE_MESSAGE(file.find(secret) == std::string::npos, secret); // SC-002
    TEST_ASSERT_TRUE(file.find("\"secrets\"") == std::string::npos);
}

void test_backup_holds_every_section_and_the_header()
{
    DynamicJsonDocument doc(16384);
    deserializeJson(doc, backupOf(configured()));
    TEST_ASSERT_EQUAL(1, doc["sqmeterBackup"].as<int>());
    TEST_ASSERT_EQUAL_STRING("0.3.4-beta.1", doc["firmware"]);
    TEST_ASSERT_EQUAL_STRING("esp32", doc["board"]);
    TEST_ASSERT_EQUAL_STRING("ble", doc["build"]);
    TEST_ASSERT_EQUAL_STRING("backyard-sqm", doc["deviceName"]);
    TEST_ASSERT_EQUAL_STRING("2026-10-18T21:04:05Z", doc["createdAt"]);
    for (const char *section : {"wifi", "mqtt", "ota", "auth", "ntp", "gps", "rain", "sensor", "skyAveraging", "skyCalibration",
                                "cloudDetection", "alpaca", "ble", "location", "wind", "alerts"})
        TEST_ASSERT_TRUE_MESSAGE(doc["settings"][section].is<JsonObject>(), section);
    TEST_ASSERT_EQUAL_STRING("********", doc["settings"]["wifi"]["password"]);
    TEST_ASSERT_EQUAL_STRING("", doc["settings"]["ota"]["password"]);
}

void test_backup_leaves_out_created_at_when_the_clock_is_unset()
{
    const std::string file = Backup::makeBackup(configured(), device(), "", nullptr);
    TEST_ASSERT_TRUE(file.find("createdAt") == std::string::npos);
}

void test_backup_with_passphrase_encrypts_the_secrets()
{
    const std::string file = backupOf(configured(), "a long passphrase");
    for (const char *secret : SECRET_VALUES)
        TEST_ASSERT_TRUE_MESSAGE(file.find(secret) == std::string::npos, secret);
    DynamicJsonDocument doc(16384);
    deserializeJson(doc, file);
    TEST_ASSERT_EQUAL_STRING("pbkdf2-sha256", doc["secrets"]["kdf"]);
    TEST_ASSERT_EQUAL_STRING("aes-256-gcm", doc["secrets"]["cipher"]);
    TEST_ASSERT_EQUAL(Backup::MIN_ITERATIONS, doc["secrets"]["iterations"].as<uint32_t>());
}

// --- Secrets (US3) ------------------------------------------------------------

void test_restore_with_the_passphrase_brings_the_secrets_back()
{
    Config fresh = Config::createDefault();
    fresh.wifi.ssid = "Setup";
    const Restore r = restore(fresh, backupOf(configured(), "a long passphrase"), "a long passphrase");
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_EQUAL(Backup::SecretsState::Restored, r.preview.secretsState);
    TEST_ASSERT_EQUAL_STRING("wifi-secret-1", r.preview.candidate.wifi.password.c_str());
    TEST_ASSERT_EQUAL_STRING("mqtt-secret-2", r.preview.candidate.mqtt.password.c_str());
    TEST_ASSERT_EQUAL_STRING("Bearer webhook-secret-5", r.preview.candidate.alerts.webhookAuthHeader.c_str());
    TEST_ASSERT_TRUE(contains(r.preview.secretsChanged, "wifi.password"));
    TEST_ASSERT_TRUE(contains(r.preview.secretsUnchanged, "ota.password"));
    TEST_ASSERT_NULL(changeAt(r.preview, "wifi.password")); // secrets never listed with values
}

void test_restore_with_the_default_iterations_round_trips()
{
    const Backup::SealParams seal = sealWith("default iterations", Backup::DEFAULT_ITERATIONS);
    const std::string file = Backup::makeBackup(configured(), device(), "", &seal);
    const Restore r = restore(configured(), file, "default iterations");
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_TRUE(r.preview.secretsChanged.empty());
}

void test_wrong_passphrase_is_refused()
{
    const Restore r = restore(configured(), backupOf(configured(), "a long passphrase"), "the wrong one");
    TEST_ASSERT_EQUAL(Backup::Error::WrongPassphrase, r.error);
}

void test_changed_ciphertext_salt_or_nonce_reads_as_wrong_passphrase()
{
    const std::string file = backupOf(configured(), "a long passphrase");
    const char *fields[] = {"data", "salt", "nonce"};
    for (const char *field : fields)
    {
        const std::string changed = edited(file,
                                           [field](JsonObject f)
                                           {
                                               std::string value = f["secrets"][field].as<const char *>();
                                               value[2] = value[2] == 'A' ? 'B' : 'A';
                                               f["secrets"][field] = value;
                                           });
        TEST_ASSERT_EQUAL_MESSAGE(Backup::Error::WrongPassphrase, restore(configured(), changed, "a long passphrase").error, field);
    }
}

void test_restore_without_passphrase_keeps_this_devices_secrets()
{
    Config other = configured();
    other.wifi.password = "different";
    const Restore r = restore(configured(), backupOf(other, "a long passphrase"));
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_EQUAL(Backup::SecretsState::Kept, r.preview.secretsState);
    TEST_ASSERT_EQUAL_STRING("wifi-secret-1", r.preview.candidate.wifi.password.c_str());
    TEST_ASSERT_TRUE(r.preview.changes.empty());
}

void test_an_empty_secret_in_the_backup_clears_it()
{
    Config other = configured();
    other.alerts.ntfyToken.clear();
    const Restore r = restore(configured(), backupOf(other, "a long passphrase"), "a long passphrase");
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_TRUE(r.preview.candidate.alerts.ntfyToken.empty());
    TEST_ASSERT_TRUE(contains(r.preview.secretsChanged, "alerts.ntfy.token"));
}

void test_a_backup_encrypted_by_webcrypto_opens()
{
    Backup::Parsed parsed;
    TEST_ASSERT_EQUAL(Backup::Error::None, Backup::parseBackup(readFile("test/fixtures/backup/webcrypto-backup.json"), parsed));
    Backup::Secrets secrets;
    TEST_ASSERT_EQUAL(Backup::Error::None, Backup::openSecrets(parsed, "correct horse battery", secrets));
    TEST_ASSERT_EQUAL(9, secrets.size());
    TEST_ASSERT_EQUAL_STRING("observatory-wifi", secrets[0].second.c_str());
    const Restore r = restore(configured(), readFile("test/fixtures/backup/webcrypto-backup.json"), "correct horse battery");
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_EQUAL_STRING("tk_ntfy", r.preview.candidate.alerts.ntfyToken.c_str());
    TEST_ASSERT_EQUAL_STRING("webcrypto-fixture", r.preview.candidate.deviceName.c_str());
}

// --- Preview (US2) ------------------------------------------------------------

void test_an_unchanged_backup_changes_nothing()
{
    const Restore r = restore(configured(), backupOf(configured(), "a long passphrase"), "a long passphrase");
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_TRUE(r.preview.changes.empty());
    TEST_ASSERT_TRUE(r.preview.secretsChanged.empty());
    TEST_ASSERT_TRUE(r.preview.kept.empty());
    TEST_ASSERT_TRUE(r.preview.ignored.empty());
    TEST_ASSERT_TRUE(r.preview.notApplicable.empty());
    TEST_ASSERT_EQUAL(0, r.preview.restart);
    TEST_ASSERT_TRUE(r.preview.warnings.empty());
}

void test_the_preview_lists_exactly_the_differences()
{
    Config other = configured();
    other.mqtt.broker = "192.168.1.10";
    other.alpaca.sqmMinSafe = 20.5F;
    const Restore r = restore(configured(), backupOf(other));
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_EQUAL(2, r.preview.changes.size());
    const Backup::Change *broker = changeAt(r.preview, "mqtt.broker");
    TEST_ASSERT_NOT_NULL(broker);
    TEST_ASSERT_EQUAL_STRING("\"broker.lan\"", broker->from.c_str());
    TEST_ASSERT_EQUAL_STRING("\"192.168.1.10\"", broker->to.c_str());
    TEST_ASSERT_NOT_NULL(changeAt(r.preview, "alpaca.sqmMinSafe"));
    TEST_ASSERT_EQUAL_STRING("192.168.1.10", r.preview.candidate.mqtt.broker.c_str());
}

void test_settings_the_save_refuses_are_refused_with_its_reason()
{
    const std::string file = edited(backupOf(configured()), [](JsonObject f) { f["settings"]["mqtt"]["port"] = "abc"; });
    const Restore r = restore(configured(), file);
    TEST_ASSERT_EQUAL(Backup::Error::Invalid, r.error);
    TEST_ASSERT_TRUE(r.reason.find("mqtt.port") != std::string::npos);
}

void test_cut_off_warnings_and_restart()
{
    Config other = configured();
    other.wifi.ssid = "Elsewhere";
    other.wifi.hostname = "roof";
    other.auth.password = "another-password";
    const Restore r = restore(configured(), backupOf(other, "a long passphrase"), "a long passphrase", device(), true);
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_TRUE(hasWarning(r.preview, Backup::Warning::WifiNetwork));
    TEST_ASSERT_TRUE(hasWarning(r.preview, Backup::Warning::Address));
    TEST_ASSERT_TRUE(hasWarning(r.preview, Backup::Warning::DevicePassword));
    TEST_ASSERT_TRUE(hasWarning(r.preview, Backup::Warning::ImagingApp));
    TEST_ASSERT_EQUAL(Backup::RESTART_WIFI, r.preview.restart);
}

void test_no_imaging_app_warning_without_a_restart()
{
    Config other = configured();
    other.mqtt.broker = "elsewhere.lan";
    const Restore r = restore(configured(), backupOf(other), nullptr, device(), true);
    TEST_ASSERT_EQUAL(0, r.preview.restart);
    TEST_ASSERT_FALSE(hasWarning(r.preview, Backup::Warning::ImagingApp));
}

void test_restart_rules_match_the_shared_cases()
{
    const std::string text = readFile("test/fixtures/backup/restart-cases.json");
    DynamicJsonDocument doc(16384);
    TEST_ASSERT_FALSE(deserializeJson(doc, text));
    for (JsonObjectConst c : doc["cases"].as<JsonArrayConst>())
    {
        Config before = Config::createDefault();
        Config after = Config::createDefault();
        std::string json;
        serializeJson(c["before"], json);
        TEST_ASSERT_TRUE_MESSAGE(Config::applyJson(json, before, false), c["name"]);
        json.clear();
        serializeJson(c["after"], json);
        TEST_ASSERT_TRUE_MESSAGE(Config::applyJson(json, after, false), c["name"]);
        const uint8_t reasons = Backup::restartReasons(before, after);
        std::string got;
        for (int i = 0; i < Backup::RESTART_REASON_COUNT; ++i)
            if (reasons & (1U << i))
                got += std::string(got.empty() ? "" : ",") + Backup::restartCode(i);
        std::string want;
        for (JsonVariantConst code : c["reasons"].as<JsonArrayConst>())
            want += std::string(want.empty() ? "" : ",") + code.as<const char *>();
        TEST_ASSERT_EQUAL_STRING_MESSAGE(want.c_str(), got.c_str(), c["name"]);
    }
}

void test_preview_document_shape()
{
    Config other = configured();
    other.mqtt.broker = "elsewhere.lan";
    other.wifi.hostname = "roof";
    const Restore r = restore(configured(), backupOf(other));
    DynamicJsonDocument doc(16384);
    TEST_ASSERT_FALSE(deserializeJson(doc, Backup::previewJson(r.preview, r.parsed, "00ff")));
    TEST_ASSERT_EQUAL_STRING("00ff", doc["token"]);
    TEST_ASSERT_EQUAL(1, doc["backup"]["format"].as<int>());
    TEST_ASSERT_EQUAL_STRING("backyard-sqm", doc["backup"]["deviceName"]);
    TEST_ASSERT_EQUAL_STRING("none", doc["secrets"]["state"]);
    TEST_ASSERT_EQUAL_STRING("elsewhere.lan", doc["changes"][1]["to"].as<const char *>() ? doc["changes"][1]["to"] : doc["changes"][0]["to"]);
    TEST_ASSERT_EQUAL_STRING("wifi", doc["restart"][0]);
    TEST_ASSERT_EQUAL_STRING("address", doc["warnings"][0]["code"]);
    TEST_ASSERT_EQUAL_STRING("roof", doc["warnings"][0]["hostname"]);
}

// --- Boards and versions (US4) ------------------------------------------------

void test_pins_this_board_lacks_are_not_applied()
{
    Config other = configured();
    other.gps.rxPin = 25;
    other.gps.txPin = 26;
    other.wind.directionPin = 34;
    Backup::DeviceInfo s3 = device();
    s3.pins.validPins &= ~((1ULL << 25) | (1ULL << 26));
    s3.pins.analogPins &= ~(1ULL << 34);
    const Restore r = restore(configured(), backupOf(other), nullptr, s3);
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_EQUAL(3, r.preview.notApplicable.size());
    TEST_ASSERT_EQUAL_STRING("gps.rxPin", r.preview.notApplicable[0].path.c_str());
    TEST_ASSERT_EQUAL_STRING("25", r.preview.notApplicable[0].value.c_str());
    TEST_ASSERT_EQUAL(Backup::NotApplicableReason::Analog, r.preview.notApplicable[2].reason);
    TEST_ASSERT_EQUAL(17, r.preview.candidate.gps.rxPin); // this board's value
    TEST_ASSERT_EQUAL(35, r.preview.candidate.wind.directionPin);
}

void test_bluetooth_settings_are_not_applied_on_the_standard_build()
{
    Config other = configured();
    other.ble.enabled = true;
    other.ble.passkey = "654321";
    const Restore r = restore(configured(), backupOf(other, "a long passphrase"), "a long passphrase", device(false));
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_EQUAL(2, r.preview.notApplicable.size());
    TEST_ASSERT_EQUAL(Backup::NotApplicableReason::Bluetooth, r.preview.notApplicable[0].reason);
    TEST_ASSERT_TRUE(r.preview.notApplicable[1].value.empty()); // the passkey: no value
    TEST_ASSERT_FALSE(r.preview.candidate.ble.enabled);
    TEST_ASSERT_EQUAL_STRING("123456", r.preview.candidate.ble.passkey.c_str());
}

void test_pushover_without_its_keys_is_not_applied()
{
    Config other = configured();
    other.alerts.pushoverEnabled = true;
    other.alerts.pushoverUserKey = std::string(30, 'u');
    other.alerts.pushoverAppToken = std::string(30, 'a');
    const Restore r = restore(configured(), backupOf(other, "a long passphrase"));
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_EQUAL(1, r.preview.notApplicable.size());
    TEST_ASSERT_EQUAL(Backup::NotApplicableReason::PushoverKeys, r.preview.notApplicable[0].reason);
    TEST_ASSERT_FALSE(r.preview.candidate.alerts.pushoverEnabled);
    TEST_ASSERT_TRUE(restore(configured(), backupOf(other, "a long passphrase"), "a long passphrase").preview.candidate.alerts.pushoverEnabled);
}

void test_settings_an_older_firmware_lacked_are_kept()
{
    const std::string file = edited(backupOf(configured()),
                                    [](JsonObject f)
                                    {
                                        f["firmware"] = "0.2.0";
                                        f["settings"].remove("wind");
                                        f["settings"]["mqtt"]["publish"].remove("diagnostics");
                                    });
    const Restore r = restore(configured(), file);
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_EQUAL(2, r.preview.kept.size());
    TEST_ASSERT_TRUE(contains(r.preview.kept, "wind"));
    TEST_ASSERT_TRUE(contains(r.preview.kept, "mqtt.publish.diagnostics"));
    TEST_ASSERT_TRUE(r.preview.warnings.empty());
}

void test_settings_a_newer_firmware_wrote_are_ignored_with_a_warning()
{
    const std::string file = edited(backupOf(configured()),
                                    [](JsonObject f)
                                    {
                                        f["firmware"] = "0.9.0-beta.2";
                                        f["settings"]["skySensors"]["second"] = true;
                                        f["settings"]["wifi"]["band"] = "5GHz";
                                    });
    const Restore r = restore(configured(), file);
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_EQUAL(2, r.preview.ignored.size());
    TEST_ASSERT_TRUE(contains(r.preview.ignored, "skySensors"));
    TEST_ASSERT_TRUE(contains(r.preview.ignored, "wifi.band"));
    TEST_ASSERT_TRUE(hasWarning(r.preview, Backup::Warning::NewerFirmware));
}

void test_a_plain_settings_export_reads_as_format_0()
{
    const Restore r = restore(configured(), configured().toJson(true));
    TEST_ASSERT_EQUAL(Backup::Error::None, r.error);
    TEST_ASSERT_EQUAL(0, r.parsed.format);
    TEST_ASSERT_EQUAL(Backup::SecretsState::None, r.preview.secretsState);
    TEST_ASSERT_TRUE(r.preview.changes.empty());
}

void test_files_that_arent_backups_or_are_damaged_are_refused()
{
    Backup::Parsed parsed;
    const std::string good = backupOf(configured(), "a long passphrase");
    const struct
    {
        std::string file;
        Backup::Error error;
    } cases[] = {
        {"\x89PNG\r\n", Backup::Error::NotBackup},
        {"{\"name\":\"something else\"}", Backup::Error::NotBackup},
        {"[1,2]", Backup::Error::NotBackup},
        {good.substr(0, good.size() / 2), Backup::Error::Damaged},
        {edited(good, [](JsonObject f) { f["sqmeterBackup"] = "one"; }), Backup::Error::Damaged},
        {edited(good, [](JsonObject f) { f["sqmeterBackup"] = 2; }), Backup::Error::NewerFormat},
        {edited(good, [](JsonObject f) { f.remove("settings"); }), Backup::Error::Damaged},
        {edited(good, [](JsonObject f) { f["secrets"]["iterations"] = 50; }), Backup::Error::Damaged},
        {edited(good, [](JsonObject f) { f["secrets"]["iterations"] = 5000000; }), Backup::Error::Damaged},
        {edited(good, [](JsonObject f) { f["secrets"]["salt"] = "AAAA"; }), Backup::Error::Damaged},
        {edited(good, [](JsonObject f) { f["secrets"]["data"] = "not base64!"; }), Backup::Error::Damaged},
        {edited(good, [](JsonObject f) { f["secrets"]["kdf"] = "md5"; }), Backup::Error::Damaged},
        {std::string(Backup::MAX_BODY_BYTES + 1, ' '), Backup::Error::TooLarge},
    };
    for (const auto &c : cases)
        TEST_ASSERT_EQUAL(c.error, Backup::parseBackup(c.file, parsed));
}

// --- Requests -----------------------------------------------------------------

void test_backup_request_checks_the_passphrase()
{
    std::string passphrase;
    TEST_ASSERT_EQUAL(Backup::Error::None, Backup::readBackupRequest("", passphrase));
    TEST_ASSERT_EQUAL(Backup::Error::None, Backup::readBackupRequest("{}", passphrase));
    TEST_ASSERT_EQUAL(Backup::Error::PassphraseShort, Backup::readBackupRequest("{\"passphrase\":\"short\"}", passphrase));
    // Eight characters, more than eight bytes: counted as the web UI counts them.
    TEST_ASSERT_EQUAL(Backup::Error::None, Backup::readBackupRequest("{\"passphrase\":\"\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\"}", passphrase));
    TEST_ASSERT_EQUAL(Backup::Error::PassphraseShort, Backup::readBackupRequest("{\"passphrase\":\"\xc3\xa9\xc3\xa9\xc3\xa9\xc3\xa9\"}", passphrase));
    TEST_ASSERT_EQUAL(Backup::Error::BadRequest, Backup::readBackupRequest("passphrase", passphrase));
}

void test_preview_request_takes_the_file_as_text_or_object()
{
    const std::string file = backupOf(configured());
    Backup::Parsed parsed;
    std::string passphrase;
    DynamicJsonDocument body(16384);
    body["backup"] = file;
    body["passphrase"] = "secret words";
    std::string text;
    serializeJson(body, text);
    TEST_ASSERT_EQUAL(Backup::Error::None, Backup::readPreviewRequest(text, parsed, passphrase));
    TEST_ASSERT_EQUAL_STRING("secret words", passphrase.c_str());
    TEST_ASSERT_EQUAL(1, parsed.format);
    const std::string asObject = "{\"backup\":" + file + "}";
    TEST_ASSERT_EQUAL(Backup::Error::None, Backup::readPreviewRequest(asObject, parsed, passphrase));
    TEST_ASSERT_EQUAL(Backup::Error::TooLarge, Backup::readPreviewRequest(std::string(Backup::MAX_BODY_BYTES + 1, ' '), parsed, passphrase));
    TEST_ASSERT_EQUAL(Backup::Error::NotBackup, Backup::readPreviewRequest("{\"backup\":3}", parsed, passphrase));
}

void test_versions_compare_numerically_with_releases_after_betas()
{
    TEST_ASSERT_EQUAL(-1, Backup::compareVersions("0.3.4-beta.1", "0.3.4-beta.2"));
    TEST_ASSERT_EQUAL(-1, Backup::compareVersions("0.3.4-beta.9", "0.3.4"));
    TEST_ASSERT_EQUAL(1, Backup::compareVersions("0.10.0", "0.9.9"));
    TEST_ASSERT_EQUAL(0, Backup::compareVersions("v0.3.4-beta.1", "0.3.4-beta.1+dev"));
    TEST_ASSERT_EQUAL(-1, Backup::compareVersions("0.3.4-beta.10", "0.3.4-beta.11+dev"));
}

void test_error_json_has_code_and_text()
{
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"Wrong passphrase, or the file's secrets were changed\",\"code\":\"wrong-passphrase\"}",
                             Backup::errorJson(Backup::Error::WrongPassphrase).c_str());
    TEST_ASSERT_EQUAL_STRING("{\"error\":\"MQTT broker: bad\",\"code\":\"invalid\"}", Backup::errorJson(Backup::Error::Invalid, "MQTT broker: bad").c_str());
}

int main(int, char **)
{
    UNITY_BEGIN();
    RUN_TEST(test_backup_without_passphrase_holds_no_secret);
    RUN_TEST(test_backup_holds_every_section_and_the_header);
    RUN_TEST(test_backup_leaves_out_created_at_when_the_clock_is_unset);
    RUN_TEST(test_backup_with_passphrase_encrypts_the_secrets);
    RUN_TEST(test_restore_with_the_passphrase_brings_the_secrets_back);
    RUN_TEST(test_restore_with_the_default_iterations_round_trips);
    RUN_TEST(test_wrong_passphrase_is_refused);
    RUN_TEST(test_changed_ciphertext_salt_or_nonce_reads_as_wrong_passphrase);
    RUN_TEST(test_restore_without_passphrase_keeps_this_devices_secrets);
    RUN_TEST(test_an_empty_secret_in_the_backup_clears_it);
    RUN_TEST(test_a_backup_encrypted_by_webcrypto_opens);
    RUN_TEST(test_an_unchanged_backup_changes_nothing);
    RUN_TEST(test_the_preview_lists_exactly_the_differences);
    RUN_TEST(test_settings_the_save_refuses_are_refused_with_its_reason);
    RUN_TEST(test_cut_off_warnings_and_restart);
    RUN_TEST(test_no_imaging_app_warning_without_a_restart);
    RUN_TEST(test_restart_rules_match_the_shared_cases);
    RUN_TEST(test_preview_document_shape);
    RUN_TEST(test_pins_this_board_lacks_are_not_applied);
    RUN_TEST(test_bluetooth_settings_are_not_applied_on_the_standard_build);
    RUN_TEST(test_pushover_without_its_keys_is_not_applied);
    RUN_TEST(test_settings_an_older_firmware_lacked_are_kept);
    RUN_TEST(test_settings_a_newer_firmware_wrote_are_ignored_with_a_warning);
    RUN_TEST(test_a_plain_settings_export_reads_as_format_0);
    RUN_TEST(test_files_that_arent_backups_or_are_damaged_are_refused);
    RUN_TEST(test_backup_request_checks_the_passphrase);
    RUN_TEST(test_preview_request_takes_the_file_as_text_or_object);
    RUN_TEST(test_versions_compare_numerically_with_releases_after_betas);
    RUN_TEST(test_error_json_has_code_and_text);
    return UNITY_END();
}
