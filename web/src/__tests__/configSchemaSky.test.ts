import { describe, it, expect } from 'vitest';
import { getConfigValidationErrors } from '../validation/configSchema';
import { mockConfig } from '../mocks/data';
import type { Config } from '../types';

// Spec 030 D-39: the second I2C bus and the SQM-L, with the device's messages.
describe('second I2C bus and SQM-L', () => {
  const withSky = (sensor: Partial<Config['sensor']>, sqmlEnabled = false, extra: Partial<Config> = {}) =>
    getConfigValidationErrors({
      ...mockConfig,
      ...extra,
      sensor: { ...mockConfig.sensor, ...sensor },
      sky: { sqmEnabled: true, sqmlEnabled, authoritative: 'sqm', showOther: true },
    });

  it('accepts both pins unset or two free pins', () => {
    expect(withSky({ i2c2SDA: 255, i2c2SCL: 255 })).toEqual({});
    expect(withSky({ i2c2SDA: 25, i2c2SCL: 26 }, true)).toEqual({});
  });
  it('rejects half-set, equal or invalid pins', () => {
    for (const pins of [
      { i2c2SDA: 25, i2c2SCL: 255 },
      { i2c2SDA: 25, i2c2SCL: 25 },
      { i2c2SDA: 6, i2c2SCL: 25 },
    ])
      expect(withSky(pins)['sensor.i2c2SDA']).toBe('Second I2C bus pins are invalid');
  });
  it('rejects pins another bus uses', () => {
    expect(withSky({ i2c2SDA: 21, i2c2SCL: 26 })['sensor.i2c2SDA']).toBe('Second I2C bus pins are already in use');
    const gps = { ...mockConfig.gps, enabled: true, rxPin: 17, txPin: 16 };
    expect(withSky({ i2c2SDA: 17, i2c2SCL: 26 }, false, { gps })['sensor.i2c2SDA']).toBe('Second I2C bus pins are already in use');
  });
  it('needs the pins for the SQM-L', () => {
    expect(withSky({ i2c2SDA: 255, i2c2SCL: 255 }, true)['sky.sqmlEnabled']).toBe('SQM-L needs the second I2C bus pins');
  });
});
