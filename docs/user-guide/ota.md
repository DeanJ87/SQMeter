# OTA Updates

Update firmware over WiFi without a USB cable.

---

## Check for Updates (Recommended)

![Updates page](../assets/screenshots/updates.png)

The **Updates** page can check GitHub Releases directly and update the device itself - no downloading or uploading required.

1. Open the web UI and go to **Updates**
2. Under **Firmware**, pick a **Release track**:
    - **Stable** - tagged releases (`prerelease: false` on GitHub)
    - **Beta** - pre-release builds (`prerelease: true` on GitHub)
3. Pick a specific release from the dropdown (defaults to the newest on the selected track) - a badge shows whether it's newer than the running firmware
4. Click **Update to `<tag>`**

In the list, the release you're running is marked **installed** and older ones **older**:

- The installed release can't be installed again: its button reads **Installed**.
- Choosing an older release turns the button into a red **Downgrade to `<tag>`**, with a warning. Older firmware may not read settings saved by a newer one; if it can't, it starts with defaults and you set it up again from its WiFi hotspot. Releases before v0.2.0-beta.3 also can't check for updates themselves (their release list was too small), so from one of those you update by [uploading](#via-web-ui-manual-upload) the firmware.

The device downloads `sqmeter-firmware-<tag>.bin` and `sqmeter-littlefs-<tag>.bin` directly from `api.github.com` over HTTPS and flashes both before rebooting - firmware and web UI are updated together as a matched pair. The web UI is written first; the device only switches to the new firmware once that's written and checked, so a failure keeps the old firmware running. A failure after the web UI is written can leave the new web UI with the old firmware until you retry. A release only appears in the list if both assets exist for it. On the Bluetooth build the device fetches `sqmeter-ble-firmware-<tag>.bin` instead, so it stays on the Bluetooth build.

Progress and errors are pushed to the page over the status WebSocket; if the connection to GitHub fails partway through (no internet, DNS, etc.), the device aborts cleanly and keeps running exactly what it was running before - see [How self-update failure handling works](#how-self-update-failure-handling-works) below.

!!! note "TLS"
    The device validates GitHub's certificate chain against two pinned root CAs (covering `api.github.com` and the release-asset CDN) rather than trusting any certificate - it will refuse to update if GitHub's certificate doesn't chain to one of them.

---

## Via Web UI (Manual Upload)

1. Download `sqmeter-firmware-vX.Y.Z.bin` from [GitHub Releases](https://github.com/DeanJ87/SQMeter/releases)
2. Open the web UI and go to **Updates**
3. Under **Manual upload**, choose **Firmware** as the image and select the `.bin` file
4. Click **Upload**
5. The device reboots automatically into the new firmware

If switching the device to the new firmware fails once after a complete upload ("Could not activate partition"), it checks the image again and retries the switch before answering; the API reply then includes `"retried": true`.

!!! warning "Don't interrupt"
    Keep the browser open during upload. A power cut mid-flash leaves the slot being written incomplete; it's never booted, so the device keeps starting the firmware it was running.

!!! note "Web UI updates"
    To update the web UI (the dashboard/settings pages), upload `sqmeter-littlefs-vX.Y.Z.bin` under **Manual upload** with **Web UI (littlefs.bin)** as the image, or use esptool directly. The web UI update doesn't touch the firmware.

!!! warning "Security"
    Updates are open to anyone who can reach the web UI unless **Settings → Device → Security → Password-protect changes** is on. Even then the login is plain HTTP: keep the device on a trusted network and don't expose it through port forwarding.

---

## API Endpoints

The Updates page uses these endpoints:

| Endpoint | Purpose | Artifact |
|----------|---------|----------|
| `GET /api/updates/check?track=stable\|beta` | List GitHub releases with a matched firmware+filesystem asset pair, filtered by track | - |
| `POST /api/updates/apply` | Self-download and flash a specific release. Body: `{"firmwareAssetUrl","firmwareAssetSize","fsAssetUrl","fsAssetSize"}` (from a `check` response entry) | fetched from GitHub |
| `POST /api/update` | Manual firmware upload | `sqmeter-firmware-vX.Y.Z.bin` |
| `POST /api/update/fs` | Manual LittleFS/web UI upload | `sqmeter-littlefs-vX.Y.Z.bin` |

Both upload endpoints take `multipart/form-data` and return `200 {"success": true}` or `500 {"error": "..."}`. The firmware endpoint reboots automatically after a successful upload.

---

## Version Numbers

The badge on the Updates page compares the running version with each release using semantic-version order: `0.2.0-beta.1` < `0.2.0-beta.2` < `0.2.0`. Release builds report their tag (without the `v`). A build you compile yourself reports the latest release plus `+dev` (for example `0.2.0-beta.1+dev`), which counts as that release - so newer releases show as updates and older ones don't.

---

## Via ArduinoOTA

Command-line ArduinoOTA is disabled by default. To enable it, set both fields below in the device configuration and restart:

```json
{
  "ota": {
    "enabled": true,
    "password": "use-a-long-random-password"
  }
}
```

Use the same password from your upload tool. If `ota.enabled` is false or `ota.password` is empty, the device will not start the ArduinoOTA listener.

---

## Via esptool (USB)

If the device is unresponsive over WiFi, fall back to USB:

```bash
# Firmware only
esptool.py --chip esp32 --port PORT --baud 115200 \
  write_flash 0x10000 sqmeter-firmware-vX.Y.Z.bin

# Full reflash (nuclear option)
esptool.py --chip esp32 --port PORT --baud 115200 \
  write_flash 0x0 sqmeter-complete-flash-vX.Y.Z.bin
```

---

## How OTA Works

The partition table has two app slots (`app0` at `0x10000`, `app1` at `0x190000`). OTA writes the new firmware to the slot that isn't running; the image is verified when the write finishes, and only then is the bootloader pointed at it for the next boot. A partial or corrupt download is therefore never booted.

If the new firmware is invalid or fails before it has started, the bootloader goes back to the previous slot. Once the new firmware has started it is kept, even if it misbehaves later, so the way back from a bad release is to install another one (or the previous one) from **Updates**, or over USB.

<!-- diagram: DIA-08
sources: src/OtaUpdater.cpp#OtaUpdater::runApply src/OtaUpdater.cpp#OtaUpdater::downloadAndFlashFirmware src/OtaUpdater.cpp#OtaUpdater::downloadAndFlashFilesystem lib/ReleaseLogic/ src/OtaUpdater.cpp#OtaUpdater::checkForUpdate
blocking: false
fingerprint: c6651a1f12020f2c
-->
<figure class="diagram" markdown>

```mermaid
sequenceDiagram
    accTitle: Updating from GitHub releases
    accDescr: The browser asks the device for releases; the device fetches the list from GitHub itself. On Update, the device downloads and writes the web UI image first and the firmware last, switching the boot slot only after the firmware is verified, then restarts. Any failure before that leaves the old firmware booting.
    participant B as Browser
    participant D as SQMeter
    participant G as GitHub
    B->>D: Check for updates, stable or beta
    D->>G: List releases, over HTTPS with pinned root certificates
    G-->>D: Releases
    D-->>B: Releases with both a firmware and a web UI image
    B->>D: Update to the chosen release
    D->>G: Download the web UI image
    D->>D: Erase and rewrite the web UI partition
    D->>G: Download the firmware image
    D->>D: Write the unused app slot, verify it
    opt Every step succeeded
        D->>D: Point the bootloader at the new slot
        D-->>B: Progress 100 %
        D->>D: Restart into the new firmware
    end
    opt A download or write failed
        D-->>B: The error
        Note over D: The boot slot is unchanged:<br/>the old firmware keeps running
    end
    Note over B,D: Manual upload: the browser sends a firmware<br/>or web UI file, written the same way, then restart
```

<figcaption>Updating from GitHub releases, and manual upload. Progress reaches the page over the status WebSocket.</figcaption>
</figure>

??? info "Diagram in words"

    1. **Check**: the browser asks the device for releases on the stable or beta track. The device fetches the list from GitHub itself, over HTTPS checked against pinned root certificates, and returns only releases that have both a firmware image (the Bluetooth one on the BLE build) and a web UI image.
    2. **Update**: the browser sends the chosen release. The device downloads the **web UI image first**, erasing and rewriting the web UI (LittleFS) partition as it goes, then the **firmware**, written to the app slot that isn't running and verified when complete.
    3. **Success**: only after the firmware is verified does the device point the bootloader at the new slot, report 100 % and restart into the new firmware.
    4. **Failure** (no internet, a failed download or write): the device reports the error and the boot slot is unchanged, so the old firmware keeps running. A failure during the web UI write can leave the web UI unusable until a later update succeeds; the REST API and update endpoints still work.
    5. **Manual upload**: the browser sends a firmware or web UI file to the device, which writes it the same way and restarts.
    6. If the new firmware is invalid or fails before it has started, the bootloader returns to the previous slot; once it has started, it's kept.

The LittleFS filesystem update is separate from app OTA slots. It replaces the dashboard/settings assets and preserves NVS configuration, but an interrupted filesystem upload can leave the web UI unavailable until LittleFS is flashed again over USB or a later successful OTA filesystem upload.

---

## How Self-Update Failure Handling Works

`POST /api/updates/apply` flashes the **filesystem first, then the firmware**, and only reboots once both have succeeded - deliberately the reverse of upload order, because writing the new firmware is the one irreversible step (it flips the boot partition the instant it succeeds). If the device loses connectivity or power at any point:

- **Before the firmware write starts** (checking, or mid-filesystem-download): nothing has changed that affects what boots next time. The device keeps running exactly what it was running before.
- **During the firmware write**: the write is aborted and the boot partition is left untouched, same as above.
- **After the firmware write succeeds but before reboot**: this can't happen in practice - the reboot is triggered immediately after the firmware write completes, with no further network calls in between.

In all cases config (WiFi, calibration, thresholds) is untouched, since it lives entirely in NVS, a separate partition from both `app0`/`app1` and the LittleFS filesystem.

One known, pre-existing limitation shared with the manual filesystem-upload endpoint: the filesystem partition is erased before being rewritten, so a connection drop specifically *during* the filesystem write (not before, not after) leaves LittleFS corrupted until a later successful update repairs it. The REST API and OTA endpoints stay reachable either way - only the served dashboard/settings pages would be affected until then.
