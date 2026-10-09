import type { Config } from '../../types';
import { t } from '../../i18n';

const changed = (a: unknown, b: unknown) => JSON.stringify(a) !== JSON.stringify(b);

// Settings the firmware only reads at boot. Everything else is applied as
// soon as it's saved (see saveConfigCallback in src/main.cpp).
export const restartReasons = (before: Config, after: Config): string[] => {
  const reasons: string[] = [];
  if (
    changed([before.wifi.ssid, before.wifi.password, before.wifi.hostname], [after.wifi.ssid, after.wifi.password, after.wifi.hostname])
  ) {
    reasons.push('WiFi');
  }
  if (after.ota.enabled && changed(before.ota, after.ota)) reasons.push(t('settings.restart.commandLineUploads'));
  if (changed(before.gps, after.gps)) reasons.push('GPS');
  if (
    changed(
      [before.sensor.i2cSDA, before.sensor.i2cSCL, before.sensor.i2cFrequency],
      [after.sensor.i2cSDA, after.sensor.i2cSCL, after.sensor.i2cFrequency],
    )
  ) {
    reasons.push('I2C');
  }
  if ((before.alpaca?.enabled ?? false) !== (after.alpaca?.enabled ?? false)) reasons.push(t('settings.restart.alpacaDiscovery'));
  if (
    changed([before.ble?.enabled, before.ble?.passkey], [after.ble?.enabled, after.ble?.passkey]) ||
    (after.ble?.enabled && before.deviceName !== after.deviceName)
  ) {
    reasons.push(t('settings.restart.bluetooth'));
  }
  return reasons;
};

export const listReasons = (reasons: string[]) =>
  reasons.length <= 1 ? reasons.join('') : `${reasons.slice(0, -1).join(', ')} and ${reasons[reasons.length - 1]}`;
