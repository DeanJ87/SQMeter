#include "BackupInternal.h"
#include "ConfigBackup.h"

#include <ArduinoJson.h>

#include <cstring>
#include <iterator>
#include <string>

// The backup file: making it, reading it and opening its secrets block
// (specs/033-config-backup/contracts/backup-file.md).

namespace SQM::Backup
{
    using namespace Detail;

    namespace
    {
        constexpr const char *KDF = "pbkdf2-sha256";
        constexpr const char *CIPHER = "aes-256-gcm";
        constexpr const char *B64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

        std::string base64Encode(const std::string &in)
        {
            std::string out;
            out.reserve((in.size() + 2) / 3 * 4);
            for (size_t i = 0; i < in.size(); i += 3)
            {
                const size_t left = in.size() - i;
                uint32_t n = static_cast<uint8_t>(in[i]) << 16;
                if (left > 1)
                    n |= static_cast<uint8_t>(in[i + 1]) << 8;
                if (left > 2)
                    n |= static_cast<uint8_t>(in[i + 2]);
                out += B64[(n >> 18) & 63];
                out += B64[(n >> 12) & 63];
                out += left > 1 ? B64[(n >> 6) & 63] : '=';
                out += left > 2 ? B64[n & 63] : '=';
            }
            return out;
        }

        int base64Value(char c)
        {
            const char *at = c != '\0' ? std::strchr(B64, c) : nullptr;
            return at ? static_cast<int>(at - B64) : -1;
        }

        // Strict: padded, nothing but the alphabet.
        bool base64Decode(const std::string &in, std::string &out)
        {
            out.clear();
            if (in.size() % 4 != 0)
                return false;
            for (size_t i = 0; i < in.size(); i += 4)
            {
                const bool last = i + 4 == in.size();
                const int pad = last ? (in[i + 3] == '=') + (in[i + 2] == '=') : 0;
                uint32_t n = 0;
                for (size_t j = 0; j < 4; ++j)
                {
                    const int v = j >= 4 - static_cast<size_t>(pad) ? 0 : base64Value(in[i + j]);
                    if (v < 0)
                        return false;
                    n = (n << 6) | static_cast<uint32_t>(v);
                }
                out += static_cast<char>(n >> 16);
                if (pad < 2)
                    out += static_cast<char>((n >> 8) & 0xff);
                if (pad < 1)
                    out += static_cast<char>(n & 0xff);
            }
            return true;
        }

        std::string bytes(const uint8_t *data, size_t size) { return std::string(reinterpret_cast<const char *>(data), size); }

        std::string secretsPlaintext(const Config &cfg)
        {
            DynamicJsonDocument doc(1024 + SECRET_COUNT * 160);
            for (size_t i = 0; i < SECRET_COUNT; ++i)
                doc[SECRET_PATHS[i]] = secretRef(cfg, i).c_str();
            std::string out;
            serializeJson(doc, out);
            return out;
        }

        // Writes `secrets` into `target`; false if the key couldn't be made.
        bool writeSecrets(JsonObject target, const Config &cfg, const SealParams &seal, std::string &data)
        {
            uint8_t key[KEY_BYTES];
            if (!deriveKey(seal.passphrase, seal.salt, SALT_BYTES, seal.iterations, key))
                return false;
            std::string sealed;
            const bool ok = Backup::seal(key, seal.nonce, AAD, secretsPlaintext(cfg), sealed);
            volatile uint8_t *wipe = key;
            for (size_t i = 0; i < KEY_BYTES; ++i)
                wipe[i] = 0;
            if (!ok)
                return false;
            data = base64Encode(sealed);
            target["kdf"] = KDF;
            target["iterations"] = seal.iterations;
            target["salt"] = base64Encode(bytes(seal.salt, SALT_BYTES));
            target["cipher"] = CIPHER;
            target["nonce"] = base64Encode(bytes(seal.nonce, NONCE_BYTES));
            target["data"] = data.c_str(); // referenced: `data` outlives the document
            return true;
        }

        void takeString(JsonObjectConst obj, const char *key, std::string &target)
        {
            if (obj[key].is<const char *>())
                target = obj[key].as<const char *>();
        }

        bool decodeExact(JsonVariantConst value, size_t size, std::string &out)
        {
            return value.is<const char *>() && base64Decode(value.as<const char *>(), out) && (size == 0 || out.size() == size);
        }

