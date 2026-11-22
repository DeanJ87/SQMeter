# Data model: Rain needs cloud

## Settings (`alpaca`)

| Key | Type | Default | Range | Meaning |
|---|---|---|---|---|
| `rainNeedsCloud` | bool | false | | Rain only counts when the sky sensor sees cloud, or can't be trusted |
| `rainNeedsCloudPercent` | whole number | 10 | 1-100 | Cloud cover at or above which rain is confirmed |

Alerts: `alerts.events.dew_suspected` `{level, sound, title, message}`, default level 0 (off).

## Rain classification (`lib/AlpacaLogic/RainTrust.h`)

`RainClass`: `Dry`, `Unchecked` (setting off), `Cloud` (cloud >= level), `NoSkyReading`, `Held`
(latched), `DewSuspected`.

Inputs: `raining` (RG-15 rain or hold-off), `skyUsable` (D2 in research), `cloudCoverPercent`,
the setting and its level, and the latch.

| raining | setting | sky usable | cloud vs level | latched | class |
|---|---|---|---|---|---|
| no | any | any | any | any | Dry (latch resets) |
| yes | off | any | any | any | Unchecked |
| yes | on | yes | >= level | any | Cloud |
| yes | on | no | | any | NoSkyReading |
| yes | on | yes | < level | yes | Held |
| yes | on | yes | < level | no | DewSuspected |

Every class except `Dry` and `DewSuspected` is trusted rain and sets the latch.

## Safety status additions

- `SafetyResult.dewSuspected`, `rainRateMmPerHour`, `cloudCoverPercent`.
- `SafetyStatus.dewSuspected`, `dewRainRateMmPerHour`, `dewCloudCoverPercent`, `dewSinceMs`.

## Documents

`/api/safety` (and readings `safety`, MQTT `<base>/safety`):

```json
{ "dewSuspected": true, "dew": { "rainIntensity": 0.12, "cloudCoverPercent": 0, "sinceAgeMs": 64000 } }
```

`dew` is present only while `dewSuspected` is true.
