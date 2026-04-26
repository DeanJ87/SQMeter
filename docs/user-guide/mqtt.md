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

---

## Payload Format

Published to `{topic}` every `publishIntervalMs` milliseconds:

```json
{
  "timestamp": 1700000000,
  "light": {
    "lux": 0.0234,
    "visible": 123,
    "infrared": 45
  },
  "sky": {
    "sqm": 21.5,
    "nelm": 6.2,
    "bortle": 2.0,
    "description": "Typical truly dark site"
  },
  "environment": {
    "temperature": 12.4,
    "humidity": 72.1,
    "pressure": 1013.25
  },
  "rain": {
    "isRaining": false,
    "acc": 0.0,
    "eventAcc": 0.0,
    "totalAcc": 12.6,
    "rInt": 0.0
  }
}
```

!!! note "Optional rain field"
    The `rain` object is only included when the RG-15 is enabled and connected.
```

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
      value_template: "{{ value_json.environment.temperature }}"
      unit_of_measurement: "°C"

    - name: "Rain Rate"
      state_topic: "sqm/backyard"
      value_template: "{{ value_json.rain.rInt | default(0) }}"
      unit_of_measurement: "mm/hr"

    - name: "Rain Event Accumulation"
      state_topic: "sqm/backyard"
      value_template: "{{ value_json.rain.eventAcc | default(0) }}"
      unit_of_measurement: "mm"

    - binary_sensor:
      - name: "Raining"
        state_topic: "sqm/backyard"
        value_template: "{{ value_json.rain.isRaining | default(false) }}"
        payload_on: "True"
        payload_off: "False"
        device_class: moisture
```

---

## Mosquitto Test

Verify data is publishing:

```bash
mosquitto_sub -h 192.168.1.10 -t "sqm/#" -v
```
