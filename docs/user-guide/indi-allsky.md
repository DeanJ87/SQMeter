# indi-allsky

[indi-allsky](https://github.com/aaronwmorris/indi-allsky) can overlay SQMeter's readings on your all-sky images and use them in its settings. Its MQTT sensor reads **one number per topic**, while SQMeter publishes all its readings as one JSON document on `<base>/state` ([MQTT](mqtt.md)). A small [Node-RED](https://nodered.org/) flow republishes the readings as separate topics.

## The flow

1. In Node-RED: **Menu → Import**, paste the flow below, **Import**.
2. Double-click **SQMeter readings** and **indi-allsky topics** and choose your MQTT broker in each (the one SQMeter publishes to).
3. If your SQMeter's base topic isn't `sqmeter` (**Settings → Network → MQTT → Base topic**), change the topic in **SQMeter readings** to `<your base>/state`.
4. **Deploy.** The function node's status shows the latest SQM, temperature and cloud cover.

```json
[{"id": "sqmeter_indi_tab", "type": "tab", "label": "SQMeter -> indi-allsky", "disabled": false, "info": "SQMeter's <base>/state readings republished as the flat indi/mysqm/* topics indi-allsky reads."}, {"id": "sqmeter_state_in", "type": "mqtt in", "z": "sqmeter_indi_tab", "name": "SQMeter readings", "topic": "sqmeter/state", "qos": "0", "datatype": "auto-detect", "broker": "", "nl": false, "rap": true, "rh": 0, "inputs": 0, "x": 150, "y": 80, "wires": [["sqmeter_indi_fn"]]}, {"id": "sqmeter_indi_fn", "type": "function", "z": "sqmeter_indi_tab", "name": "SQMeter -> indi/mysqm topics", "func": "// SQMeter <base>/state -> the flat topics indi-allsky reads.\nlet p = msg.payload;\nif (typeof p === 'string') {\n  try { p = JSON.parse(p); } catch (e) { node.error('SQMeter state is not JSON', msg); return null; }\n}\nif (!p || typeof p !== 'object') return null;\n\n// A group whose status isn't \"ok\" carries no readings: its topics are skipped.\nconst ok = (group) => (p[group] && p[group].status === 'ok' ? p[group] : {});\nconst sky = ok('sky'), light = ok('light'), env = ok('environment'), ir = ok('infrared');\nconst clouds = ok('clouds'), rain = ok('rain'), wind = ok('wind');\n\nconst num = (v, d) => (typeof v === 'number' && Number.isFinite(v) ? Number(v.toFixed(d)) : null);\nconst flag = (v) => (typeof v === 'boolean' ? (v ? 1 : 0) : null);\n\nconst v = {\n  sqm: num(sky.sqm, 2),\n  bortle: num(sky.bortle, 0),\n  nelm: num(sky.nelm, 2),\n  lux: num(light.lux, 6),\n  ambient: num(env.temperature, 1),\n  humidity: num(env.humidity, 1),\n  dewpoint: num(env.dewpoint, 1),\n  pressure: num(env.pressure, 1),\n  cloudcover: num(clouds.coverPercent, 0),\n  skystate: clouds.condition || null,\n  raining: flag(rain.raining),\n  rainrate: num(rain.intensity, 2),\n  windspd: num(wind.speed, 1),\n  windgust: num(wind.gust, 1),\n  skyambient: num(ir.ambientTemperature, 1),\n  skyobject: num(ir.skyTemperature, 1),\n};\nv.bortle_label = v.bortle !== null ? `B${v.bortle}` : null;\nconst now = Date.now() / 1000;\nv.last_update = Math.round(now);\n\nnode.status({\n  fill: v.raining ? 'red' : v.cloudcover > 50 ? 'yellow' : 'green',\n  shape: 'dot',\n  text: `SQM ${v.sqm ?? '--'} ${v.bortle_label ?? ''} | ${v.ambient ?? '--'}°C | Cloud ${v.cloudcover ?? '--'}% | ${new Date(now * 1000).toLocaleTimeString('en-GB', { hour12: false })}`,\n});\n\nconst out = [];\nconst send = (topic, payload) => {\n  if (payload !== null && payload !== undefined) out.push({ topic, payload, retain: true });\n};\nsend('indi/mysqm/json', JSON.stringify(v));\nfor (const key of ['sqm', 'bortle', 'bortle_label', 'nelm', 'lux', 'ambient', 'humidity', 'dewpoint', 'pressure', 'cloudcover', 'skystate',\n  'raining', 'rainrate', 'windspd', 'windgust', 'skyambient', 'skyobject', 'last_update']) send(`indi/mysqm/${key}`, v[key]);\n// The IR sensor's two temperatures, under the names you already use.\nsend('indi/sqmeter/infrared_ambientTemp', v.skyambient);\nsend('indi/sqmeter/infrared_skyTemp', v.skyobject);\nreturn [out];\n", "outputs": 1, "timeout": 0, "noerr": 0, "initialize": "", "finalize": "", "libs": [], "x": 400, "y": 80, "wires": [["sqmeter_indi_out"]]}, {"id": "sqmeter_indi_out", "type": "mqtt out", "z": "sqmeter_indi_tab", "name": "indi-allsky topics", "topic": "", "qos": "", "retain": "", "respTopic": "", "contentType": "", "userProps": "", "correl": "", "expiry": "", "broker": "", "x": 660, "y": 80, "wires": []}]
```

## Topics

All are retained, so indi-allsky has values as soon as it connects.

| Topic | Value | From `<base>/state` |
|---|---|---|
| `indi/mysqm/sqm` | Sky quality, mag/arcsec² | `sky.sqm` |
| `indi/mysqm/bortle` | Bortle class (the device's own) | `sky.bortle` |
| `indi/mysqm/bortle_label` | `B1` ... `B9` | |
| `indi/mysqm/nelm` | Naked-eye limiting magnitude | `sky.nelm` |
| `indi/mysqm/lux` | Illuminance, lux | `light.lux` |
| `indi/mysqm/ambient` | Air temperature, °C | `environment.temperature` |
| `indi/mysqm/humidity` | Relative humidity, % | `environment.humidity` |
| `indi/mysqm/dewpoint` | Dew point, °C | `environment.dewpoint` |
| `indi/mysqm/pressure` | Pressure, hPa | `environment.pressure` |
| `indi/mysqm/cloudcover` | Cloud cover, % | `clouds.coverPercent` |
| `indi/mysqm/skystate` | `clear`, `cloudy` or `overcast` | `clouds.condition` |
| `indi/mysqm/raining` | `1` raining, `0` dry (held for the rain clear delay) | `rain.raining` |
| `indi/mysqm/rainrate` | Rain, mm/h | `rain.intensity` |
| `indi/mysqm/windspd` | Wind speed, **m/s** | `wind.speed` |
| `indi/mysqm/windgust` | Gust, m/s | `wind.gust` |
| `indi/mysqm/skyambient` | IR sensor's own temperature, °C | `infrared.ambientTemperature` |
| `indi/mysqm/skyobject` | Sky temperature, °C | `infrared.skyTemperature` |
| `indi/sqmeter/infrared_ambientTemp` | Same as `skyambient` | |
| `indi/sqmeter/infrared_skyTemp` | Same as `skyobject` | |
| `indi/mysqm/last_update` | Unix time of the last update | |
| `indi/mysqm/json` | All of the above as one JSON object | |

A sensor that isn't reporting (status not `ok`) is skipped, so its topic keeps its last value rather than showing a made-up zero; check `last_update` if that matters. Rain and wind only appear when that hardware is enabled.

For the safety verdict, indi-allsky can read SQMeter's own `<base>/safe` topic directly: `1` safe, `0` unsafe.

!!! note "The same in every language"
    The readings don't change with SQMeter's UI language. Numbers are JSON numbers with `.` as the decimal point and no thousands separators (`12.1`, never `"12,1"`), so the flow works unchanged whichever language the web interface uses.

## Logging to InfluxDB

Point an MQTT-in node at `<base>/state` and flatten one level: you get fields such as `sky_sqm`, `environment_temperature` and `clouds_coverPercent`. Each group carries a `status` (`ok`, `missing`, `error`, `stale`) - skip groups that aren't `ok`.
