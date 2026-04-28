# Tests

## Frontend Tests

Uses Vitest + @testing-library/preact.

```bash
cd web
npm test             # run once
npm run test:watch   # watch mode
npm run test:coverage
```

## Backend Tests

Uses PlatformIO + Unity test framework.

### Run on native platform (no hardware required — pure calculation tests):

```bash
pio test --environment native
```

### Run on hardware (ESP32):

```bash
pio test --environment esp32dev --upload-port /dev/cu.usbserial-*
```
