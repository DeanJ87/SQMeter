#include "ConfigBackup.h"

#include <ArduinoJson.h>

#include <cstdlib>
#include <tuple>

// The small rules behind a backup: refusal codes and texts, the board's pins,
// which changes need a restart, and version order.

namespace SQM::Backup
{
    const char *errorCode(Error error)
    {
        switch (error)
        {
        case Error::None:
            return "";
        case Error::TooLarge:
            return "too-large";
        case Error::NotBackup:
            return "not-backup";
        case Error::Damaged:
            return "damaged";
        case Error::NewerFormat:
            return "newer-format";
        case Error::WrongPassphrase:
            return "wrong-passphrase";
        case Error::PassphraseShort:
            return "passphrase-short";
        case Error::BadRequest:
            return "bad-request";
        case Error::Invalid:
            break;
        }
        return "invalid";
    }

    // Device text (specs/023-i18n): the web UI shows these translated.
    const char *errorText(Error error)
    {
        switch (error)
        {
        case Error::None:
            return "";
        case Error::TooLarge:
            return "The backup file is too large";
        case Error::NotBackup:
            return "This isn't a SQMeter backup file";
        case Error::Damaged:
            return "The backup file is damaged";
        case Error::NewerFormat:
            return "This backup is from newer firmware; update this device first";
        case Error::WrongPassphrase:
            return "Wrong passphrase, or the file's secrets were changed";
        case Error::PassphraseShort:
            return "The passphrase needs at least 8 characters";
        case Error::BadRequest:
            return "The request isn't valid JSON";
        case Error::Invalid:
            break;
        }
        return "The backup's settings aren't valid";
    }

    std::string errorJson(Error error, const std::string &detail)
    {
        StaticJsonDocument<256> doc;
        doc["error"] = detail.empty() ? std::string(errorText(error)) : detail;
        doc["code"] = errorCode(error);
        std::string out;
        serializeJson(doc, out);
        return out;
    }

    BoardPins classicEsp32Pins()
    {
        BoardPins pins;
        for (int pin : {0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39})
            pins.validPins |= 1ULL << pin;
        for (int pin = 32; pin <= 39; ++pin)
            pins.analogPins |= 1ULL << pin;
        return pins;
    }

    namespace
    {
        bool wifiChanged(const WiFiConfig &a, const WiFiConfig &b)
        {
            return std::tie(a.ssid, a.password, a.hostname, a.mdns, a.autoReconnect, a.reconnectDelayMs, a.maxReconnectDelayMs) !=
                   std::tie(b.ssid, b.password, b.hostname, b.mdns, b.autoReconnect, b.reconnectDelayMs, b.maxReconnectDelayMs);
        }

        bool gpsChanged(const GPSConfig &a, const GPSConfig &b)
        {
            return std::tie(a.enabled, a.rxPin, a.txPin, a.baudRate) != std::tie(b.enabled, b.rxPin, b.txPin, b.baudRate);
        }

        bool i2cChanged(const SensorConfig &a, const SensorConfig &b)
        {
            return std::tie(a.i2cSDA, a.i2cSCL, a.i2cFrequency) != std::tie(b.i2cSDA, b.i2cSCL, b.i2cFrequency);
        }

        bool bleChanged(const Config &before, const Config &after)
        {
            return before.ble.enabled != after.ble.enabled || before.ble.passkey != after.ble.passkey ||
                   (after.ble.enabled && before.deviceName != after.deviceName);
        }
    } // namespace

    // The same rules as restart.ts: settings the firmware only reads at boot.
    uint8_t restartReasons(const Config &before, const Config &after)
    {
        uint8_t reasons = 0;
        if (wifiChanged(before.wifi, after.wifi))
            reasons |= RESTART_WIFI;
        if (after.ota.enabled && (before.ota.enabled != after.ota.enabled || before.ota.password != after.ota.password))
            reasons |= RESTART_OTA;
        if (gpsChanged(before.gps, after.gps))
            reasons |= RESTART_GPS;
        if (i2cChanged(before.sensor, after.sensor))
            reasons |= RESTART_I2C;
        if (before.wifi.ipv6 != after.wifi.ipv6)
            reasons |= RESTART_IPV6;
        if (before.alpaca.enabled != after.alpaca.enabled)
            reasons |= RESTART_ALPACA;
        if (bleChanged(before, after))
            reasons |= RESTART_BLE;
        return reasons;
    }

    const char *restartCode(int index)
    {
        static const char *const CODES[RESTART_REASON_COUNT] = {"wifi", "ota", "gps", "i2c", "ipv6", "alpaca", "ble"};
        return index >= 0 && index < RESTART_REASON_COUNT ? CODES[index] : "";
    }

    const char *reasonCode(NotApplicableReason reason)
    {
        switch (reason)
        {
        case NotApplicableReason::Pin:
            return "pin";
        case NotApplicableReason::Analog:
            return "analog";
        case NotApplicableReason::Bluetooth:
            return "bluetooth";
        case NotApplicableReason::PushoverKeys:
            break;
        }
        return "pushover-keys";
    }

    namespace
    {
        struct Version
        {
            long part[3] = {0, 0, 0};
            bool release = true; // no pre-release suffix
            long pre = 0;        // the pre-release's trailing number ("beta.4" -> 4)
        };

        Version parseVersion(const std::string &text)
        {
            Version v;
            const char *p = text.c_str();
            if (*p == 'v')
                ++p;
            for (long &part : v.part)
            {
                char *end = nullptr;
                part = std::strtol(p, &end, 10);
                p = end;
                if (*p != '.')
                    break;
                ++p;
            }
            if (*p == '-')
            {
                v.release = false;
                std::string pre(p + 1);
                pre = pre.substr(0, pre.find('+'));
                const size_t dot = pre.find_last_of('.');
                v.pre = std::strtol(pre.c_str() + (dot == std::string::npos ? 0 : dot + 1), nullptr, 10);
            }
            return v;
        }
    } // namespace

    int compareVersions(const std::string &a, const std::string &b)
    {
        const Version x = parseVersion(a);
        const Version y = parseVersion(b);
        for (size_t i = 0; i < 3; ++i)
            if (x.part[i] != y.part[i])
                return x.part[i] < y.part[i] ? -1 : 1;
        if (x.release != y.release)
            return x.release ? 1 : -1;
        if (x.pre != y.pre)
            return x.pre < y.pre ? -1 : 1;
        return 0;
    }

    std::string toHex(const uint8_t *bytes, size_t size)
    {
        static const char DIGITS[] = "0123456789abcdef";
        std::string out;
        out.reserve(size * 2);
        for (size_t i = 0; i < size; ++i)
        {
            out += DIGITS[bytes[i] >> 4];
            out += DIGITS[bytes[i] & 0x0f];
        }
        return out;
    }
} // namespace SQM::Backup
