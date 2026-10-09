# Dashboard

The dashboard is the web UI's home page: live readings, the safety verdict and tonight's darkness, updated every second over a WebSocket. Try it without hardware in the [live demo](../live-demo.md).

![Dashboard](../assets/screenshots/dashboard.png)

---

## Status

The **Status** card answers, without a click: is the data fresh, is it safe, and will anyone be told. It is the first card (move it with **Arrange** like any other) and looks like the rest: tiles with a pill each, and a pill top-right that reads **All good** or **N to check**.

![Status card](../assets/screenshots/status-card.png)

| Tile | Shows | When it's there |
|---|---|---|
| **Data** | **Live**, **Stale** (the device's readings are old, with their age), **No updates** (nothing new has arrived for a while) or **Offline** (the cards keep the last values, greyed) | Always |
| **Safety** | **Safe**, or **Unsafe** with the reasons; **Safe in N s** while the safe delay runs | Always |
| **Alerts** | **Sending**, **Paused** with who paused it and a **Resume** button, **Waiting** for an imaging app, or **Off** | Always |
| **Imaging app** | Per Alpaca device (safety monitor, weather device): **Connected**, **Checking**, **Disconnected**, **Waiting** or **Gone quiet** (it stopped checking without disconnecting, which is a problem), and when it last checked | When the send mode needs an app, or an app has connected since the device started |

Anything else that needs attention is one row under the tiles: its name, a pill, and "?" for what it means. The row takes you to where it's fixed.

| Row | Pill | When it's there |
|---|---|---|
| **Send mode** | **Not in effect**: "Only while an imaging app is connected" needs Alpaca, which is off, so alerts go out any time | That send mode with Alpaca off |
| **Alert channels** | **None on**, or **Can't send** (every channel that is on can't work right now, e.g. MQTT alerts with MQTT off) | When it happens |
| A sensor (**Light sensor**, **IR sky sensor**, **Environment sensor**, **Rain sensor**, **Anemometer**, **GPS**) | **Not responding**, **Stale** or **Error**; "?" says what it affects and how old its last reading is | When a sensor that is on isn't working; its card stays, in a fault state |
| **Settings** | **N inactive**: settings that affect alerts or safety are switched on but can't work (e.g. Wake me without the Bluetooth build) | When there are any (from **Settings**, spec 020) |
| **Language** | **Not loaded** or **Downloading**: the chosen language's file isn't on the device yet, so the page is in English | After a language change that didn't finish |
| **Clock** / **Location** | **Not set** / **Unknown**: darkness and the night-only rules can't apply | When it happens |
| **Phone alarm** | **Ringing**, with **Acknowledge** | Bluetooth build, while ringing |

What's shown, and where each piece of device state appears (or why it doesn't), is recorded in `web/src/dashboard/inventory.json`, and every item has a test - see [Coding standards](../development/coding-standards.md#dashboard-dash). The card follows the UI design system ([DS rules](../development/coding-standards.md#ui-design-system-ds)).

## Cards

| Card | Shows | When it's there |
|---|---|---|
| **Safety Monitor** | Safe or unsafe, with each failing rule's value and limit, how long it's been that way, while rain is held how long until it clears, any safety rule that is on but can't work (e.g. the rain rule with the rain sensor off), a **Rules** link and the **History** of recent changes, restarts and alerts | Always |
| **Sky Quality** | SQM (mag/arcsec²), its description and Bortle class, a trend of recent readings, NELM and illuminance | Always |
| **Sun & Moon** | Moon phase, tonight's astronomical darkness, moonrise/moonset and a chart of sun and moon altitude through the night - hover (or tap) the chart for the time, altitudes and sky phase | When the device knows its location (GPS fix or **Settings → Time & Location → Location**) and **Sun & Moon card on the dashboard** is on |
| **Cloud Conditions** | The device's verdict (Clear / Cloudy / Overcast), cloud cover and the sky−ambient temperature delta before and after humidity correction. Notes when humidity is assumed because there's no BME280 reading | Always |
| **Environment** | Temperature, humidity, pressure, dew point | BME280 fitted |
| **GPS Location** | Fix, position, satellites, altitude, HDOP | GPS enabled |
| **Light Sensor** | Illuminance (whole lux from 100 up, 3 significant figures below) and raw TSL2591 counts | Always |
| **IR Temperature** | Sky and ambient temperature | Always |
| **Device & Network** | WiFi signal and uptime as tiles; then one row each for IPv4, every IPv6 address (global, local, link, with "?" for the last two) when IPv6 is on, the `.local` name when mDNS is on, MQTT when it's on (**Connected**, **Retrying**, or **Can't connect** with the reason behind "?"), and the firmware version with **Update available** when the Updates page found a newer release this session | Always |
| **Wind** | Mean speed and gust (m/s and km/h), direction. Speed and gust turn amber near and red at your safety limits | Anemometer enabled |
| **Rain Sensor** | Raining, intensity, event and daily totals, in the RG-15's units | Rain sensor enabled |

A sensor that is switched on but stops working keeps its card, in a fault state: **Not responding**, **Stale** or **Error**, what it affects and how old its last reading is - never zeros, and never a card that vanishes as if the sensor wasn't fitted. A sensor switched off in Settings has no card, and a BME280 that has never answered is taken as not fitted. The **System** page shows every sensor's status. Sun & Moon times are calculated in the browser for the device's location and shown in the browser's time zone (the card's "?" says which).

The **Live** badge on Sky Quality turns **Stale** when readings stop arriving; the Status card's **Data** tile says the same whichever cards are shown.

## Arranging cards

Cards flow into columns like a pin board. **Arrange** lets you drag them into your own order; **Done** keeps it and **Reset order** goes back to the default. The order is remembered per browser.

## Alerts bell

![Alerts flyout](../assets/screenshots/alerts-flyout.png)

The bell in the header lists the recent alerts with each channel's delivery result, and counts new ones. From it you can:

- **Pause** / **Resume** alerts - the same as **Pause alerts** in Settings (see [Alerts](alerts.md#when-to-send)); the bell is crossed out while alerts are paused
- **Clear** the list
- jump to the alert **Settings**

## Other pages

| Page | |
|---|---|
| **Alpaca** | The ASCOM Alpaca devices N.I.N.A. sees, with live device state - see [ASCOM Alpaca](alpaca.md) |
| **System** | Firmware, uptime, memory, time sync, every sensor's status and the RG-15 diagnostics |
| **Settings** | Device, Network, Time & Location, Sensors, Safety and Alerts - see [Configuration](configuration.md) |
| **Updates** | Install a release from GitHub or upload a file - see [OTA Updates](ota.md) |

![Alpaca page](../assets/screenshots/alpaca.png)

![System page](../assets/screenshots/system.png)
