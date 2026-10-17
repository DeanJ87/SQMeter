---
description: "Running SQMeter from a Li-ion pack: the charge and time left from the MAX17048 fuel gauge, charging over USB-C, battery alerts, the battery safety rule, power saving and deep sleep until USB power returns."
---

# Battery

The [travel board](../hardware/overview.md#travel-board-battery) runs SQMeter from a single-cell
Li-ion pack, charged over USB-C. SQMeter reads the pack's **MAX17048 fuel gauge** and the charger's
status lines, shows how much is left, warns you before it runs out, can tell your imaging app
the night is over, and puts itself to sleep before the cells cut out. Without a fuel gauge nothing
here appears and nothing changes.

## Charging and power

The charger is a power-path type: on USB-C it runs the device and charges the pack at the same time
(833 mA), and the device carries on from the pack when USB is unplugged. It only charges between
about 3 °C and 44 °C (the board's thermistor); outside that it stops and reports a **charging fault**.
The power switch turns the device off but not the charging.

## Reading the gauge

The gauge is found by itself on the main I2C bus (address `0x36`) and read every 5 seconds. It
reports the **charge** (%), the **cell voltage** and the **charge rate** (% per hour, averaged over
about 10 minutes).

- **Estimating**: right after the pack is connected the gauge needs a moment to work out the charge.
  Until it has, the dashboard says *Estimating* instead of a misleading 0 % or 100 %; the voltage is
  shown at once. The rate, and with it the time left, needs about a minute more.
- **Accuracy**: the gauge models the cell from its voltage, which is good to a few percent once the
  pack has rested or been through a cycle. Near full, the charger's *Charged* is what counts, not
  the percentage (a pack can sit at 99 %).

The **Battery** card on the dashboard shows the charge, a pill with the state, the voltage and the
time left (or time to full while charging). **System → Sensors** shows the gauge, its version, the
voltage and the charger's pin levels.

| State | Meaning |
|---|---|
| Discharging | Running from the pack |
| Charging | On USB, charging |
| Charged | On USB, the charger says the pack is full |
| Charging fault | On USB, the charger stopped: too cold or too hot to charge, or a bad cell |
| No battery | On USB with no pack fitted |

*Charged*, *Charging fault* and *No battery* come from the charger's STAT1, STAT2 and PG outputs,
so they need those three pins set in **Settings → Device → Battery**. Without them the state is worked
out from the gauge's rate (charging or discharging only).

### Time left

Time left is the charge divided by the rate while discharging; time to full is what's missing divided
by the rate while charging. Both show `--` while estimating, when the rate is close to zero (below
0.5 % per hour) or in any other state, and `> 99 h` beyond 99 hours. With the **Pack capacity** set,
time to full is never shorter than the charger's 833 mA allows, and the diagnostics show the current.

## Settings

**Settings → Device → Battery** (all muted with *No battery gauge detected* on a board without one,
except the pins):

| Setting | Default | What it does |
|---|---|---|
| Monitor battery | on | Off: the gauge isn't read and the device behaves as if none were fitted |
| Low level | 20 % | Below it: the *Battery low* alert and a warning tile on the dashboard |
| Critical level | 10 % | Below it: power saving and the *Battery critical* alert |
| Shutdown level | 5 % | At it: a warning, then sleep until USB power returns |
| Pack capacity | empty | In mAh; refines the time to full |
| Save power when low | on | Power saving and the sleep at the shutdown level (below) |
| Read sensors every | 30 s | The reading interval while saving power |
| Warn before sleeping | 30 s | How long the device warns before it sleeps |
| Pins | none | Gauge alert (diagnostics only), Charger STAT1, STAT2 and USB power (PG) |

The levels must go low > critical > shutdown. The configuration keys are in
[Configuration → Battery](configuration.md#battery).

## Alerts

Six events on **Settings → Alerts**, in their own **Battery** group, with Test, level, sound and your
own wording like every event (see [Alerts](alerts.md)):

| Event | When | Default level |
|---|---|---|
| Battery low | The charge drops below the low level | Normal |
| Battery critical | The charge drops below the critical level | Wake me |
| Going to sleep: battery empty | The device is about to sleep at the shutdown level | Wake me |
| Power restored | USB power returns after power saving or sleep; *Battery recovered* when power saving ends because the charge climbed back without USB | Off |
| Battery charged | The charger reports the pack full | Off |
| Charging fault | The charger stops with a fault | Normal |

*Battery low* and *critical* fire once on the way down and again only after the charge has risen
5 points above the level, so a pack hovering at the line doesn't page you. A pack already below a
level when the device starts (or when the gauge's first reading arrives), and a charging fault
already there at start-up, are announced once the first minute after start-up is over. *Going to sleep* and
*Power restored* are sent even in the first minute after a start-up and skip the cooldown. A **Wake
me** battery event also rings paired phones on the [Bluetooth](ble.md) build. Templates can use
`{battery}`, `{battery_voltage}`, `{battery_state}` and `{battery_time_left}`.

## The safety rule

**Settings → Safety → Battery → Unsafe below** (off by default, 10 %) makes the safety verdict unsafe
when the charge drops below the limit, so N.I.N.A. and the like can park and close before the device
goes dark:

- below the limit: *Battery 8% < 10%* (a charging pack below the limit stays unsafe until it is above it);
- no fuel gauge, monitoring off, or data stale or still estimating: *Battery data missing*;
- at the shutdown level, whether the rule is on or not: *Shutting down: battery 4% <= 5%*, before the
  device sleeps.

Like rain and wind, the battery is checked even when the other sensors' data is stale. Two options
change only who hears about it, never the verdict: **Send an alert** (an Urgent *Battery: unsafe*
alert alongside the unsafe alert, at most once per alert cooldown) and **Ring paired phones**
(Bluetooth build, with a passkey and a paired phone). The limit follows the **Critical level** until
you set it to something else. In `reasonFlags` the battery is bit 14 (below the limit), 15 (data missing) and 16
(shutting down). See [ASCOM Alpaca → Safety rules](alpaca.md#safety-rules).

## Power saving and deep sleep

With **Save power when low** on:

1. **Below the critical level** (not charging): sensors are read every 30 s instead of every few
   seconds (the gauge too), the OLED display goes off, and the dashboard's Battery card shows *Power
   saving*. Alpaca, REST and MQTT keep answering, and the data counts as fresh for the slower interval,
   so the verdict doesn't turn stale just because readings come less often. USB power (*Power
   restored*), or the charge back 5 points above the critical level (*Battery recovered*), ends it.
2. **At the shutdown level** (two readings in a row, not charging): the device sends *Going to
   sleep*, reports unsafe, and waits the warning time (30 s) for the alert to go out, 15 s more at
   most, then goes into **deep sleep**. Plugging in USB during the warning cancels it.
3. **Asleep** it draws almost nothing and doesn't answer: an imaging app sees it disconnect, which
   is why the warning and the unsafe verdict come first. It wakes:
    - **on USB power**, at once, when the charger's PG pin is set (it must be a pin that can wake the
      ESP32); it starts up normally and sends *Power restored* once WiFi is back, or after 2 minutes;
    - **every hour** to check the gauge: not charging and the charge not back 5 points above the
      critical level (a resting cell's charge creeps up a little), or the gauge not answering, it goes
      straight back to sleep without a word. Without a PG pin this hourly check is how it notices
      charging, so it can take up to an hour to wake after you plug in. The reset button always wakes it.

With **Save power when low** off nothing slows down and the device never sleeps; it runs until the
cells' protection cuts them off at about 2.4 V. Missing or estimating gauge data never moves the
device towards sleep: a gauge that stops answering can't switch it off (the safety rule reports it).

## Cells

Use one cell, or several **matched** cells in parallel (same make, model, age and charge; for example
two 18650s, about 7 Ah). Never connect cells in series: the charger is for a single cell (4.2 V).
Check the polarity before connecting the pack. See the [travel board](../hardware/overview.md#travel-board-battery).

## Integrations

- REST, WebSocket and MQTT `<base>/state`: the `battery` group ([MQTT → Readings](mqtt.md#readings-basestate),
  [REST](../api/rest.md#get-apistatus)); MQTT only with **Battery** on in the MQTT publish switches.
- Home Assistant: *Battery*, *Battery voltage*, *Battery state* and *Battery time to empty* entities
  ([MQTT → Home Assistant](mqtt.md#home-assistant)).
- Bluetooth: `battery` and `batteryState` in the sensor summary ([Bluetooth](ble.md)).
- OLED display: the *Battery* page ([OLED display](display.md#pages)).
