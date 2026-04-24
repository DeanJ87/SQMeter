# Adding Sensors

All sensors implement `SensorBase`. Adding a new one takes three files: a header, an implementation, and wiring it into `main.cpp`.

---

## 1. Header (`include/sensors/MySensor.h`)

```cpp
#pragma once
#include "sensors/SensorBase.h"

class MySensor : public SensorBase {
public:
    bool begin() override;
    void update() override;
    std::string getName() const override { return "mysensor"; }
    std::string toJson() const override;

private:
    float _value = 0.0f;
};
```

---

## 2. Implementation (`src/sensors/MySensor.cpp`)

```cpp
#include "sensors/MySensor.h"

bool MySensor::begin() {
    // initialise hardware, return false on failure
    return true;
}

void MySensor::update() {
    // read hardware into _value
}

std::string MySensor::toJson() const {
    char buf[64];
    snprintf(buf, sizeof(buf), R"({"value":%.4f})", _value);
    return buf;
}
```

---

## 3. Wire into `main.cpp`

```cpp
#include "sensors/MySensor.h"

MySensor mySensor;

void setup() {
    if (!mySensor.begin()) {
        Logger::error("MySensor init failed");
    }
}

void loop() {
    mySensor.update();
}
```

---

## SensorBase Interface

| Method | Required | Description |
|--------|----------|-------------|
| `begin()` | Yes | Init hardware, return `false` on failure |
| `update()` | Yes | Poll hardware, store latest reading |
| `getName()` | Yes | Key name used in JSON output |
| `toJson()` | Yes | Serialise current reading to JSON string |

---

## Adding to WebSocket Broadcast

Include your sensor's JSON in the broadcast payload in `WebServer.cpp` alongside the existing sensors.
