#include "BackupInternal.h"
#include "ConfigBackup.h"

#include <ArduinoJson.h>

#include <utility>

// The restore preview: the candidate config a restore would save, built and
// validated exactly as a settings save, and what it changes
// (specs/033-config-backup FR-007, FR-008, FR-011, FR-012).

namespace SQM::Backup
{
    using namespace Detail;

    namespace
    {
        // Derived from alerts.sendMode and written only for older firmware.
        constexpr const char *DERIVED_PATH = "alerts.armWithAlpaca";

        bool reported(const std::string &path) { return path != DERIVED_PATH && !isSecretPath(path); }

        std::string join(const std::string &base, const char *key) { return base.empty() ? std::string(key) : base + "." + key; }

        // Paths in `have` that `other` lacks; a whole missing object is one path.
        void listMissing(JsonObjectConst have, JsonObjectConst other, const std::string &base, std::vector<std::string> &out)
        {
            for (JsonPairConst pair : have)
            {
                const std::string path = join(base, pair.key().c_str());
                if (!reported(path))
                    continue;
                if (!other.containsKey(pair.key()))
                    out.push_back(path);
                else if (pair.value().is<JsonObjectConst>() && other[pair.key()].is<JsonObjectConst>())
                    listMissing(pair.value().as<JsonObjectConst>(), other[pair.key()].as<JsonObjectConst>(), path, out);
            }
        }

        // Every reported setting whose value differs between the two documents.
        void listChanges(JsonObjectConst before, JsonObjectConst after, const std::string &base, std::vector<Change> &out)
        {
            for (JsonPairConst pair : before)
            {
                const std::string path = join(base, pair.key().c_str());
                JsonVariantConst next = after[pair.key()];
                if (!reported(path) || next.isNull())
                    continue;
                if (pair.value().is<JsonObjectConst>() && next.is<JsonObjectConst>())
                {
                    listChanges(pair.value().as<JsonObjectConst>(), next.as<JsonObjectConst>(), path, out);
                    continue;
                }
                std::string from = jsonText(pair.value());
                std::string to = jsonText(next);
                if (from != to)
                    out.push_back({path, std::move(from), std::move(to)});
            }
        }

        struct PinSetting
        {
            const char *path;
            bool analog;
        };
        constexpr PinSetting PIN_SETTINGS[] = {
            {"gps.rxPin", false},
            {"gps.txPin", false},
            {"rain.rxPin", false},
            {"rain.txPin", false},
            {"sensor.i2cSDA", false},
            {"sensor.i2cSCL", false},
            {"wind.speedPin", false},
            {"wind.directionPin", true},
        };

        // Removes a setting from the backup document, so the restore keeps this device's value.
        void dropSetting(JsonObject settings, const std::string &path)
        {
            const size_t dot = path.rfind('.');
            JsonVariant parent = dot == std::string::npos ? JsonVariant(settings) : atPath(settings, path.substr(0, dot), false);
            if (parent.is<JsonObject>())
                parent.as<JsonObject>().remove(path.substr(dot + 1));
        }

