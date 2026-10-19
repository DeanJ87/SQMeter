import { describe, it, expect } from 'vitest';
import { getConfigValidationErrors } from '../validation/configSchema';
import { mockConfig } from '../mocks/data';
import type { BatteryConfig, Config } from '../types';

// Spec 032: the battery settings, with the device's messages (D-51 for the pins).
describe('battery settings', () => {
  const withBattery = (battery: Partial<BatteryConfig>, extra: Partial<Config> = {}) =>
    getConfigValidationErrors({ ...mockConfig, ...extra, battery: { ...mockConfig.battery!, ...battery } });

  it('accepts the defaults and a config without a battery section', () => {
    expect(withBattery({})).toEqual({});
    const { battery: _battery, ...older } = mockConfig;
    expect(getConfigValidationErrors(older as Config)).toEqual({});
  });
  it('keeps low > critical > shutdown', () => {
    expect(withBattery({ criticalPercent: 20 })['battery.criticalPercent']).toBe('2 to 25%, below the low level');
    expect(withBattery({ shutdownPercent: 10 })['battery.shutdownPercent']).toBe('2 to 10%, below the critical level');
    expect(withBattery({ lowPercent: 51 })['battery.lowPercent']).toBe('5 to 50%');
  });
  it('capacity is empty (0) or 500..50000 mAh', () => {
    expect(withBattery({ capacityMah: 0 })).toEqual({});
    expect(withBattery({ capacityMah: 7000 })).toEqual({});
    expect(withBattery({ capacityMah: 300 })['battery.capacityMah']).toBe('500 to 50000 mAh, or empty if unknown');
  });
  it('D-51: pins are valid, different, all three charger pins or none, free, PG can wake', () => {
    expect(withBattery({ stat1Pin: 13, stat2Pin: 14, pgPin: 32 })).toEqual({});
    expect(withBattery({ alertPin: 7 })['battery.alertPin']).toBe('Battery: pins must be valid and different');
    expect(withBattery({ stat1Pin: 13, stat2Pin: 13, pgPin: 32 })['battery.stat1Pin']).toBe('Battery: pins must be valid and different');
    expect(withBattery({ stat1Pin: 13 })['battery.stat2Pin']).toBe('Battery: set all three charger pins or none');
    expect(withBattery({ alertPin: 21 })['battery.alertPin']).toBe('Battery: pin is already in use');
    // mockConfig has the anemometer on 27.
    expect(withBattery({ alertPin: 27 })['battery.alertPin']).toBe('Battery: pin is already in use');
    expect(withBattery({ stat1Pin: 13, stat2Pin: 14, pgPin: 23 })['battery.pgPin']).toBe(
      'Battery: the USB power pin must be able to wake the device',
    );
  });
  it('safety battery limit is 2 to 50%', () => {
    expect(
      getConfigValidationErrors({ ...mockConfig, alpaca: { ...mockConfig.alpaca!, batteryMinPercent: 1 } })['alpaca.batteryMinPercent'],
    ).toBe('2 to 50%');
  });
});
