import { describe, it, expect } from 'vitest';
import { deriveHardware, unavailableReason } from '../components/settings/hardware';
import { tabForErrorPath, tabFromLocation } from '../components/settings/tabs';
import { toConfigPayload } from '../components/settings/payload';
import { mockConfig, mockStatus } from '../mocks/data';

describe('tabFromLocation', () => {
  it('opens the requested tab', () => {
    expect(tabFromLocation('?tab=alerts')).toEqual({ tab: 'alerts', anchor: undefined });
  });

  it('maps the Alpaca setup link onto the Safety tab', () => {
    expect(tabFromLocation('?section=alpaca')).toEqual({ tab: 'safety', anchor: 'alpaca' });
  });

  it('falls back to Device for unknown values', () => {
    expect(tabFromLocation('?tab=bogus').tab).toBe('device');
    expect(tabFromLocation('').tab).toBe('device');
  });
});

describe('tabForErrorPath', () => {
  it('assigns validation errors to the tab that shows the field', () => {
    expect(tabForErrorPath('alerts.ntfy.topic')).toBe('alerts');
    expect(tabForErrorPath('alpaca.windGustUnsafeMs')).toBe('safety');
    expect(tabForErrorPath('rain.rxPin')).toBe('sensors');
    expect(tabForErrorPath('mqtt.broker')).toBe('network');
    expect(tabForErrorPath('primaryTimeSource')).toBe('time');
    expect(tabForErrorPath('deviceName')).toBe('device');
  });
});

describe('deriveHardware', () => {
  const config = toConfigPayload(mockConfig);

  it('treats sensors as unknown until status loads', () => {
    const hw = deriveHardware(config, null);
    expect(hw.statusLoaded).toBe(false);
    expect(hw.irSky.detected).toBeNull();
    expect(unavailableReason(hw.irSky, 'MLX', 'wire')).toBeNull();
  });

  it('reports undetected I2C sensors', () => {
    const status = { ...mockStatus, sensors: { ...mockStatus.sensors!, mlx90614: { initialized: false, status: 1, lastUpdate: 0 } } };
    const hw = deriveHardware(config, status);
    expect(hw.irSky.detected).toBe(false);
    expect(unavailableReason(hw.irSky, 'The MLX90614', 'wire')).toMatch(/wasn't detected/);
  });

  it('follows the form, not the device, for optional sensors', () => {
    const hw = deriveHardware({ ...config, rain: { ...config.rain!, enabled: false } }, mockStatus);
    expect(hw.rain.enabled).toBe(false);
    expect(unavailableReason(hw.rain, 'The rain sensor', 'enable')).toBe('The rain sensor is turned off.');
  });

  it('only judges rain sensor health when the device is running it', () => {
    const status = { ...mockStatus, sensors: { ...mockStatus.sensors!, rg15: { ...mockStatus.sensors!.rg15!, enabled: false } } };
    expect(deriveHardware(config, status).rain.detected).toBeNull();
  });
});
