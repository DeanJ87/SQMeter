import type { Config } from '../../types';

const changed = (a: unknown, b: unknown) => JSON.stringify(a) !== JSON.stringify(b);

// Settings the firmware only reads at boot. Everything else is applied as
// soon as it's saved (see saveConfigCallback in src/main.cpp).
export const restartReasons = (before: Config, after: Config): string[] => {
  const reasons: string[] = [];
  // WiFiManager takes its settings at boot (src/main.cpp doesn't pass a
  // saved config on), so every WiFi field needs a restart.
  const wifi = (c: Config) => [
    c.wifi.ssid,
    c.wifi.password,
    c.wifi.hostname,
    c.wifi.mdns,
    c.wifi.autoReconnect,
    c.wifi.reconnectDelayMs,
    c.wifi.maxReconnectDelayMs,
  ];
  if (changed(wifi(before), wifi(after))) {
    reasons.push('WiFi');
  }
  if (after.ota.enabled && changed(before.ota, after.ota)) reasons.push('command-line uploads');
  if (changed(before.gps, after.gps)) reasons.push('GPS');
  if (
    changed(
      [before.sensor.i2cSDA, before.sensor.i2cSCL, before.sensor.i2cFrequency],
      [after.sensor.i2cSDA, after.sensor.i2cSCL, after.sensor.i2cFrequency],
    )
  ) {
    reasons.push('I2C');
  }
  if ((before.alpaca?.enabled ?? false) !== (after.alpaca?.enabled ?? false)) reasons.push('Alpaca discovery');
  if (
    changed([before.ble?.enabled, before.ble?.passkey], [after.ble?.enabled, after.ble?.passkey]) ||
    (after.ble?.enabled && before.deviceName !== after.deviceName)
  ) {
    reasons.push('Bluetooth');
  }
  return reasons;
};

export const listReasons = (reasons: string[]) =>
  reasons.length <= 1 ? reasons.join('') : `${reasons.slice(0, -1).join(', ')} and ${reasons[reasons.length - 1]}`;
