# One-time USB flash

SQMeter v0.3 moves to a new partition layout that uses the whole 4 MB flash: two 1.75 MB firmware slots instead of 1.5 MB, so there's room for new features. A partition layout can only be changed over USB, so each device needs **one** USB flash to get onto v0.3. After that it updates over WiFi again, as before.

**Your settings are kept**, WiFi included. They live in a part of the flash this doesn't write.

You need a computer with Chrome or Edge (for the browser flasher) or Python (for `esptool`), and a USB data cable.

---

## Browser flasher

<div class="usb-flasher">
  <p class="usb-flasher-missing" hidden>The flasher files arrive with the first v0.3 release. Until then, use <code>esptool</code> below.</p>
  <p><esp-web-install-button manifest="../../flash/standard/manifest.json" data-flash-build="standard"><button slot="activate" class="md-button md-button--primary">Install the standard build</button></esp-web-install-button></p>
  <p><esp-web-install-button manifest="../../flash/ble/manifest.json" data-flash-build="ble"><button slot="activate" class="md-button">Install the Bluetooth build</button></esp-web-install-button></p>
</div>
<script type="module" src="../../assets/javascripts/vendor/esp-web-tools/install-button.js"></script>
<script type="module">
  // Hide the buttons until a release with the flasher files exists.
  const ok = await fetch('../../flash/standard/manifest.json', { method: 'HEAD' }).then((r) => r.ok).catch(() => false);
  if (!ok) {
    document.querySelectorAll('.usb-flasher esp-web-install-button').forEach((b) => b.closest('p').remove());
    document.querySelector('.usb-flasher-missing').hidden = false;
  }
</script>

1. Plug the ESP32 into the computer.
2. Click the button for your build: the one your device runs now (System page → Firmware) if you're moving an existing device.
3. Pick the device's serial port, then **Install**.
4. **Leave "Erase device" unticked**, then confirm. Ticking it wipes your settings.
5. Wait for "Installation complete" (about a minute), then unplug and power the device as usual.

!!! tip "No port to pick?"
    The ESP32 needs a USB-to-serial driver on some computers (CP210x or CH340, depending on the board),
    and some USB cables only carry power.

---

## esptool

Download the USB package for your build from the [release](https://github.com/DeanJ87/SQMeter/releases): `sqmeter-l2-usb-standard-*.zip` or `sqmeter-l2-usb-ble-*.zip`. Unzip it, and from that folder:

```bash
pip install esptool
esptool.py --chip esp32 --port PORT --baud 460800 write_flash \
  0x1000 bootloader.bin 0x8000 partitions.bin 0xE000 boot_app0.bin \
  0x10000 firmware.bin 0x390000 littlefs.bin
```

Don't add `--erase-all` and don't run `erase_flash`: both wipe your settings. See [Flashing](flashing.md) for finding the port.

---

## Check it worked

Open the device's page as before (its address and `sqmeter.local` don't change):

- **System → Firmware** shows v0.3 or later.
- Your settings are as you left them.
- `http://sqmeter.local/api/status` shows `"layout": "l2"` under `firmware`.

If the device comes up as the **SQM-Setup** hotspot instead, its settings didn't survive (for example "Erase device" was ticked): join the hotspot and set it up again ([First Boot](first-setup.md)).

---

## Why it's needed

| | Before (v0.2) | After (v0.3) |
|---|---|---|
| Firmware slots | 1.5 MB (standard) or 1.69 MB (Bluetooth) each | 1.75 MB each, both builds |
| Web UI (LittleFS) | 512 KB | 448 KB (the web UI is now stored compressed, about 150 KB) |
| Free flash | up to 448 KB unused | none |

v0.3 is also built on a newer platform (Arduino-ESP32 3.x / ESP-IDF 5.5), with IPv6 for secure connections and time sync. The bigger slots are what let it fit with room to grow.

Over-the-air updates can't change a partition table, and v0.3's files are built for the new layout, so a device on v0.2 isn't offered v0.3 by its Updates page. If you upload a v0.3 file to a v0.2 device by hand, it won't fit and the update fails without changing anything.

## Going back to v0.2

Flash v0.2.0-beta.3's `sqmeter-complete-flash-*.bin` (or `sqmeter-ble-complete-flash-*.bin`) at `0x0` as its release describes. That image includes the old layout and **resets the settings**: set the device up again from its hotspot.
