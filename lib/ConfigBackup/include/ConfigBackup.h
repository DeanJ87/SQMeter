#pragma once

// Settings backup and restore (specs/033-config-backup): the backup file, the
// restore preview and the rules behind it, shared by the firmware
// (src/WebServerBackup.cpp), the native tests and the browser demo. No I/O:
// random bytes, the time and the device's facts come in as parameters.

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "BackupCrypto.h"
#include "Config.h"

namespace SQM::Backup
{
    constexpr int FORMAT_VERSION = 1;
    constexpr uint32_t DEFAULT_ITERATIONS = 20000; // ~1-2 s on an ESP32 (research R2)
    constexpr uint32_t MIN_ITERATIONS = 1000;
    constexpr uint32_t MAX_ITERATIONS = 100000; // bounds what a crafted file can cost
    constexpr size_t MAX_BODY_BYTES = 20480;    // request body, checked before parsing
    constexpr size_t MIN_PASSPHRASE_CHARS = 8;
    constexpr size_t TOKEN_BYTES = 16;
    constexpr const char *AAD = "sqmeter-backup/1"; // GCM associated data

    // Why a request was refused; codes and English texts in errorCode()/errorText().
    enum class Error : uint8_t
    {
        None,
        TooLarge,
        NotBackup,
        Damaged,
        NewerFormat,
        WrongPassphrase,
        PassphraseShort,
        BadRequest, // the request body isn't the JSON the route expects
        Invalid, // the save's own validation refused the result
    };
    const char *errorCode(Error error);
    const char *errorText(Error error);

    // GPIOs a board offers: any pin setting, and the wind vane's analog pin.
    struct BoardPins
    {
        uint64_t validPins = 0;
        uint64_t analogPins = 0;
        bool valid(int pin) const { return pin >= 0 && pin < 64 && ((validPins >> pin) & 1ULL) != 0; }
        bool analog(int pin) const { return pin >= 0 && pin < 64 && ((analogPins >> pin) & 1ULL) != 0; }
    };
    // The classic ESP32 (ConfigDetail::isValidGpio; ADC1 is GPIO 32-39).
    BoardPins classicEsp32Pins();

    // What the device restoring or backing up is.
    struct DeviceInfo
    {
        std::string firmware; // FIRMWARE_VERSION
        std::string board;    // "esp32"
        bool bluetoothBuild = false;
        BoardPins pins;
    };

    // A passphrase and the random salt and nonce to encrypt the secrets with.
    struct SealParams
    {
        std::string passphrase;
        uint8_t salt[SALT_BYTES] = {};
        uint8_t nonce[NONCE_BYTES] = {};
        uint32_t iterations = DEFAULT_ITERATIONS;
    };

    // The backup file's text. `createdAt` (ISO 8601 UTC) is left out when
    // empty; without `seal` the file holds no secret at all (FR-003).
    std::string makeBackup(const Config &cfg, const DeviceInfo &device, const std::string &createdAt, const SealParams *seal);

    // The encrypted secrets block, decoded.
    struct SecretsBlock
    {
        uint32_t iterations = 0;
        std::string salt;
        std::string nonce;
        std::string sealed; // ciphertext + tag
    };

    // A backup file that read correctly.
    struct Parsed
    {
        int format = 0; // 0: a plain settings export (no header)
        std::string firmware;
        std::string board;
        std::string build;
        std::string deviceName;
        std::string createdAt;
        std::string settings; // the settings object as JSON text
        bool hasSecrets = false;
        SecretsBlock secrets;
    };

    // Reads a backup file's text.
    Error parseBackup(const std::string &text, Parsed &out);

    // Secret path ("wifi.password") and value pairs.
    using Secrets = std::vector<std::pair<std::string, std::string>>;
    // The nine secret paths, in file order.
    const std::vector<const char *> &secretPaths();

