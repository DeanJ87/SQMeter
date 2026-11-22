# Interfaces: Rain needs cloud

## REST `/api/config` (GET, POST)

`alpaca.rainNeedsCloud` (boolean) and `alpaca.rainNeedsCloudPercent` (integer 1-100). Missing on
POST: unchanged. Out of range: 400 `{"error": "..."}` like the other safety limits. Included in
settings backup and restore (spec 033) through the `alpaca` object.

`alerts.events.dew_suspected`: `{level 0-4, sound, title, message}` like every event.

## REST `/api/safety` (GET), readings `safety`, WebSocket, MQTT `<base>/safety`

Always: `dewSuspected` (boolean). While true: `dew` object with `rainIntensity` (mm/h),
`cloudCoverPercent` (%) and `sinceAgeMs`. Not an unsafe reason: `safe`, `reasons` and
`reasonFlags` are unaffected by it. Numbers are never localised (I18N-06).

Unsafe reasons for rain (`reasonFlags` bit 9, `UnsafeRain`), English as sent:

- `Rain detected` / `Rain 0.40 mm/h` (setting off)
- `Rain 0.40 mm/h with cloud 35% >= 10%`
- `Rain 0.40 mm/h with no cloud reading`
- `Rain 0.40 mm/h, held until dry`

## MQTT / Home Assistant

`binary_sensor` `<id>_dew_suspected`, name "Dew suspected", device class `problem`,
`state_topic` `<base>/safety`, `value_template` `{{ 'ON' if value_json.dewSuspected else 'OFF' }}`.
Discovered while the rain sensor's group and safety publishing are on; otherwise its config topic
is cleared.

## Alerts

Event key `dew_suspected` (webhook/MQTT `event`), built-in title "Rain sensor: dew suspected",
message "Rain {rain_rate} mm/h under a clear sky (cloud {cloud}%). Check the rain sensor's lens."
Variables `{rain_rate}`, `{cloud}`.
