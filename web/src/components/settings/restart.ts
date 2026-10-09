import type { Config } from '../../types';
import { currentLanguage, t } from '../../i18n';

const changed = (a: unknown, b: unknown) => JSON.stringify(a) !== JSON.stringify(b);

// WiFiManager takes its settings at boot (src/main.cpp doesn't pass a
// saved config on), so every WiFi field needs a restart.
const wifiFields = (c: Config) => [
  c.wifi.ssid,
  c.wifi.password,
  c.wifi.hostname,
  c.wifi.mdns,
  c.wifi.autoReconnect,
  c.wifi.reconnectDelayMs,
  c.wifi.maxReconnectDelayMs,
];
const i2cFields = (c: Config) => [c.sensor.i2cSDA, c.sensor.i2cSCL, c.sensor.i2cFrequency];
const ipv6On = (c: Config) => c.wifi.ipv6 ?? true;
const alpacaOn = (c: Config) => c.alpaca?.enabled ?? false;
const bleChanged = (before: Config, after: Config) =>
  changed([before.ble?.enabled, before.ble?.passkey], [after.ble?.enabled, after.ble?.passkey]) ||
  Boolean(after.ble?.enabled && before.deviceName !== after.deviceName);

// Each boot-time setting and its name in the "Restart to apply" note, in order.
const BOOT_SETTINGS: [(before: Config, after: Config) => boolean, () => string][] = [
  [(before, after) => changed(wifiFields(before), wifiFields(after)), () => 'WiFi'],
  [(before, after) => after.ota.enabled && changed(before.ota, after.ota), () => t('settings.restart.commandLineUploads')],
  [(before, after) => changed(before.gps, after.gps), () => 'GPS'],
  [(before, after) => changed(i2cFields(before), i2cFields(after)), () => 'I2C'],
  [(before, after) => ipv6On(before) !== ipv6On(after), () => 'IPv6'],
  [(before, after) => alpacaOn(before) !== alpacaOn(after), () => t('settings.restart.alpacaDiscovery')],
  [bleChanged, () => t('settings.restart.bluetooth')],
];

// Settings the firmware only reads at boot. Everything else is applied as
// soon as it's saved (see saveConfigCallback in src/main.cpp).
export const restartReasons = (before: Config, after: Config): string[] =>
  BOOT_SETTINGS.filter(([needsRestart]) => needsRestart(before, after)).map(([, name]) => name());

// "a, b and c" in the UI's language (British English: no comma before "and").
export const listReasons = (reasons: string[]) =>
  typeof Intl.ListFormat === 'function'
    ? new Intl.ListFormat(currentLanguage() === 'en' ? 'en-GB' : currentLanguage(), { type: 'conjunction' }).format(reasons)
    : reasons.join(t('common.listSeparator'));
