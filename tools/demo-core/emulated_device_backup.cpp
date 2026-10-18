// Settings backup and restore in the demo (specs/033-config-backup FR-014):
// the same lib/ConfigBackup code as the device, with random bytes from the
// browser (crypto.getRandomValues) and the portable crypto.

#include "emulated_device.h"

#include "ConfigBackup.h"

namespace
{
    // `count` bytes from the start of a hex string (missing bytes are zero).
    void fromHex(const std::string &hex, size_t offset, uint8_t *out, size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            const size_t at = (offset + i) * 2;
            out[i] = at + 1 < hex.size() ? static_cast<uint8_t>(std::strtoul(hex.substr(at, 2).c_str(), nullptr, 16)) : 0;
        }
    }

    int statusFor(Backup::Error error) { return error == Backup::Error::TooLarge ? 413 : 400; }
} // namespace

Backup::DeviceInfo EmulatedDevice::backupDevice() const
{
    Backup::DeviceInfo device;
    device.firmware = firmwareVersion;
    device.board = "esp32";
    device.bluetoothBuild = false; // the demo reports the standard build (Bluetooth unavailable)
    device.pins = Backup::classicEsp32Pins();
    return device;
}

std::string EmulatedDevice::backup(const std::string &body, const std::string &randomHex)
{
    Backup::SealParams seal;
    const Backup::Error error = Backup::readBackupRequest(body, seal.passphrase);
    if (error != Backup::Error::None)
        return response(statusFor(error), Backup::errorJson(error));
    fromHex(randomHex, 0, seal.salt, Backup::SALT_BYTES);
    fromHex(randomHex, Backup::SALT_BYTES, seal.nonce, Backup::NONCE_BYTES);
    const std::string createdAt = validEpoch() ? Core::isoUtc(validEpoch()) : std::string();
    const std::string file = Backup::makeBackup(cfg, backupDevice(), createdAt, seal.passphrase.empty() ? nullptr : &seal);
    if (file.empty())
        return response(500, DemoCore::errorJson("The backup couldn't be made"));
    return response(200, file);
}

std::string EmulatedDevice::restorePreview(const std::string &body, const std::string &randomHex)
{
    Backup::Parsed parsed;
    std::string passphrase;
    Backup::Error error = Backup::readPreviewRequest(body, parsed, passphrase);
    Backup::Secrets secrets;
    const bool restoring = error == Backup::Error::None && parsed.hasSecrets && !passphrase.empty();
    if (restoring)
        error = Backup::openSecrets(parsed, passphrase, secrets);
    Backup::Preview preview(cfg);
    std::string reason;
    if (error == Backup::Error::None)
    {
        const Backup::PreviewInput input{&cfg, &parsed, restoring ? &secrets : nullptr, backupDevice()};
        error = Backup::buildPreview(input, preview, &reason);
    }
    if (error != Backup::Error::None)
        return response(statusFor(error), Backup::errorJson(error, reason));
    uint8_t random[Backup::TOKEN_BYTES];
    fromHex(randomHex, 0, random, sizeof(random));
    staged.token = Backup::toHex(random, sizeof(random));
    staged.candidate = preview.candidate;
    staged.from = cfg.toJson(false);
    staged.restart = preview.restart != 0;
    return response(200, Backup::previewJson(preview, parsed, staged.token));
}

std::string EmulatedDevice::restoreApply(const std::string &body)
{
    const std::string token = Backup::readApplyRequest(body);
    if (staged.token.empty() || token != staged.token || staged.from != cfg.toJson(false))
        return response(409, Backup::errorJson(Backup::Error::Stale));
    cfg = staged.candidate;
    const bool restarting = staged.restart;
    staged = StagedRestore{};
    return response(200, restarting ? "{\"success\":true,\"restarting\":true}" : "{\"success\":true,\"restarting\":false}");
}

std::string EmulatedDevice::backupRoute(const std::string &route, const std::string &body, const std::string &randomHex)
{
    if (route == "preview")
        return restorePreview(body, randomHex);
    if (route == "apply")
        return restoreApply(body);
    return backup(body, randomHex);
}