        Error readSecretsBlock(JsonVariantConst value, Parsed &out)
        {
            if (!value.is<JsonObjectConst>())
                return Error::Damaged;
            JsonObjectConst block = value.as<JsonObjectConst>();
            SecretsBlock &secrets = out.secrets;
            const bool known = block["kdf"] == KDF && block["cipher"] == CIPHER && block["iterations"].is<uint32_t>();
            if (!known)
                return Error::Damaged;
            secrets.iterations = block["iterations"].as<uint32_t>();
            if (secrets.iterations < MIN_ITERATIONS || secrets.iterations > MAX_ITERATIONS)
                return Error::Damaged;
            if (!decodeExact(block["salt"], SALT_BYTES, secrets.salt) || !decodeExact(block["nonce"], NONCE_BYTES, secrets.nonce) ||
                !decodeExact(block["data"], 0, secrets.sealed) || secrets.sealed.size() < TAG_BYTES)
                return Error::Damaged;
            out.hasSecrets = true;
            return Error::None;
        }

        Error parseObject(JsonObjectConst obj, Parsed &out)
        {
            out = Parsed{};
            JsonVariantConst version = obj["sqmeterBackup"];
            if (version.isNull())
            {
                // Format 0: a plain GET /api/config export, the documented backup before this one.
                if (!obj["wifi"].is<JsonObjectConst>())
                    return Error::NotBackup;
                serializeJson(obj, out.settings);
                return Error::None;
            }
            if (!version.is<uint32_t>() || version.as<uint32_t>() < 1)
                return Error::Damaged;
            if (version.as<uint32_t>() > static_cast<uint32_t>(FORMAT_VERSION))
                return Error::NewerFormat;
            if (!obj["settings"].is<JsonObjectConst>())
                return Error::Damaged;
            out.format = static_cast<int>(version.as<uint32_t>());
            takeString(obj, "firmware", out.firmware);
            takeString(obj, "board", out.board);
            takeString(obj, "build", out.build);
            takeString(obj, "deviceName", out.deviceName);
            takeString(obj, "createdAt", out.createdAt);
            serializeJson(obj["settings"], out.settings);
            return obj["secrets"].isNull() ? Error::None : readSecretsBlock(obj["secrets"], out);
        }

        // Characters as the web UI counts them: UTF-8 code points.
        size_t characters(const std::string &text)
        {
            size_t count = 0;
            for (char c : text)
                count += (static_cast<uint8_t>(c) & 0xC0) != 0x80;
            return count;
        }

        bool isBlank(const std::string &text) { return text.find_first_not_of(" \t\r\n") == std::string::npos; }
    } // namespace

    namespace Detail
    {
        bool isSecretPath(const std::string &path)
        {
            for (const char *secret : SECRET_PATHS)
                if (path == secret)
                    return true;
            return false;
        }

        size_t jsonCapacity(const std::string &text, size_t extra)
        {
            size_t values = 8;
            for (char c : text)
                values += c == ':' || c == ',' || c == '[' || c == '{';
            return JSON_OBJECT_SIZE(values) + text.size() + extra; // + the strings, copied
        }

        bool parseCopy(const std::string &text, JsonDocument &doc) { return !deserializeJson(doc, text); }

        std::string jsonText(JsonVariantConst value)
        {
            std::string out;
            serializeJson(value, out);
            return out;
        }
    } // namespace Detail

    const std::vector<const char *> &secretPaths()
    {
        static const std::vector<const char *> PATHS(std::begin(SECRET_PATHS), std::end(SECRET_PATHS));
        return PATHS;
    }

    std::string makeBackup(const Config &cfg, const DeviceInfo &device, const std::string &createdAt, const SealParams *seal)
    {
        const std::string settings = cfg.toJson(true);
        DynamicJsonDocument doc(settings.size() + 2048);
        doc["sqmeterBackup"] = FORMAT_VERSION;
        doc["firmware"] = device.firmware.c_str();
        doc["board"] = device.board.c_str();
        doc["build"] = device.bluetoothBuild ? "ble" : "standard";
        doc["deviceName"] = cfg.deviceName.c_str();
        if (!createdAt.empty())
            doc["createdAt"] = createdAt.c_str();
        doc["settings"] = serialized(settings.c_str(), settings.size()); // referenced: `settings` outlives the document
        std::string data;
        if (seal && !writeSecrets(doc.createNestedObject("secrets"), cfg, *seal, data))
            return std::string();
        std::string out;
        serializeJson(doc, out);
        return out;
    }