    // Derives the key and decrypts the secrets (the slow step).
    Error openSecrets(const Parsed &parsed, const std::string &passphrase, Secrets &out, uint32_t *deriveIterations = nullptr);

    // POST /api/backup body: {"passphrase": "..."} (or empty).
    Error readBackupRequest(const std::string &body, std::string &passphrase);
    // POST /api/backup/preview body: {"backup": "<file text>" | {...}, "passphrase": "..."}.
    Error readPreviewRequest(const std::string &body, Parsed &parsed, std::string &passphrase);

    // Settings that need a restart to take effect, as bits (web/src/components/settings/restart.ts).
    constexpr uint8_t RESTART_WIFI = 1U << 0;
    constexpr uint8_t RESTART_OTA = 1U << 1;
    constexpr uint8_t RESTART_GPS = 1U << 2;
    constexpr uint8_t RESTART_I2C = 1U << 3;
    constexpr uint8_t RESTART_IPV6 = 1U << 4;
    constexpr uint8_t RESTART_ALPACA = 1U << 5;
    constexpr uint8_t RESTART_BLE = 1U << 6;
    constexpr int RESTART_REASON_COUNT = 7;
    uint8_t restartReasons(const Config &before, const Config &after);
    // "wifi", "ota", "gps", "i2c", "ipv6", "alpaca", "ble" for bit `index`.
    const char *restartCode(int index);

    // -1, 0 or 1 comparing "major.minor.patch[-pre.N][+build]" versions.
    int compareVersions(const std::string &a, const std::string &b);

    // One setting that will change; values as JSON text.
    struct Change
    {
        std::string path;
        std::string from;
        std::string to;
    };

    enum class NotApplicableReason : uint8_t
    {
        Pin,          // not a usable GPIO on this board
        Analog,       // not an analog (ADC1) pin on this board
        Bluetooth,    // needs the Bluetooth firmware build
        PushoverKeys, // Pushover on, but its keys are neither restored nor set here
    };
    const char *reasonCode(NotApplicableReason reason);

    struct NotApplicable
    {
        std::string path;
        std::string value; // JSON text, empty for a secret
        NotApplicableReason reason;
    };

    enum class SecretsState : uint8_t
    {
        Restored, // decrypted with the right passphrase
        Kept,     // the file has secrets, no passphrase given: this device's stay
        None,     // the file has none: this device's stay
    };

    enum class Warning : uint8_t
    {
        WifiNetwork,    // joins another network
        DevicePassword, // the next page load asks for the backup's password
        Address,        // the local name changes
        ImagingApp,     // a restart interrupts a connected imaging app
        NewerFirmware,  // made by newer firmware
    };

    // Everything the owner sees before confirming, and what confirming saves.
    struct Preview
    {
        Config candidate;
        std::vector<Change> changes;
        SecretsState secretsState = SecretsState::None;
        std::vector<std::string> secretsChanged;
        std::vector<std::string> secretsUnchanged;
        std::vector<std::string> kept;
        std::vector<std::string> ignored;
        std::vector<NotApplicable> notApplicable;
        uint8_t restart = 0;
        std::vector<Warning> warnings;
    };

    // The facts a preview needs besides the two configs.
    struct PreviewInput
    {
        const Config *current = nullptr;
        const Parsed *parsed = nullptr;
        const Secrets *secrets = nullptr; // nullptr: not restored
        DeviceInfo device;
        bool imagingAppConnected = false;
    };

    // Builds the candidate (validated like any save) and the preview.
    // Error::Invalid puts the save's reason in `error`.
    Error buildPreview(const PreviewInput &input, Preview &out, std::string *error);

    // The preview document (POST /api/backup/preview).
    std::string previewJson(const Preview &preview, const Parsed &parsed, const std::string &token);

    // {"error": text, "code": code}.
    std::string errorJson(Error error, const std::string &detail = std::string());

    // Lower-case hex of `bytes`.
    std::string toHex(const uint8_t *bytes, size_t size);
} // namespace SQM::Backup
