# indi-allsky

[indi-allsky](https://github.com/aaronwmorris/indi-allsky) can put SQMeter's readings on your all-sky images, chart them, and use them to drive its dew heater and fan. It can also show whether the sky is safe and whether your imaging app is connected.

indi-allsky's MQTT sensor reads **one number per topic**. SQMeter publishes its readings as one JSON document on `<base>/state` ([MQTT](mqtt.md)). A [Node-RED](https://nodered.org/) flow sits in between and republishes everything as separate numeric topics.

```text
SQMeter ──sqmeter/state, safety, alerts/armed, availability──▶ MQTT broker ──▶ Node-RED flow
   ╰────────── HTTP every 30 s (optional): Alpaca connected, sun ──────────────────╯   │
                                                                                      ▼
indi-allsky (MQTT Broker Sensor) ◀── indi/mysqm/* (one number each, retained) ── MQTT broker
   └─▶ image label · charts · dew heater · fan
```

!!! example "TODO screenshot: indi-allsky-overlay.png"
    An all-sky image with the SQMeter label block in a corner: SQM, Bortle, cloud, temperature, dew point and the SAFE/UNSAFE and Imaging lines.

---

## What you need

| | Version | Notes |
|---|---|---|
| SQMeter | v0.2.0-beta.3 or later | **Settings → Network → MQTT** on, with the safe/unsafe group published |
| MQTT broker | Any (Mosquitto, the Home Assistant add-on, ...) | SQMeter, Node-RED and indi-allsky all connect to it |
| Node-RED | 3 or later | Tested on Node-RED 4 |
| indi-allsky | A release with **MQTT Broker Sensor - (10 slots)** in the Sensor A-F list | The text lines (`{custom_1}` ...) need the image pre-save hook, added in 2025.08.1 |

---

## 1. Import the flow

1. In Node-RED: **Menu → Import**, paste the flow from [The flow](#the-flow) below, choose **new flow**, **Import**.
2. Double-click each MQTT node (**SQMeter readings**, **SQMeter safety**, **SQMeter alerts on/off**, **SQMeter online** and **indi-allsky topics**) and choose your broker.
3. If SQMeter's base topic isn't `sqmeter` (**Settings → Network → MQTT → Base topic**), change the four **SQMeter ...** topics to match, e.g. `observatory/sqm/state`.
4. Optional - imaging app and sun altitude: double-click the **SQMeter -> indi-allsky** tab, and under **Environment variables** set `SQMETER_HOST` to the device's address (e.g. `192.168.1.128` or `sqmeter.local`). Leave it empty to skip the HTTP poll.
5. **Deploy.** The function node's status shows the latest SQM, temperature, cloud cover and safe/UNSAFE.

!!! example "TODO screenshot: indi-allsky-node-red.png"
    The imported flow in the Node-RED editor, deployed, with the green status under **SQMeter -> indi/mysqm topics**.

Check it's working:

```bash
mosquitto_sub -h <broker> -t 'indi/mysqm/#' -v
```

You should see every topic below at once (they're retained), then updates every publish interval.

---

## 2. Topics

All topics are **retained** and carry a plain number, so indi-allsky has every value as soon as it connects. Text topics are marked; indi-allsky can't read them (see [Words on the image](#words-on-the-image)).

### Sky and weather - from `<base>/state`, every publish interval

| Topic | Value | Unit | From |
|---|---|---|---|
| `indi/mysqm/sqm` | Sky quality | mag/arcsec² | `sky.sqm` |
| `indi/mysqm/bortle` | Bortle class, from SQMeter | 1-9 | `sky.bortle` |
| `indi/mysqm/nelm` | Naked-eye limiting magnitude | mag | `sky.nelm` |
| `indi/mysqm/lux` | Illuminance | lux | `light.lux` |
| `indi/mysqm/cloudcover` | Cloud cover | % | `clouds.coverPercent` |
| `indi/mysqm/skystate_code` | `0` clear, `1` cloudy, `2` overcast | | `clouds.condition` |
| `indi/mysqm/skyobject` | Sky temperature | °C | `infrared.skyTemperature` |
| `indi/mysqm/skyambient` | IR sensor's own temperature | °C | `infrared.ambientTemperature` |
| `indi/mysqm/ambient` | Air temperature | °C | `environment.temperature` |
| `indi/mysqm/humidity` | Relative humidity | % | `environment.humidity` |
| `indi/mysqm/dewpoint` | Dew point | °C | `environment.dewpoint` |
| `indi/mysqm/pressure` | Pressure | hPa | `environment.pressure` |
| `indi/mysqm/raining` | `1` raining, `0` dry - held for the rain clear delay | | `rain.raining` |
| `indi/mysqm/rainrate` | Rain intensity | mm/h | `rain.intensity` |
| `indi/mysqm/windspd` | Wind speed | **m/s** | `wind.speed` |
| `indi/mysqm/windgust` | Gust | m/s | `wind.gust` |
| `indi/mysqm/winddir` | Wind direction, 0 = north - not sent when calm | ° | `wind.direction` |
| `indi/mysqm/gps_fix` | `1` GPS has a fix | | `gps.fix` |
| `indi/mysqm/latitude`, `longitude` | GPS position, when there's a fix | ° | `gps.latitude`, `gps.longitude` |
| `indi/mysqm/data_age` | Age of the oldest reading | s | `dataAgeMs` |
| `indi/mysqm/stale` | `1` readings are stale | | `dataStale` |
| `indi/mysqm/last_update` | When Node-RED last got readings | Unix s | |

### Safety and status

| Topic | Value | From | When |
|---|---|---|---|
| `indi/mysqm/safe` | `1` safe, `0` unsafe - the verdict N.I.N.A. gets as IsSafe | `<base>/safety` → `safe` | On every change, and every minute |
| `indi/mysqm/seconds_until_safe` | Seconds left of the safe delay | `secondsUntilSafe` | With `safe` |
| `indi/mysqm/unsafe_flags` | Why it's unsafe, as bit flags ([list](../api/rest.md#get-apisafety)) | `reasonFlags` | With `safe` |
| `indi/mysqm/alerts_armed` | `1` alerts on, `0` off | `<base>/alerts/armed` | On change |
| `indi/mysqm/online` | `1` SQMeter connected to the broker, `0` offline | `<base>/availability` | On change |
| `indi/mysqm/imaging` | `1` an imaging app has either Alpaca device connected | HTTP poll ¹ | Every 30 s |
| `indi/mysqm/imaging_safetymonitor` | `1` the SafetyMonitor is connected | `GET /api/v1/safetymonitor/0/connected` ¹ | Every 30 s |
| `indi/mysqm/imaging_weather` | `1` the ObservingConditions device is connected | `GET /api/v1/observingconditions/0/connected` ¹ | Every 30 s |
| `indi/mysqm/sun_alt` | Sun altitude at SQMeter's location ² | `GET /api/status` → `sky.sunAltitudeDeg` ¹ | Every 30 s |
| `indi/mysqm/is_night` | `1` dark enough for SQMeter's night-only alerts ² | `sky.isNight` ¹ | Every 30 s |

¹ Only with `SQMETER_HOST` set. Whether an imaging app is connected isn't published on MQTT, so the flow asks the device's Alpaca API (read-only, no login needed). "Connected" means the app connected and hasn't disconnected: an app that crashes stays connected until SQMeter restarts.
² Only once SQMeter knows its location (**Settings → Time & Location**, or a GPS fix). indi-allsky has its own `{sun_alt}`, `{moon_alt}` and `{moon_phase}` for its own location.

### Text topics (not for indi-allsky)

`indi/mysqm/bortle_label` (`B4`), `indi/mysqm/skystate` (`clear`), `indi/mysqm/unsafe_reasons` (`Rain detected, Cloud cover 95% > 90%`), and `indi/mysqm/json` (all the readings in one object) - for Home Assistant, dashboards or loggers. The older `indi/sqmeter/infrared_ambientTemp` and `indi/sqmeter/infrared_skyTemp` are still published (same values as `skyambient` and `skyobject`).

A sensor that isn't reporting (`status` not `ok`) is skipped, never sent as zero - its topic keeps its last value. Use `online`, `stale` and `data_age` to tell. Rain, wind and GPS only appear when that hardware is enabled.

---

## 3. Set up indi-allsky

indi-allsky's **MQTT Broker Sensor** subscribes to up to 10 topics and puts their values in 10 consecutive *user slots*, starting at the sensor's **Initial Slot**. With three of them you get 30 values. This layout is used in every example on this page:

| Sensor | Initial slot | Topics, in order → slots |
|---|---|---|
| A - SQMeter sky | User Slot 10 | sqm 10 · bortle 11 · nelm 12 · lux 13 · cloudcover 14 · skystate_code 15 · skyobject 16 · skyambient 17 · safe 18 · imaging 19 |
| B - SQMeter weather | User Slot 20 | ambient 20 · humidity 21 · dewpoint 22 · pressure 23 · raining 24 · rainrate 25 · windspd 26 · windgust 27 · winddir 28 · online 29 |
| C - SQMeter status | User Slot 30 | seconds_until_safe 30 · unsafe_flags 31 · alerts_armed 32 · is_night 33 · sun_alt 34 · data_age 35 · stale 36 · gps_fix 37 · latitude 38 · longitude 39 |

If Sensor A-C are already used by other sensors, use free ones (D starts at 40, E at 50) and shift the slot numbers in the examples.

In the indi-allsky web UI, **Config → Sensors** (the sensor settings):

1. **MQTT broker** (the sensor MQTT settings, shared by all MQTT sensors):
    - **MQTT Transport** `tcp`, **MQTT Protocol** `MQTTv5` (or `MQTTv311` for an older broker)
    - **MQTT Host**: your broker, e.g. `192.168.1.127`
    - **Port** `1883` - the default is 8883 (TLS)
    - **Username** / **Password**: your broker's (the default username is `indi-allsky`)
    - **Use TLS** off for a plain 1883 broker
2. **Sensor A**: `MQTT Broker Sensor - (10 slots)`
    - **Label**: `SQMeter sky`
    - **Pin/Port 1** - the topics, comma separated, no spaces:
      ```text
      indi/mysqm/sqm,indi/mysqm/bortle,indi/mysqm/nelm,indi/mysqm/lux,indi/mysqm/cloudcover,indi/mysqm/skystate_code,indi/mysqm/skyobject,indi/mysqm/skyambient,indi/mysqm/safe,indi/mysqm/imaging
      ```
    - **Sensor A Initial Slot**: `User Slot 10`
3. **Sensor B**: `MQTT Broker Sensor - (10 slots)`, label `SQMeter weather`, initial slot `User Slot 20`:
    ```text
    indi/mysqm/ambient,indi/mysqm/humidity,indi/mysqm/dewpoint,indi/mysqm/pressure,indi/mysqm/raining,indi/mysqm/rainrate,indi/mysqm/windspd,indi/mysqm/windgust,indi/mysqm/winddir,indi/mysqm/online
    ```
4. **Sensor C**: `MQTT Broker Sensor - (10 slots)`, label `SQMeter status`, initial slot `User Slot 30`:
    ```text
    indi/mysqm/seconds_until_safe,indi/mysqm/unsafe_flags,indi/mysqm/alerts_armed,indi/mysqm/is_night,indi/mysqm/sun_alt,indi/mysqm/data_age,indi/mysqm/stale,indi/mysqm/gps_fix,indi/mysqm/latitude,indi/mysqm/longitude
    ```
5. Save, and restart indi-allsky's capture service so the sensors connect.

indi-allsky names each slot after the last part of its topic (`sqm`, `cloudcover`, ...), so its charts and sensor lists read naturally.

!!! example "TODO screenshot: indi-allsky-sensor-config.png"
    indi-allsky's sensor settings with Sensor A set to MQTT Broker Sensor, the topic list in Pin/Port 1 and Initial Slot 10, plus the MQTT host/port/TLS fields.

!!! warning "Exact topics, numbers only"
    indi-allsky subscribes to exactly the topics you list (no `#` wildcards) and only accepts payloads that are a number - a text topic is logged as an error and ignored. Every slot reads `0.0` until its first message, which is why the flow retains everything.

---

## 4. Put it on the image

In indi-allsky, the **Label Template** (image label settings) uses Python format strings: `{sensor_user_10:0.2f}` is user slot 10 with two decimals, `{sensor_user_18:0.0f}` shows a 0/1 flag as `0` or `1`. Lines starting with `#` set the size, position (`# xy:x,y`, negative = from the right/bottom), anchor (`# anchor:la` left, `ra` right) and colour (`# color:R,G,B`) for the lines after them. Add these blocks to your existing template.

**Minimal** - one line, bottom left:

```text
# xy:15,-60
# anchor:la
# color:200,200,200
SQM {sensor_user_10:0.2f}  Bortle {sensor_user_11:0.0f}  Cloud {sensor_user_14:0.0f}%
```

**Weather block** - top left, under indi-allsky's own lines:

```text
# xy:15,260
# anchor:la
# color:0,150,150
SQM {sensor_user_10:0.2f} mag/arcsec²  NELM {sensor_user_12:0.1f}
Cloud {sensor_user_14:0.0f}%  Sky {sensor_user_16:0.1f}°C
# color:150,150,150
Air {sensor_user_20:0.1f}°C  RH {sensor_user_21:0.0f}%  Dew {sensor_user_22:0.1f}°C
Pressure {sensor_user_23:0.0f} hPa
Wind {sensor_user_26:0.1f} m/s  gust {sensor_user_27:0.1f}  from {sensor_user_28:0.0f}°
Rain {sensor_user_25:0.1f} mm/h
```

**Status line, as numbers** (no hook needed):

```text
# xy:-15,-60
# anchor:ra
# color:200,200,0
Safe {sensor_user_18:0.0f}  Imaging {sensor_user_19:0.0f}  Alerts {sensor_user_32:0.0f}  Online {sensor_user_29:0.0f}
```

**Status line, in words** (with the [pre-save hook](#words-on-the-image)):

```text
# xy:-15,-120
# anchor:ra
# color:200,200,0
{custom_1}
{custom_2} · {custom_3} · {custom_4}
{custom_5}
```

which reads e.g. `UNSAFE: cloud, rain` / `Imaging · Raining · Overcast`, or `SAFE` / `Not imaging · Dry · Clear`.

**Everything, multi-line** - weather block on the left, verdict on the right:

```text
# size:30
# xy:15,-260
# anchor:la
# color:0,150,150
SQMeter  SQM {sensor_user_10:0.2f}  B{sensor_user_11:0.0f}  NELM {sensor_user_12:0.1f}
Cloud {sensor_user_14:0.0f}%  Sky {sensor_user_16:0.1f}°C  Air {sensor_user_20:0.1f}°C
RH {sensor_user_21:0.0f}%  Dew {sensor_user_22:0.1f}°C  {sensor_user_23:0.0f} hPa
Wind {sensor_user_26:0.1f}/{sensor_user_27:0.1f} m/s  Rain {sensor_user_25:0.1f} mm/h
# xy:-15,-200
# anchor:ra
# color:200,200,0
{custom_1}
{custom_2}
Safe in {sensor_user_30:0.0f} s
```

!!! example "TODO screenshot: indi-allsky-label-template.png"
    The Label Template field in indi-allsky with the "Everything" example pasted in.

!!! tip "Units"
    Values are as SQMeter sends them: °C, hPa, mm/h and **m/s** for wind. For km/h multiply by 3.6 in the flow (`windspd: num(wind.speed * 3.6, 1)`); indi-allsky's own temperature unit setting doesn't convert MQTT values.

---

## Words on the image

indi-allsky can't show text from MQTT, and a format string can't turn `0`/`1` into words. Its **image pre-save hook** can: indi-allsky runs a script before each image is saved, passes every user slot as an environment variable (`SENSOR_USER_10` ...), and puts the strings the script returns into `{custom_1}` ... `{custom_9}`. No network calls - it only reads the slots above.

1. Save the script below on the indi-allsky machine, e.g. `/home/allsky/sqmeter_presave_hook.py` (also in the repo at [`tools/integrations/indi-allsky/sqmeter_presave_hook.py`](https://github.com/DeanJ87/SQMeter/blob/main/tools/integrations/indi-allsky/sqmeter_presave_hook.py)).
2. Make it executable: `chmod +x /home/allsky/sqmeter_presave_hook.py`.
3. In indi-allsky set **Image Pre-Save Hook** to that path (the default 5 s timeout is plenty).
4. Use `{custom_1}` ... `{custom_5}` in the label template.

| Variable | Shows |
|---|---|
| `{custom_1}` | `SAFE`, or `UNSAFE: cloud, rain` (the reasons from `unsafe_flags`) |
| `{custom_2}` | `Imaging` / `Not imaging` |
| `{custom_3}` | `Raining` / `Dry` |
| `{custom_4}` | `Clear` / `Cloudy` / `Overcast` |
| `{custom_5}` | `SQMeter offline` while the device is off the broker, otherwise empty |

```python
#!/usr/bin/env python3
# indi-allsky image pre-save hook: turns SQMeter's numbers into words for
# the image label ({custom_1} ... {custom_5}).
#
# indi-allsky passes every user sensor slot as an environment variable
# (SENSOR_USER_10 ... SENSOR_USER_59) and reads custom_1 ... custom_9 back
# from the JSON file named in DATA_JSON. The slot numbers below match the
# suggested layout on https://sqmeter.dev/user-guide/indi-allsky/ - change
# them if you put the SQMeter topics in other slots.

import json
import os
import sys

SLOT_SKYSTATE = 15
SLOT_SAFE = 18
SLOT_IMAGING = 19
SLOT_RAINING = 24
SLOT_ONLINE = 29
SLOT_UNSAFE_FLAGS = 31

# SQMeter's safety reason bits (GET /api/safety -> reasonFlags).
REASONS = {
    0: 'manual override',
    1: 'no data',
    2: 'stale data',
    3: 'sensor fault',
    4: 'cloud',
    5: 'sky brightness',
    6: 'humidity',
    7: 'dew point',
    8: 'humidity sensor fault',
    9: 'rain',
    10: 'rain sensor fault',
    11: 'wind',
    12: 'gusts',
    13: 'wind sensor fault',
}


def slot(number):
    try:
        return float(os.environ['SENSOR_USER_{0:d}'.format(number)])
    except (KeyError, ValueError):
        return None


def unsafe_reasons(flags):
    if flags is None:
        return ''
    bits = int(flags)
    return ', '.join(text for bit, text in REASONS.items() if bits & (1 << bit))


try:
    data_file = os.environ['DATA_JSON']
except KeyError:
    sys.exit(1)

safe = slot(SLOT_SAFE)
reasons = unsafe_reasons(slot(SLOT_UNSAFE_FLAGS))
sky_code = slot(SLOT_SKYSTATE)
sky = {0: 'Clear', 1: 'Cloudy', 2: 'Overcast'}.get(int(sky_code), '') if sky_code is not None else ''

data = {
    'custom_1': 'SAFE' if safe == 1 else 'UNSAFE' + (': ' + reasons if reasons else ''),
    'custom_2': 'Imaging' if slot(SLOT_IMAGING) == 1 else 'Not imaging',
    'custom_3': 'Raining' if slot(SLOT_RAINING) == 1 else 'Dry',
    'custom_4': sky,
    # indi-allsky keeps the last values it received: say so when the device is offline.
    'custom_5': 'SQMeter offline' if slot(SLOT_ONLINE) == 0 else '',
}

with open(data_file, 'w') as f:
    json.dump(data, f)

sys.exit(0)
```

If you used other slots, change the `SLOT_...` numbers at the top.

---

## Using the values

indi-allsky can use any user slot as an input for:

- **Dew heater thresholds** (dew heater settings → **Enable Dew Heater Thresholds**): **Temperature Sensor Slot** and **Target Sensor Slot**. SQMeter's dew point is slot 22; for the temperature, a sensor on the lens or dome is best - SQMeter's air temperature (slot 20) is the next best thing. The heater steps up as the gap between them closes (**Low/Medium/High Threshold Delta**).
- **Fan thresholds**: **Temperature Sensor Slot** and **Target Temp**.
- **Charts**: **Extra Chart Slot 1-9** - e.g. SQM (10), cloud cover (14) and sky temperature (16) next to indi-allsky's own star count and exposure.

indi-allsky doesn't pause or stop capture on a sensor value, so the safe/imaging values are for the image and charts, not for control. Roof and imaging decisions stay with N.I.N.A. reading SQMeter's own [Alpaca SafetyMonitor](alpaca.md).

!!! example "TODO screenshot: indi-allsky-charts.png"
    indi-allsky's chart page with SQM and cloud cover from SQMeter on the extra chart slots.

---

## Troubleshooting

| Symptom | Check |
|---|---|
| A value is `0.0` on the image | `mosquitto_sub -h <broker> -t 'indi/mysqm/<name>' -v` - nothing means Node-RED isn't publishing it: is the sensor enabled and `ok` on SQMeter's dashboard? Then restart indi-allsky's capture so it resubscribes. |
| Nothing at all from the flow | `mosquitto_sub -h <broker> -t 'sqmeter/#' -v` - nothing means SQMeter isn't publishing: **Settings → Network → MQTT** status, broker address, base topic. |
| `MQTT data ValueError` in indi-allsky's log | A text topic (`skystate`, `bortle_label`, `unsafe_reasons`, `json`) is in a sensor's topic list - use the numeric ones. |
| indi-allsky never connects | **Port 1883 with Use TLS off** for a plain broker; the defaults are 8883 with TLS. |
| `imaging` stays `0` with N.I.N.A. connected | `SQMETER_HOST` set on the Node-RED tab? Open `http://<sqmeter>/api/v1/safetymonitor/0/connected` - `"Value": true` while connected. |
| `sun_alt` / `is_night` missing | SQMeter doesn't know where it is: set **Settings → Time & Location** or give the GPS a fix. |
| Values freeze | indi-allsky keeps the last value it got. `online` 0 means SQMeter is off the broker; `stale` 1 or a growing `data_age` means its sensors stopped answering. |
| Wind looks 3.6× too low | It's m/s - see the Units tip above. |

---

## Logging to InfluxDB / Grafana

Point an MQTT-in node at `<base>/state` and flatten one level: you get fields such as `sky_sqm`, `environment_temperature` and `clouds_coverPercent`. Each group has a `status` (`ok`, `missing`, `error`, `stale`) - skip groups that aren't `ok` so a dead sensor doesn't log zeros. `indi/mysqm/json` is already flat, if you'd rather log what indi-allsky sees.

---

## The flow

Also in the repo at [`tools/integrations/indi-allsky/sqmeter-indi-allsky.flow.json`](https://github.com/DeanJ87/SQMeter/blob/main/tools/integrations/indi-allsky/sqmeter-indi-allsky.flow.json).

```json
[
  {
    "id": "sqmeter_indi_tab",
    "type": "tab",
    "label": "SQMeter -> indi-allsky",
    "disabled": false,
    "info": "SQMeter readings, safety and status republished as flat numeric topics (indi/mysqm/*) for indi-allsky.\nSet SQMETER_HOST (this tab's environment variables) to the device's address to also publish whether an imaging app is connected and the sun altitude.\n\nhttps://sqmeter.dev/user-guide/indi-allsky/",
    "env": [
      {
        "name": "SQMETER_HOST",
        "value": "",
        "type": "str"
      }
    ]
  },
  {
    "id": "sqmeter_in_state",
    "type": "mqtt in",
    "z": "sqmeter_indi_tab",
    "name": "SQMeter readings",
    "topic": "sqmeter/state",
    "qos": "0",
    "datatype": "auto-detect",
    "broker": "",
    "nl": false,
    "rap": true,
    "rh": 0,
    "inputs": 0,
    "x": 160,
    "y": 60,
    "wires": [
      [
        "sqmeter_indi_fn"
      ]
    ]
  },
  {
    "id": "sqmeter_in_safety",
    "type": "mqtt in",
    "z": "sqmeter_indi_tab",
    "name": "SQMeter safety",
    "topic": "sqmeter/safety",
    "qos": "0",
    "datatype": "auto-detect",
    "broker": "",
    "nl": false,
    "rap": true,
    "rh": 0,
    "inputs": 0,
    "x": 160,
    "y": 120,
    "wires": [
      [
        "sqmeter_indi_fn"
      ]
    ]
  },
  {
    "id": "sqmeter_in_armed",
    "type": "mqtt in",
    "z": "sqmeter_indi_tab",
    "name": "SQMeter alerts on/off",
    "topic": "sqmeter/alerts/armed",
    "qos": "0",
    "datatype": "auto-detect",
    "broker": "",
    "nl": false,
    "rap": true,
    "rh": 0,
    "inputs": 0,
    "x": 160,
    "y": 180,
    "wires": [
      [
        "sqmeter_indi_fn"
      ]
    ]
  },
  {
    "id": "sqmeter_in_availability",
    "type": "mqtt in",
    "z": "sqmeter_indi_tab",
    "name": "SQMeter online",
    "topic": "sqmeter/availability",
    "qos": "0",
    "datatype": "auto-detect",
    "broker": "",
    "nl": false,
    "rap": true,
    "rh": 0,
    "inputs": 0,
    "x": 160,
    "y": 240,
    "wires": [
      [
        "sqmeter_indi_fn"
      ]
    ]
  },
  {
    "id": "sqmeter_poll_tick",
    "type": "inject",
    "z": "sqmeter_indi_tab",
    "name": "Every 30 s",
    "props": [],
    "repeat": "30",
    "crontab": "",
    "once": true,
    "onceDelay": "5",
    "topic": "",
    "x": 150,
    "y": 320,
    "wires": [
      [
        "sqmeter_poll_build"
      ]
    ]
  },
  {
    "id": "sqmeter_poll_build",
    "type": "function",
    "z": "sqmeter_indi_tab",
    "name": "Imaging app + sun (HTTP)",
    "func": "// Optional: whether an imaging app has SQMeter's Alpaca devices connected,\n// and the device's sun altitude. Neither is on MQTT, so they're read over\n// HTTP. Set SQMETER_HOST on this flow (double-click the tab) to turn it on.\nconst host = String(env.get('SQMETER_HOST') || '').trim();\nif (!host) {\n  node.status({ fill: 'grey', shape: 'ring', text: 'off - set SQMETER_HOST on the flow' });\n  return null;\n}\nnode.status({ fill: 'green', shape: 'dot', text: `polling ${host}` });\nconst base = `http://${host}`;\nreturn [[\n  { topic: 'poll/safetymonitor', url: `${base}/api/v1/safetymonitor/0/connected` },\n  { topic: 'poll/observingconditions', url: `${base}/api/v1/observingconditions/0/connected` },\n  { topic: 'poll/status', url: `${base}/api/status` },\n]];\n",
    "outputs": 1,
    "timeout": 0,
    "noerr": 0,
    "initialize": "",
    "finalize": "",
    "libs": [],
    "x": 360,
    "y": 320,
    "wires": [
      [
        "sqmeter_poll_http"
      ]
    ]
  },
  {
    "id": "sqmeter_poll_http",
    "type": "http request",
    "z": "sqmeter_indi_tab",
    "name": "GET from SQMeter",
    "method": "GET",
    "ret": "obj",
    "paytoqs": "ignore",
    "url": "",
    "tls": "",
    "persist": false,
    "proxy": "",
    "insecureHTTPParser": false,
    "authType": "",
    "senderr": false,
    "headers": [],
    "x": 580,
    "y": 320,
    "wires": [
      [
        "sqmeter_indi_fn"
      ]
    ]
  },
  {
    "id": "sqmeter_indi_fn",
    "type": "function",
    "z": "sqmeter_indi_tab",
    "name": "SQMeter -> indi/mysqm topics",
    "func": "// SQMeter -> flat topics with one number each, retained, for indi-allsky.\n// Fed by SQMeter's MQTT topics and, optionally, an HTTP poll for whether an\n// imaging app is connected (that isn't published on MQTT).\nconst OUT = 'indi/mysqm/';\nconst out = [];\nconst send = (name, value, prefix = OUT) => {\n  if (value !== null && value !== undefined) out.push({ topic: prefix + name, payload: value, retain: true });\n};\nconst num = (v, d) => (typeof v === 'number' && Number.isFinite(v) ? Number(v.toFixed(d)) : null);\nconst flag = (v) => (typeof v === 'boolean' ? (v ? 1 : 0) : null);\n\nlet p = msg.payload;\nif (typeof p === 'string' && /^[[{]/.test(p.trim())) {\n  try {\n    p = JSON.parse(p);\n  } catch (e) {\n    node.error('SQMeter sent JSON that could not be read', msg);\n    return null;\n  }\n}\nconst topic = String(msg.topic || '');\nconst source = topic.split('/').pop();\n\nif (source === 'state') {\n  // A group whose status isn't \"ok\" carries no readings: its topics are skipped.\n  const ok = (group) => (p[group] && p[group].status === 'ok' ? p[group] : {});\n  const sky = ok('sky'), light = ok('light'), env = ok('environment'), ir = ok('infrared');\n  const clouds = ok('clouds'), rain = ok('rain'), wind = ok('wind'), gps = ok('gps');\n  const condition = { clear: 0, cloudy: 1, overcast: 2 }[clouds.condition];\n  const v = {\n    sqm: num(sky.sqm, 2),\n    bortle: num(sky.bortle, 0),\n    nelm: num(sky.nelm, 2),\n    lux: num(light.lux, 6),\n    ambient: num(env.temperature, 1),\n    humidity: num(env.humidity, 1),\n    dewpoint: num(env.dewpoint, 1),\n    pressure: num(env.pressure, 1),\n    cloudcover: num(clouds.coverPercent, 0),\n    skystate_code: condition === undefined ? null : condition,\n    raining: flag(rain.raining),\n    rainrate: num(rain.intensity, 2),\n    windspd: num(wind.speed, 1),\n    windgust: num(wind.gust, 1),\n    // The device leaves this out when it's calm or there's no vane.\n    winddir: num(wind.direction, 0),\n    skyambient: num(ir.ambientTemperature, 1),\n    skyobject: num(ir.skyTemperature, 1),\n    gps_fix: flag(gps.fix),\n    latitude: gps.fix ? num(gps.latitude, 5) : null,\n    longitude: gps.fix ? num(gps.longitude, 5) : null,\n    data_age: typeof p.dataAgeMs === 'number' ? num(p.dataAgeMs / 1000, 1) : null,\n    stale: flag(p.dataStale),\n    last_update: Math.round(Date.now() / 1000),\n  };\n  for (const [name, value] of Object.entries(v)) send(name, value);\n  // Text versions for other consumers (indi-allsky only reads numbers).\n  send('bortle_label', v.bortle !== null ? `B${v.bortle}` : null);\n  send('skystate', clouds.condition || null);\n  send('json', JSON.stringify(v));\n  // The IR sensor's two temperatures under their older names.\n  send('infrared_ambientTemp', v.skyambient, 'indi/sqmeter/');\n  send('infrared_skyTemp', v.skyobject, 'indi/sqmeter/');\n  context.set('summary', `SQM ${v.sqm ?? '--'} | ${v.ambient ?? '--'}°C | Cloud ${v.cloudcover ?? '--'}%`);\n  context.set('raining', v.raining);\n} else if (source === 'safety') {\n  send('safe', flag(p.safe));\n  send('seconds_until_safe', num(p.secondsUntilSafe, 0));\n  send('unsafe_flags', num(p.reasonFlags, 0));\n  send('unsafe_reasons', Array.isArray(p.reasons) ? (p.reasons.length ? p.reasons.join(', ') : 'none') : null);\n  context.set('safe', p.safe);\n} else if (source === 'armed') {\n  send('alerts_armed', String(p).trim() === '1' ? 1 : 0);\n} else if (source === 'availability') {\n  send('online', String(p).trim() === 'online' ? 1 : 0);\n} else if (topic === 'poll/safetymonitor' || topic === 'poll/observingconditions') {\n  // Alpaca GET .../connected answers {\"Value\": true|false, ...}\n  const connected = msg.statusCode === 200 && p && typeof p.Value === 'boolean' ? p.Value : null;\n  const name = topic === 'poll/safetymonitor' ? 'imaging_safetymonitor' : 'imaging_weather';\n  context.set(name, connected);\n  send(name, flag(connected));\n  const sm = context.get('imaging_safetymonitor');\n  const oc = context.get('imaging_weather');\n  if (sm !== undefined && oc !== undefined && (sm !== null || oc !== null)) send('imaging', sm || oc ? 1 : 0);\n} else if (topic === 'poll/status') {\n  const sky = (msg.statusCode === 200 && p && p.sky) || {};\n  send('sun_alt', num(sky.sunAltitudeDeg, 1));\n  send('is_night', flag(sky.isNight));\n} else {\n  return null;\n}\n\nconst safe = context.get('safe');\nnode.status({\n  fill: context.get('raining') ? 'red' : safe === false ? 'yellow' : 'green',\n  shape: 'dot',\n  text: `${context.get('summary') || 'waiting for readings'}${safe === undefined ? '' : safe ? ' | safe' : ' | UNSAFE'} | ${new Date().toLocaleTimeString('en-GB', { hour12: false })}`,\n});\nreturn [out];\n",
    "outputs": 1,
    "timeout": 0,
    "noerr": 0,
    "initialize": "",
    "finalize": "",
    "libs": [],
    "x": 600,
    "y": 150,
    "wires": [
      [
        "sqmeter_indi_out"
      ]
    ]
  },
  {
    "id": "sqmeter_indi_out",
    "type": "mqtt out",
    "z": "sqmeter_indi_tab",
    "name": "indi-allsky topics",
    "topic": "",
    "qos": "",
    "retain": "",
    "respTopic": "",
    "contentType": "",
    "userProps": "",
    "correl": "",
    "expiry": "",
    "broker": "",
    "x": 860,
    "y": 150,
    "wires": []
  }
]
```

---

## References

- indi-allsky wiki: [Sensors](https://github.com/aaronwmorris/indi-allsky/wiki/Sensors) (MQTT Broker Sensor, user slots) and [Image Labels](https://github.com/aaronwmorris/indi-allsky/wiki/Image-Labels) (template syntax, `sensor_user_*`, `custom_*`)
- indi-allsky's [example pre-save hook](https://github.com/aaronwmorris/indi-allsky/blob/main/misc/example_image_presave_hook.py)
- SQMeter: [MQTT](mqtt.md), [`GET /api/safety`](../api/rest.md#get-apisafety), [Alpaca](alpaca.md)
