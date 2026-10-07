# Alerts

SQMeter can send push notifications itself - over Pushover, ntfy, a webhook, or MQTT - so you hear about rain or a safety change even when N.I.N.A. and the observatory PC aren't running.

Configure everything in **Settings → Alerts**, then **Save** and use **Send test** next to each channel.

---

## Events

| Event | When | Urgent |
|---|---|---|
| Observatory UNSAFE / safe | The SafetyMonitor verdict changes (after the safe delay). Unsafe alerts list the reasons | Unsafe: yes |
| Rain detected / cleared | The RG-15 starts reporting rain / the rain clear delay passes with no rain | Rain: yes |
| Sensor fault / recovered | A sensor (TSL2591, MLX90614, BME280, RG-15) goes offline or stale, or recovers. Also the RG-15 lens-fault flag | Fault: yes |
| Dew risk | Temperature comes within the configured margin of the dew point (off by default) | No |
| Clear skies | Cloud cover drops below the configured percentage (off by default) | No |

"Urgent" alerts use Pushover's **Priority for urgent alerts** setting (High by default, which bypasses quiet hours; Emergency repeats every minute for up to an hour until acknowledged) and ntfy's `high` priority.

**Cooldown** (default 5 min) is the minimum time between alerts of the same kind, so a flapping condition doesn't spam you. A change held back by the cooldown isn't lost: if the condition still differs from what you were last told when the cooldown ends, that alert is sent then - the most recent alert always matches reality.

Nothing is sent in the first minute after boot, so a restart doesn't announce the device's startup state. Sensor faults and recoveries must also last 30 seconds before they're sent, so brief blips (saving settings, a sensor being reconfigured, an OTA upload) don't page anyone.

---

## Channels

### Pushover

1. Create an application at [pushover.net](https://pushover.net/apps/build) and copy its **API token**
2. Copy your **user key** from the Pushover dashboard
3. Enable **Pushover**, paste both, pick the urgent priority (and optionally a sound), **Save**, **Send test**

### ntfy

Enable **ntfy** and set a topic. The server defaults to `https://ntfy.sh`; use your own server's URL if you self-host, plus an access token if it requires one. Topics on ntfy.sh are public - choose a long, unguessable topic name, then subscribe to it in the ntfy app.

### Webhook

POSTs a JSON body to any `http://` or `https://` URL - e.g. a Home Assistant webhook trigger, Node-RED, or a relay to Discord/Slack:

```json
{"device":"SQM-ESP32","event":"rain_started","title":"Rain detected","message":"The rain sensor reports rain (2.4 mm/h).","priority":1,"timestamp":1759500000}
```

`event` is one of `unsafe`, `safe`, `rain_started`, `rain_stopped`, `sensor_fault`, `sensor_recovered`, `lens_fault`, `dew_risk`, `clear_sky`, `test`. An optional **Authorization header** value is sent as-is (e.g. `Bearer <token>`).

HTTPS webhooks are verified against a built-in set of common root CAs (Let's Encrypt, DigiCert, Sectigo/USERTrust, Google Trust Services, Amazon). For a self-signed server on your own network, either use `http://` or tick **Skip TLS certificate checks** - only do that on a network you trust.

### MQTT

Uses the broker from the MQTT settings. Publishes:

- `<topic>/alerts` - each alert as JSON (`event`, `title`, `message`, `priority`, `device`, `timestamp`), not retained
- `<topic>/safety` - retained `{"isSafe": bool, "reasons": [...]}`, published on every change and refreshed every minute

---

## Recent alerts

While alerts are on, a bell in the header shows how many alerts arrived since you last looked, and opens the last 20 alerts since boot with each channel's delivery status (`sent`, `failed` with the reason, or `skipped` - e.g. no WiFi, or an OTA update in progress). **Send test** waits for that status and shows the actual result. The list is also available from `GET /api/alerts/recent`.

## Pushover keys

The **user key** is the 30-character key at the top of your Pushover dashboard; the **app token** is the 30-character API token of an application you create there. Settings rejects anything else - a pasted email address, or the token in the user field, is the usual cause of Pushover's "user identifier is not a valid user" error.
