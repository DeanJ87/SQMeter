#include "BackupInternal.h"
#include "ConfigBackup.h"

#include <ArduinoJson.h>

#include <utility>

// The restore preview: the candidate config a restore would save, built and
// validated exactly as a settings save (specs/033-config-backup FR-007,
// FR-009, FR-011, FR-012). The device decides what is applied; the web UI
// lists the differences from the candidate's masked settings.

namespace SQM::Backup
{
    using namespace Detail;

    namespace
    {
        struct PinSetting
        {
            const char *section;
            const char *key;
            bool analog;
        };
        constexpr PinSetting PIN_SETTINGS[] = {
            {"gps", "rxPin", false},
            {"gps", "txPin", false},
            {"rain", "rxPin", false},
            {"rain", "txPin", false},
            {"sensor", "i2cSDA", false},
            {"sensor", "i2cSCL", false},
            {"wind", "speedPin", false},
            {"wind", "directionPin", true},
        };

        // Pins this board doesn't have are removed from the backup document,
        // so the restore keeps this board's value.
        void markPins(JsonObject settings, const BoardPins &pins, std::vector<NotApplicable> &out)
        {
            for (const PinSetting &setting : PIN_SETTINGS)
            {
                JsonObject section = settings[setting.section].as<JsonObject>();
                JsonVariant value = section[setting.key];
                if (!value.is<uint8_t>())
                    continue; // missing (kept) or the wrong type (the save refuses it)
                const int pin = value.as<uint8_t>();
                if (setting.analog ? pins.analog(pin) : pins.valid(pin))
                    continue;
                out.push_back({std::string(setting.section) + "." + setting.key, jsonText(value),
                               setting.analog ? NotApplicableReason::Analog : NotApplicableReason::Pin});
                section.remove(setting.key);
            }
        }

        const std::string *findSecret(const Secrets *secrets, const char *path)
        {
            if (secrets)
                for (const auto &secret : *secrets)
                    if (secret.first == path)
                        return &secret.second;
            return nullptr;
        }

        // Bluetooth settings on the standard build: never applied; listed when they'd differ.
        void markBluetooth(JsonObject settings, const PreviewInput &in, const Config &current, std::vector<NotApplicable> &out)
        {
            JsonVariant ble = settings["ble"];
            if (in.device.bluetoothBuild || !ble.is<JsonObject>())
                return;
            JsonVariant enabled = ble["enabled"];
            if (enabled.is<bool>() && enabled.as<bool>() != current.ble.enabled)
                out.push_back({"ble.enabled", jsonText(enabled), NotApplicableReason::Bluetooth});
            const std::string *passkey = findSecret(in.secrets, SECRET_PATHS[BLE_PASSKEY]);
            if (passkey && *passkey != current.ble.passkey)
                out.push_back({SECRET_PATHS[BLE_PASSKEY], std::string(), NotApplicableReason::Bluetooth});
            settings.remove("ble");
        }

        // Pushover on in the backup, but no keys to send with: the save would be refused.
        void markPushover(JsonObject settings, const PreviewInput &in, const Config &current, std::vector<NotApplicable> &out)
        {
            if (in.secrets || (!current.alerts.pushoverUserKey.empty() && !current.alerts.pushoverAppToken.empty()))
                return;
            JsonObject pushover = settings["alerts"]["pushover"].as<JsonObject>();
            if (!pushover["enabled"].is<bool>() || !pushover["enabled"].as<bool>())
                return;
            out.push_back({"alerts.pushover.enabled", "true", NotApplicableReason::PushoverKeys});
            pushover.remove("enabled");
        }

        // Where a secret goes in the settings document, made if missing.
        JsonVariant secretSlot(JsonObject settings, const std::string &path)
        {
            JsonObject node = settings;
            size_t start = 0;
            for (size_t dot = path.find('.'); dot != std::string::npos; start = dot + 1, dot = path.find('.', start))
            {
                const std::string key = path.substr(start, dot - start);
                JsonObject child = node[key].as<JsonObject>();
                node = child.isNull() ? node.createNestedObject(key) : child;
            }
            const std::string key = path.substr(start);
            if (!node.containsKey(key))
                node[key] = nullptr; // a member must exist before a variant can point at it
            return node[key];
        }

        // The decrypted secrets replace the masks; an empty one clears the setting.
        void injectSecrets(JsonObject settings, const Secrets &secrets, bool bluetoothBuild)
        {
            for (const auto &secret : secrets)
            {
                if (!bluetoothBuild && secret.first == SECRET_PATHS[BLE_PASSKEY])
                    continue;
                JsonVariant target = secretSlot(settings, secret.first);
                if (secret.second.empty())
                    target.set(nullptr);
                else
                    target.set(secret.second);
            }
        }

