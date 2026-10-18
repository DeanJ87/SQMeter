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

// Each boot-time setting, by the code the device uses for it too
// (lib/ConfigBackup restartReasons, held to test/fixtures/backup/restart-cases.json).
export type RestartCode = 'wifi' | 'ota' | 'gps' | 'i2c' | 'ipv6' | 'alpaca' | 'ble';
const BOOT_SETTINGS: [RestartCode, (before: Config, after: Config) => boolean][] = [
  ['wifi', (before, after) => changed(wifiFields(before), wifiFields(after))],
  ['ota', (before, after) => after.ota.enabled && changed(before.ota, after.ota)],
  ['gps', (before, after) => changed(before.gps, after.gps)],
  ['i2c', (before, after) => changed(i2cFields(before), i2cFields(after))],
  ['ipv6', (before, after) => ipv6On(before) !== ipv6On(after)],
  ['alpaca', (before, after) => alpacaOn(before) !== alpacaOn(after)],
  ['ble', bleChanged],
];

// Each code's name in the "Restart to apply" note.
export const restartLabel = (code: RestartCode): string =>
  ({
    wifi: 'WiFi',
    ota: t('settings.restart.commandLineUploads'),
    gps: 'GPS',
    i2c: 'I2C',
    ipv6: 'IPv6',
    alpaca: t('settings.restart.alpacaDiscovery'),
    ble: t('settings.restart.bluetooth'),
  })[code];

// Settings the firmware only reads at boot. Everything else is applied as
// soon as it's saved (see saveConfigCallback in src/main.cpp).
export const restartCodes = (before: Config, after: Config): RestartCode[] =>
  BOOT_SETTINGS.filter(([, needsRestart]) => needsRestart(before, after)).map(([code]) => code);

export const restartReasons = (before: Config, after: Config): string[] => restartCodes(before, after).map(restartLabel);

// "a, b and c" in the UI's language (British English: no comma before "and").
export const listReasons = (reasons: string[]) =>
  typeof Intl.ListFormat === 'function'
    ? new Intl.ListFormat(currentLanguage() === 'en' ? 'en-GB' : currentLanguage(), { type: 'conjunction' }).format(reasons)
    : reasons.join(t('common.listSeparator'));