        void markPins(JsonObject settings, const BoardPins &pins, std::vector<NotApplicable> &out)
        {
            for (const PinSetting &setting : PIN_SETTINGS)
            {
                JsonVariant value = atPath(settings, setting.path, false);
                if (!value.is<int>())
                    continue; // missing (kept) or the wrong type (the save refuses it)
                const int pin = value.as<int>();
                if (setting.analog ? pins.analog(pin) : pins.valid(pin))
                    continue;
                out.push_back({setting.path, jsonText(value), setting.analog ? NotApplicableReason::Analog : NotApplicableReason::Pin});
                dropSetting(settings, setting.path);
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
        void markBluetooth(JsonObject settings, const PreviewInput &in, std::vector<NotApplicable> &out)
        {
            JsonVariant ble = settings["ble"];
            if (in.device.bluetoothBuild || !ble.is<JsonObject>())
                return;
            JsonVariant enabled = ble["enabled"];
            if (enabled.is<bool>() && enabled.as<bool>() != in.current->ble.enabled)
                out.push_back({"ble.enabled", jsonText(enabled), NotApplicableReason::Bluetooth});
            const std::string *passkey = findSecret(in.secrets, SECRET_PATHS[BLE_PASSKEY]);
            if (passkey && *passkey != in.current->ble.passkey)
                out.push_back({SECRET_PATHS[BLE_PASSKEY], std::string(), NotApplicableReason::Bluetooth});
            settings.remove("ble");
        }

        // Pushover on in the backup, but no keys to send with: the save would be refused.
        void markPushover(JsonObject settings, const PreviewInput &in, std::vector<NotApplicable> &out)
        {
            if (in.secrets || (!in.current->alerts.pushoverUserKey.empty() && !in.current->alerts.pushoverAppToken.empty()))
                return;
            JsonVariant enabled = atPath(settings, "alerts.pushover.enabled", false);
            if (!enabled.is<bool>() || !enabled.as<bool>())
                return;
            out.push_back({"alerts.pushover.enabled", "true", NotApplicableReason::PushoverKeys});
            dropSetting(settings, "alerts.pushover.enabled");
        }

        // The decrypted secrets replace the masks; an empty one clears the setting.
        void injectSecrets(JsonObject settings, const Secrets &secrets, bool bluetoothBuild)
        {
            for (const auto &secret : secrets)
            {
                if (!bluetoothBuild && secret.first == SECRET_PATHS[BLE_PASSKEY])
                    continue;
                JsonVariant target = atPath(settings, secret.first, true);
                if (secret.second.empty())
                    target.set(nullptr);
                else
                    target.set(secret.second);
            }
        }

        void secretStatus(const PreviewInput &in, Preview &out)
        {
            if (!in.secrets)
                return;
            for (size_t i = 0; i < SECRET_COUNT; ++i)
            {
                if (i == BLE_PASSKEY && !in.device.bluetoothBuild)
                    continue;
                const bool same = secretRef(*in.current, i) == secretRef(out.candidate, i);
                (same ? out.secretsUnchanged : out.secretsChanged).emplace_back(SECRET_PATHS[i]);
            }
        }

        void addWarnings(const PreviewInput &in, Preview &out)
        {
            const Config &before = *in.current;
            const Config &after = out.candidate;
            if (before.wifi.ssid != after.wifi.ssid)
                out.warnings.push_back(Warning::WifiNetwork);
            const bool passwordOn = after.auth.enabled && !after.auth.password.empty();
            const bool passwordNew = !before.auth.enabled || before.auth.password.empty() || before.auth.username != after.auth.username ||
                                     before.auth.password != after.auth.password;
            if (passwordOn && passwordNew)
                out.warnings.push_back(Warning::DevicePassword);
            if (before.wifi.hostname != after.wifi.hostname || before.wifi.mdns != after.wifi.mdns)
                out.warnings.push_back(Warning::Address);
            if (out.restart != 0 && in.imagingAppConnected)
                out.warnings.push_back(Warning::ImagingApp);
            if (!in.parsed->firmware.empty() && compareVersions(in.parsed->firmware, in.device.firmware) > 0)
                out.warnings.push_back(Warning::NewerFirmware);
        }

        // The backup's settings as they will be applied: what can't apply removed, secrets in.
        Error prepareSettings(const PreviewInput &in, JsonObjectConst current, Preview &out, std::string &applyText)
        {
            std::string buffer = in.parsed->settings;
            DynamicJsonDocument doc(inPlaceCapacity(buffer, 2048)); // + the secrets put in
            if (!parseInPlace(buffer, doc) || !doc.is<JsonObject>())
                return Error::Damaged;
            JsonObject settings = doc.as<JsonObject>();
            listMissing(current, settings, "", out.kept);
            listMissing(settings, current, "", out.ignored);
            markPins(settings, in.device.pins, out.notApplicable);
            markBluetooth(settings, in, out.notApplicable);
            markPushover(settings, in, out.notApplicable);
            if (in.secrets)
                injectSecrets(settings, *in.secrets, in.device.bluetoothBuild);
            serializeJson(settings, applyText);
            return Error::None;
        }
    } // namespace

    Error buildPreview(const PreviewInput &in, Preview &out, std::string *error)
    {
        out = Preview{};
        const Config &current = *in.current;
        out.candidate = current;
        out.secretsState = in.secrets ? SecretsState::Restored : (in.parsed->hasSecrets ? SecretsState::Kept : SecretsState::None);

        std::string currentJson = current.toJson(false);
        DynamicJsonDocument currentDoc(inPlaceCapacity(currentJson, 0));
        parseInPlace(currentJson, currentDoc);
        std::string applyText;
        const Error prepared = prepareSettings(in, currentDoc.as<JsonObjectConst>(), out, applyText);
        if (prepared != Error::None)
            return prepared;
        // The same rules as POST /api/config: masks keep secrets, types are strict, validate() runs.
        if (!Config::applyJson(applyText, out.candidate, true, error))
            return Error::Invalid;
        applyText.clear();

        std::string candidateJson = out.candidate.toJson(false);
        DynamicJsonDocument candidateDoc(inPlaceCapacity(candidateJson, 0));
        parseInPlace(candidateJson, candidateDoc);
        listChanges(currentDoc.as<JsonObjectConst>(), candidateDoc.as<JsonObjectConst>(), "", out.changes);
        secretStatus(in, out);
        out.restart = restartReasons(current, out.candidate);
        addWarnings(in, out);
        return Error::None;
    }

    namespace
    {
        const char *warningCode(Warning warning)
        {
            switch (warning)
            {
            case Warning::WifiNetwork:
                return "wifi-network";
            case Warning::DevicePassword:
                return "device-password";
            case Warning::Address:
                return "address";
            case Warning::ImagingApp:
                return "imaging-app";
            case Warning::NewerFirmware:
                break;
            }
            return "newer-firmware";
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

        // Capacity for previewJson: every string is referenced, values are raw JSON.
        size_t previewCapacity(const Preview &p)
        {
            size_t size = 2048 + 96 * p.changes.size() + 128 * p.warnings.size();
            for (const NotApplicable &n : p.notApplicable)
                size += 96 + n.path.size();
            size += 16 * (p.kept.size() + p.ignored.size() + p.secretsChanged.size() + p.secretsUnchanged.size());
            return size;
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

        void writeList(JsonArray target, const std::vector<std::string> &paths)
        {
            for (const std::string &path : paths)
                target.add(path.c_str());
        }

        void writeWarnings(JsonArray target, const Preview &p, const Parsed &parsed)
        {
            for (Warning warning : p.warnings)
            {
                JsonObject item = target.createNestedObject();
                item["code"] = warningCode(warning);
                if (warning == Warning::WifiNetwork)
                    item["ssid"] = p.candidate.wifi.ssid.c_str();
                else if (warning == Warning::Address)
                    item["hostname"] = p.candidate.wifi.hostname.c_str();
                else if (warning == Warning::NewerFirmware)
                    item["firmware"] = parsed.firmware.c_str();
            }
        }

        void writeSettingLists(JsonObject root, const Preview &p)
        {
            JsonArray changes = root.createNestedArray("changes");
            for (const Change &c : p.changes)
            {
                JsonObject item = changes.createNestedObject();
                item["path"] = c.path.c_str();
                item["from"] = serialized(c.from.c_str(), c.from.size());
                item["to"] = serialized(c.to.c_str(), c.to.size());
            }
            JsonObject secrets = root.createNestedObject("secrets");
            secrets["state"] = secretsStateName(p.secretsState);
            writeList(secrets.createNestedArray("changed"), p.secretsChanged);
            writeList(secrets.createNestedArray("unchanged"), p.secretsUnchanged);
            writeList(root.createNestedArray("kept"), p.kept);
            writeList(root.createNestedArray("ignored"), p.ignored);
            JsonArray notApplicable = root.createNestedArray("notApplicable");
            for (const NotApplicable &n : p.notApplicable)
            {
                JsonObject item = notApplicable.createNestedObject();
                item["path"] = n.path.c_str();
                if (!n.value.empty())
                    item["value"] = serialized(n.value.c_str(), n.value.size());
                item["reason"] = reasonCode(n.reason);
            }
        }
    } // namespace

    std::string previewJson(const Preview &preview, const Parsed &parsed, const std::string &token)
    {
        DynamicJsonDocument doc(previewCapacity(preview));
        doc["token"] = token.c_str();
        writeHeader(doc.createNestedObject("backup"), parsed);
        writeSettingLists(doc.as<JsonObject>(), preview);
        JsonArray restart = doc.createNestedArray("restart");
        for (int i = 0; i < RESTART_REASON_COUNT; ++i)
            if (preview.restart & (1U << i))
                restart.add(restartCode(i));
        writeWarnings(doc.createNestedArray("warnings"), preview, parsed);
        std::string out;
        serializeJson(doc, out);
        return out;
    }
} // namespace SQM::Backup