    Error parseBackup(const std::string &text, Parsed &out)
    {
        if (text.size() > MAX_BODY_BYTES)
            return Error::TooLarge;
        DynamicJsonDocument doc(jsonCapacity(text, 0));
        if (!parseCopy(text, doc))
            return text.find("\"sqmeterBackup\"") != std::string::npos ? Error::Damaged : Error::NotBackup;
        if (!doc.is<JsonObject>())
            return Error::NotBackup;
        return parseObject(doc.as<JsonObjectConst>(), out);
    }

    Error openSecrets(const Parsed &parsed, const std::string &passphrase, Secrets &out, uint32_t *deriveIterations)
    {
        out.clear();
        if (!parsed.hasSecrets)
            return Error::None;
        uint8_t key[KEY_BYTES];
        uint8_t nonce[NONCE_BYTES];
        std::memcpy(nonce, parsed.secrets.nonce.data(), NONCE_BYTES);
        const auto *salt = reinterpret_cast<const uint8_t *>(parsed.secrets.salt.data());
        if (!deriveKey(passphrase, salt, parsed.secrets.salt.size(), parsed.secrets.iterations, key))
            return Error::Damaged;
        if (deriveIterations)
            *deriveIterations = parsed.secrets.iterations;
        std::string decrypted;
        const bool opened = open(key, nonce, AAD, parsed.secrets.sealed, decrypted);
        const std::string &plain = decrypted; // const: the same JSON reader as Config::applyJson
        volatile uint8_t *wipe = key;
        for (size_t i = 0; i < KEY_BYTES; ++i)
            wipe[i] = 0;
        if (!opened)
            return Error::WrongPassphrase;
        DynamicJsonDocument doc(jsonCapacity(plain, 0));
        if (!parseCopy(plain, doc) || !doc.is<JsonObject>())
            return Error::Damaged;
        for (const char *path : SECRET_PATHS)
            if (doc[path].is<const char *>())
                out.emplace_back(path, doc[path].as<const char *>());
        return Error::None;
    }

    Error readBackupRequest(const std::string &body, std::string &passphrase)
    {
        passphrase.clear();
        if (body.size() > MAX_BODY_BYTES)
            return Error::TooLarge;
        if (isBlank(body))
            return Error::None;
        DynamicJsonDocument doc(jsonCapacity(body, 0));
        if (!parseCopy(body, doc) || !doc.is<JsonObject>())
            return Error::BadRequest;
        if (doc["passphrase"].is<const char *>())
            passphrase = doc["passphrase"].as<const char *>();
        return passphrase.empty() || characters(passphrase) >= MIN_PASSPHRASE_CHARS ? Error::None : Error::PassphraseShort;
    }

    std::string readApplyRequest(const std::string &body)
    {
        if (body.size() > 256)
            return std::string();
        DynamicJsonDocument doc(jsonCapacity(body, 0));
        return parseCopy(body, doc) && doc["token"].is<const char *>() ? std::string(doc["token"].as<const char *>()) : std::string();
    }

    Error readPreviewRequest(const std::string &body, Parsed &parsed, std::string &passphrase)
    {
        passphrase.clear();
        if (body.size() > MAX_BODY_BYTES)
            return Error::TooLarge;
        std::string text;
        {
            DynamicJsonDocument doc(jsonCapacity(body, 0));
            if (!parseCopy(body, doc) || !doc.is<JsonObject>())
                return Error::BadRequest;
            if (doc["passphrase"].is<const char *>())
                passphrase = doc["passphrase"].as<const char *>();
            JsonVariantConst backup = doc["backup"];
            if (backup.is<JsonObjectConst>())
                return parseObject(backup.as<JsonObjectConst>(), parsed);
            if (!backup.is<const char *>())
                return Error::NotBackup;
            text = backup.as<const char *>(); // the file as uploaded: parsed below, after this document is freed
        }
        return parseBackup(text, parsed);
    }
} // namespace SQM::Backup
