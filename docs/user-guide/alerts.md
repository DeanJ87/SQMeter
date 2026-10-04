# Alerts

SQMeter can send push notifications itself - over Pushover, ntfy, a webhook, or MQTT - so you hear about rain or a safety change even when N.I.N.A. and the observatory PC aren't running.

Configure everything in **Settings → Alerts**, then **Save** and use **Send test** next to each channel.

---

## Events

Every event has its own **level**, and with Pushover on, its own **sound**:

| Event | When | Default level |
|---|---|---|
| It turns unsafe / safe again | The SafetyMonitor verdict changes (after the safe delay). Unsafe alerts list the reasons | Urgent / Normal |
| Rain starts / stops | The RG-15 starts reporting rain / the rain clear delay passes with no rain | Wake me / Normal |
| A sensor fails / recovers | A sensor (TSL2591, MLX90614, BME280, RG-15) goes offline or stale, or recovers. Also the RG-15 lens-fault flag | Wake me / Quiet |
| Dew risk | Temperature comes within the configured margin of the dew point | Off |
| Skies clear up / cloud over | Cloud cover drops below the "clear" percentage (default 20%) / rises above the "clouded over" percentage (default 70%). Between the two nothing changes, and a change has to hold for 2 minutes | Off |

| Level | Pushover | ntfy | Bluetooth phone alarm |
|---|---|---|---|
| Off | not sent | not sent | - |
| Quiet | priority -1, no sound | `low` | - |
| Normal | priority 0 | `default` | - |
| Urgent | priority 1, bypasses quiet hours | `high` | - |
| Wake me | priority 2 (emergency): repeats every minute for up to an hour until acknowledged | `max` | rings paired phones |

So "skies cloud over" can be Wake me with a loud sound while "skies clear up" stays Normal, and dew risk can be Quiet. A sound left on **Default** uses the Pushover channel's default sound. Webhook and MQTT payloads carry the level as `"level": "quiet" | "normal" | "urgent" | "wake"`.

**Cooldown** (default 5 min) is the minimum time between alerts of the same kind, so a flapping condition doesn't spam you. A change held back by the cooldown isn't lost: if the condition still differs from what you were last told when the cooldown ends, that alert is sent then - the most recent alert always matches reality.

Nothing is sent in the first minute after boot, so a restart doesn't announce the device's startup state. Sensor faults and recoveries must also last 30 seconds before they're sent, so brief blips (saving settings, a sensor being reconfigured, an OTA upload) don't page anyone.

---

### Only when it's dark

Sky alerts are limited to darkness by default: **after sunset**, **nautical dark** (sun 12° below the horizon, the default) or **astronomical dark** (18°). Darkness comes from the sun's position, so the device needs to know where it is - a GPS fix if there is one, otherwise the coordinates under **Settings → Time & Location → Location** (paste "latitude, longitude" from any maps app). If the sky is already clear when it gets dark, you get one "Dark and clear" alert. Without a clock or a location, sky alerts aren't held back.

The browser's own location can't be used on the device's plain-HTTP pages - browsers only share it with HTTPS sites - so the "Use my location" button only appears where it works.

## Channels

### Pushover

1. Create an application at [pushover.net](https://pushover.net/apps/build) and copy its **API token**
2. Copy your **user key** from the Pushover dashboard
3. Enable **Pushover**, paste both, optionally pick a default sound, **Save**, **Send test**

### ntfy

Enable **ntfy** and set a topic. The server defaults to `https://ntfy.sh`; use your own server's URL if you self-host, plus an access token if it requires one. Topics on ntfy.sh are public - choose a long, unguessable topic name, then subscribe to it in the ntfy app.

### Webhook

POSTs a JSON body to any `http://` or `https://` URL - e.g. a Home Assistant webhook trigger, Node-RED, or a relay to Discord/Slack:

```json
{"device":"SQM-ESP32","event":"rain_started","title":"Rain detected","message":"The rain sensor reports rain (2.4 mm/h).","level":"wake","timestamp":1759500000}
```

`event` is one of `unsafe`, `safe`, `rain_started`, `rain_stopped`, `sensor_fault`, `sensor_recovered`, `lens_fault`, `dew_risk`, `clear_sky`, `test`. An optional **Authorization header** value is sent as-is (e.g. `Bearer <token>`).

HTTPS webhooks are verified against a built-in set of common root CAs (Let's Encrypt, DigiCert, Sectigo/USERTrust, Google Trust Services, Amazon). For a self-signed server on your own network, either use `http://` or tick **Skip TLS certificate checks** - only do that on a network you trust.

### MQTT

Uses the broker from the MQTT settings. Publishes:

- `<topic>/alerts` - each alert as JSON (`event`, `title`, `message`, `level`, `device`, `timestamp`), not retained
- `<topic>/safety` - retained `{"isSafe": bool, "reasons": [...]}`, published on every change and refreshed every minute

---

## Recent alerts

While alerts are on, a bell in the header shows how many alerts arrived since you last looked, and opens the last 20 alerts since boot with each channel's delivery status (`sent`, `failed` with the reason, or `skipped` - e.g. no WiFi, or an OTA update in progress). **Send test** waits for that status and shows the actual result. The list is also available from `GET /api/alerts/recent`.

## Pushover keys

The **user key** is the 30-character key at the top of your Pushover dashboard; the **app token** is the 30-character API token of an application you create there. Settings rejects anything else - a pasted email address, or the token in the user field, is the usual cause of Pushover's "user identifier is not a valid user" error.
