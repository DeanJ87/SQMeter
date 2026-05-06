# MQTT Integration

SQMeter can publish sensor readings to any MQTT broker — Home Assistant, Grafana, or your own pipeline.

---

## Enabling MQTT

In the Settings page (or via API), enable MQTT and set your broker details:

```json
{
  "mqtt": {
    "enabled": true,
    "broker": "192.168.1.10",
    "port": 1883,
    "username": "mqttuser",
    "password": "mqttpass",
    "topic": "sqm/backyard",
    "publishIntervalMs": 60000
  }
}
```

The device will start publishing once it reconnects.

!!! warning "Current MQTT limitations"
    MQTT is still a plaintext LAN integration and should not be treated as a safety controller. Use the payload as telemetry and keep physical safety interlocks independent.

---

## Payload Format

Published to `{topic}` every `publishIntervalMs` milliseconds:

```json
{
  "timestamp": 1234567,
  "light": {
    "lux": 0.0234,
    "visible": 123,
    "infrared": 45
  },
  "sky": {
    "sqm": 21.5,
    "nelm": 6.2,
    "bortle": 2.0
  },
  "environment": {
    "temperature": 12.4,
    "humidity": 72.1,
    "pressure": 1013.25
  },
  "infrared": {
    "skyTemp": -15.2,
    "ambientTemp": 12.4
  },
  "clouds": {
    "temperatureDelta": -27.6,
    "correctedDelta": -24.1,
    "coverPercent": 5.0,
    "condition": 0,
    "description": "Clear"
  },
  "location": {
    "latitude": 51.5074,
    "longitude": -0.1278,
    "altitude": 42.0,
    "satellites": 8,
    "hdop": 1.2
  },
  "rain": {
    "enabled": true,
    "initialized": true,
    "online": true,
    "stale": false,
    "status": 0,
    "isRaining": false,
    "raining": false,
    "acc": 0.0,
    "eventAcc": 2.4,
    "event_accumulation": 0.0,
    "hydreon_event_accumulation": 2.4,
    "totalAcc": 12.34,
    "rInt": 0.0,
    "uart": {
      "last_command": "R",
      "last_raw_response": "Acc 0.00 mm, EventAcc 0.00 mm, TotalAcc 1.24 mm, RInt 0.00 mm/h",
      "timeouts": 0,
      "parse_errors": 0,
      "successful_reads": 42
    }
  }
}
```

`infrared` and `clouds` are omitted if the MLX90614 reading is unavailable. `location` is omitted unless GPS has a valid fix. `rain` is included whenever the RG-15 path is enabled and now carries UART diagnostics alongside the rain reading.

---

## Home Assistant

Add to your `configuration.yaml`:

```yaml
mqtt:
  sensor:
    - name: "Sky Quality (SQM)"
      state_topic: "sqm/backyard"
      value_template: "{{ value_json.sky.sqm }}"
      unit_of_measurement: "mag/arcsec²"

    - name: "Bortle Class"
      state_topic: "sqm/backyard"
      value_template: "{{ value_json.sky.bortle }}"

    - name: "NELM"
      state_topic: "sqm/backyard"
      value_template: "{{ value_json.sky.nelm }}"

    - name: "Sky Temperature"
      state_topic: "sqm/backyard"
      value_template: "{{ value_json.infrared.skyTemp }}"
      unit_of_measurement: "°C"

    - name: "Rain Intensity"
      state_topic: "sqm/backyard"
      value_template: "{{ value_json.rain.rInt | default(0) }}"
      unit_of_measurement: "mm/h"
```

---

## Mosquitto Test

Verify data is publishing:

```bash
mosquitto_sub -h 192.168.1.10 -t "sqm/#" -v
```