        void secretStatus(const PreviewInput &in, const Config &current, Preview &out)
        {
            if (!in.secrets)
                return;
            for (size_t i = 0; i < SECRET_COUNT; ++i)
            {
                if (i == BLE_PASSKEY && !in.device.bluetoothBuild)
                    continue;
                const bool same = secretRef(current, i) == secretRef(out.candidate, i);
                (same ? out.secretsUnchanged : out.secretsChanged) |= static_cast<uint16_t>(1U << i);
            }
        }

        // The backup's settings as they will be applied: what can't apply removed, secrets in.
        Error prepareSettings(const PreviewInput &in, const Config &current, Preview &out, std::string &applyText)
        {
            DynamicJsonDocument doc(jsonCapacity(in.parsed->settings, 2048)); // + the secrets put in
            if (!parseCopy(in.parsed->settings, doc) || !doc.is<JsonObject>())
                return Error::Damaged;
            JsonObject settings = doc.as<JsonObject>();
            markPins(settings, in.device.pins, out.notApplicable);
            markBluetooth(settings, in, current, out.notApplicable);
            markPushover(settings, in, current, out.notApplicable);
            if (in.secrets)
                injectSecrets(settings, *in.secrets, in.device.bluetoothBuild);
            serializeJson(settings, applyText);
            return Error::None;
        }

        const char *secretsStateName(SecretsState state)
        {
            switch (state)
            {
            case SecretsState::Restored:
                return "restored";
            case SecretsState::Kept:
                return "kept";
            case SecretsState::None:
                break;
            }
            return "none";
        }

        void writeHeader(JsonObject target, const Parsed &parsed)
        {
            target["format"] = parsed.format;
            const std::pair<const char *, const std::string *> fields[] = {
                {"firmware", &parsed.firmware},
                {"board", &parsed.board},
                {"build", &parsed.build},
                {"deviceName", &parsed.deviceName},
                {"createdAt", &parsed.createdAt},
            };
            for (const auto &field : fields)
                if (!field.second->empty())
                    target[field.first] = field.second->c_str();
        }

        void writeSecretList(JsonArray target, uint16_t bits)
        {
            for (size_t i = 0; i < SECRET_COUNT; ++i)
                if ((bits >> i) & 1U)
                    target.add(SECRET_PATHS[i]);
        }
    } // namespace

    Error buildPreview(const PreviewInput &in, Preview &out, std::string *error)
    {
        const Config &current = *in.current;
        out.secretsState = in.secrets ? SecretsState::Restored : (in.parsed->hasSecrets ? SecretsState::Kept : SecretsState::None);
        std::string applyText;
        const Error prepared = prepareSettings(in, current, out, applyText);
        if (prepared != Error::None)
            return prepared;
        // The same rules as POST /api/config: masks keep secrets, types are strict, validate() runs.
        if (!Config::applyJson(applyText, out.candidate, true, error))
            return Error::Invalid;
        secretStatus(in, current, out);
        out.restart = restartReasons(current, out.candidate);
        return Error::None;
    }

    std::string previewJson(const Preview &preview, const Parsed &parsed, const std::string &token)
    {
        const std::string settings = preview.candidate.toJson(true);
        DynamicJsonDocument doc(1536 + 96 * preview.notApplicable.size());
        doc["token"] = token.c_str();
        writeHeader(doc.createNestedObject("backup"), parsed);
        doc["settings"] = serialized(settings.c_str(), settings.size()); // referenced: `settings` outlives the document
        JsonObject secrets = doc.createNestedObject("secrets");
        secrets["state"] = secretsStateName(preview.secretsState);
        writeSecretList(secrets.createNestedArray("changed"), preview.secretsChanged);
        writeSecretList(secrets.createNestedArray("unchanged"), preview.secretsUnchanged);
        JsonArray notApplicable = doc.createNestedArray("notApplicable");
        for (const NotApplicable &n : preview.notApplicable)
        {
            JsonObject item = notApplicable.createNestedObject();
            item["path"] = n.path.c_str();
            if (!n.value.empty())
                item["value"] = serialized(n.value.c_str(), n.value.size());
            item["reason"] = reasonCode(n.reason);
        }
        JsonArray restart = doc.createNestedArray("restart");
        for (int i = 0; i < RESTART_REASON_COUNT; ++i)
            if (preview.restart & (1U << i))
                restart.add(restartCode(i));
        std::string out;
        serializeJson(doc, out);
        return out;
    }
} // namespace SQM::Backup
