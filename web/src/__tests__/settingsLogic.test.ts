import { describe, it, expect } from 'vitest';
import { deriveHardware } from '../components/settings/hardware';
import { tabForErrorPath, tabFromLocation } from '../components/settings/tabs';
import { toConfigPayload } from '../components/settings/payload';
import { listReasons, restartReasons } from '../components/settings/restart';
import { parseCoordinates } from '../components/settings/TimeTab';
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
  });

  it('reports undetected I2C sensors', () => {
    const status = { ...mockStatus, sensors: { ...mockStatus.sensors!, infrared: { status: 'missing' as const, ageMs: 0 } } };
    const hw = deriveHardware(config, status);
    expect(hw.irSky.detected).toBe(false);
  });

  it('follows the form, not the device, for optional sensors', () => {
    const hw = deriveHardware({ ...config, rain: { ...config.rain!, enabled: false } }, mockStatus);
    expect(hw.rain.enabled).toBe(false);
  });

  it('only judges rain sensor health when the device is running it', () => {
    const status = { ...mockStatus, sensors: { ...mockStatus.sensors!, rain: undefined } };
    expect(deriveHardware(config, status).rain.detected).toBeNull();
  });
});

describe('restartReasons', () => {
  const base = toConfigPayload(mockConfig);

  it('is empty for settings applied live', () => {
    const next = { ...base, mqtt: { ...base.mqtt, topic: 'x' }, rain: { ...base.rain!, enabled: !base.rain!.enabled } };
    expect(restartReasons(base, next)).toEqual([]);
  });

  it('lists boot-time settings that changed', () => {
    const next = {
      ...base,
      alpaca: { ...base.alpaca!, enabled: !base.alpaca!.enabled },
      sensor: { ...base.sensor, i2cFrequency: 400000 },
      ble: { ...base.ble!, enabled: true },
    };
    expect(restartReasons(base, next)).toEqual(['I2C', 'Alpaca discovery', 'Bluetooth']);
    expect(listReasons(['I2C', 'Alpaca discovery', 'Bluetooth'])).toBe('I2C, Alpaca discovery and Bluetooth');
  });

  // The WiFi manager reads all its settings at boot (spec 011 FR-003).
  it.each([['mdns', false], ['autoReconnect', false], ['reconnectDelayMs', 2000], ['maxReconnectDelayMs', 600000]] as const)(
    'needs a restart for wifi.%s',
    (key, value) => {
      expect(restartReasons(base, { ...base, wifi: { ...base.wifi, [key]: value } })).toEqual(['WiFi']);
    },
  );
});

describe('parseCoordinates', () => {
  it('accepts "lat, lon" as pasted from a maps app', () => {
    expect(parseCoordinates('51.4779, -0.0015')).toEqual([51.4779, -0.0015]);
    expect(parseCoordinates('  -33.87 151.21 ')).toEqual([-33.87, 151.21]);
  });

  it('rejects anything else', () => {
    expect(parseCoordinates('London')).toBeNull();
    expect(parseCoordinates('91, 0')).toBeNull();
    expect(parseCoordinates('51.5')).toBeNull();
  });
});
