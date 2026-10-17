# Mockups: two sky sensors

Every state in [contracts/ui.md](../contracts/ui.md), drawn with the app's own markup and CSS
(the demo build, rewritten in the browser; nothing here is shipped). Each state has a
`-desktop.png` (1280 px) and a `-phone.png` (390 px).

## Dashboard

| File | State | DS rules |
|---|---|---|
| `m1-sky-both` | Both on and ok, "Show other reading" on: name pill "SQM-L" beside Live/Bortle; the other reading ("SQM") and "Difference" as two more tiles in the existing grid | DS-02, DS-03, DS-27 |
| `m2-sky-name-only` | Both on, "Show other reading" off, or the other sensor not ok: name pill only | DS-02 |
| `m3-sky-big-difference` | Difference above 1.5 mag/arcsec²: both values shown, Difference tile amber | DS-03 |
| `m4-sky-authoritative-missing` | Authoritative SQM-L missing, SQM ok: no fallback; "Not responding" pill, one-line note naming the sensor, action "Use SQM" (FR-010) | DS-06, DS-07 |
| `m5-sqml-fault-card` | Non-authoritative SQM-L answered then failed: keeps a fault card titled with its name (spec 025 FR-013). A sensor that never answered since boot has no card | DS-02, DS-27 |

With one sensor (every existing device) the Sky Quality card is unchanged.

## Settings → Sensors

| File | State | DS rules |
|---|---|---|
| `s1-sky-sensors-both` | Rows "SQM" and "SQM-L" (TSL2591) with detected badge and enable switch; "I2C-2 SDA"/"I2C-2 SCL" after the I2C fields; "Authoritative reading" and "Show other reading" | DS-01, DS-05, DS-27 |
| `s2-sky-sensors-one` | Only the SQM, I2C-2 unset: SQM-L switch blocked ("Second I2C bus pins not set", Set up); authoritative controls blocked ("SQM and SQM-L must both be on", D-40) | DS-04, DS-06 |
| `s3-sky-sensors-authoritative-off` | Saved authoritative SQM-L switched off: one Note "SQM-L is off; no sky reading." with "Use SQM" (FR-010) | DS-07 |
| `s4-sky-quality-per-sensor` | One Sky quality card per enabled sensor, titled "SQM" / "SQM-L", each with its own averaging, offset and dark calibration | DS-01, DS-08 |

## Owner review

Pending. The components (T023-T025, T035, T036, T042) are built after the owner signs off here.
