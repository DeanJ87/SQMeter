# Languages

The device's web interface is available in 14 languages: English, Bahasa Indonesia, Deutsch, Español, Français, Italiano, Nederlands, Polski, Português (Brasil), Türkçe, العربية (Arabic, right-to-left), 日本語, 한국어 and 简体中文.

## Choosing a language

Open **Settings → Device → Language**, pick the language and **Save**.

- The language is a device setting, so it applies to everyone who opens the device.
- English is built in. Another language is a small file (about 22 KB) that the device downloads from the GitHub release matching its firmware and keeps in its storage. The page reloads in the new language once the file is there, usually within a few seconds.
- Switching back to English deletes the file.
- After a firmware update the device downloads the file for the new version by itself.

Numbers, dates and times follow the language too (for example a decimal comma in German), and Arabic lays the page out right to left. Readings, units, coordinates and charts stay left to right.

## Without internet

If the device can't reach GitHub (no internet, or a firewall), the interface stays in English and the Language card says why. You can install the file by hand:

1. On a computer with internet, open the [release page](https://github.com/DeanJ87/SQMeter/releases) for the firmware version shown under **Updates**.
2. Download `sqmeter-i18n-<code>.json.gz` for your language, for example `sqmeter-i18n-es.json.gz`.
3. In **Settings → Device → Language**, choose **Upload a language file** and pick the file.

A file made for a different firmware version still works: any text it lacks shows in English, and the card says so.

## What stays in English

- **Alerts the device sends** (ntfy, Pushover, webhook, MQTT and Bluetooth). Write your own alert title and message under **Settings → Alerts** to send them in your language.
- The demo's own controls (the Demo panel and the tour).
- Names and units: SQMeter, N.I.N.A., Alpaca, MQTT, mag/arcsec², °C and so on.

## Improving a translation

The translations were written for SQMeter by an AI acting as a native-speaker UI writer for each language, with a glossary per language and a review pass. If something reads oddly in your language, a pull request or an issue is very welcome; see [Translations](../development/translations.md).
