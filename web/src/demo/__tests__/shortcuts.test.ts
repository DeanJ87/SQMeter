import { describe, expect, it } from 'vitest';
import { DEFAULT_CONDITIONS, type Conditions } from '../conditions';
import { DARK_SKY_SQM, shortcut, type ShortcutConfig } from '../shortcuts';

// The device's own formulas (lib/SkyLogic): corrected difference, cover, SQM, dew point.
const corrected = (c: Conditions, sky: number, hc = 0.75) => sky - c.ir.ambient - (hc / 100) * c.air.humidity;
const cover = (delta: number, clear: number, cloudy: number) => Math.min(100, Math.max(0, ((delta - clear) / (cloudy - clear)) * 100));
const luxToSqm = (lux: number) => 12.6 - 2.5 * Math.log10(lux);
const dewpoint = (t: number, rh: number) => {
  const alpha = (17.27 * t) / (237.7 + t) + Math.log(rh / 100);
  return (237.7 * alpha) / (17.27 - alpha);
};

const defaults: ShortcutConfig = {
  cloudDetection: { clearSkyThreshold: -13, cloudyThreshold: -3, humidityCorrection: 0.75 },
  alpaca: { cloudCoverEnabled: true, cloudCoverUnsafePercent: 90 },
  alerts: { dewRiskMarginC: 2 },
  rain: { enabled: true },
};
const custom: ShortcutConfig = { ...defaults, cloudDetection: { clearSkyThreshold: -30, cloudyThreshold: -20, humidityCorrection: 2 } };

const sky = (result: ReturnType<typeof shortcut>) => {
  if (!result.ok) throw new Error(result.reason);
  return result.changes['ir.sky'] as number;
};

describe('cloud shortcuts follow the cloud thresholds', () => {
  it.each([
    ['defaults', defaults],
    ['clear -30 / cloudy -20', custom],
  ])('%s', (_name, config) => {
    const { clearSkyThreshold: clear = 0, cloudyThreshold: cloudy = 0, humidityCorrection: hc = 0 } = config.cloudDetection ?? {};
    expect(corrected(DEFAULT_CONDITIONS, sky(shortcut('clear', config, DEFAULT_CONDITIONS)), hc)).toBeCloseTo(clear - 3);
    expect(corrected(DEFAULT_CONDITIONS, sky(shortcut('overcast', config, DEFAULT_CONDITIONS)), hc)).toBeCloseTo(cloudy + 1);
    const unsafe = cover(corrected(DEFAULT_CONDITIONS, sky(shortcut('cloudUnsafe', config, DEFAULT_CONDITIONS)), hc), clear, cloudy);
    expect(unsafe).toBeCloseTo(93);
  });

  it('says which settings it used', () => {
    const result = shortcut('clear', custom, DEFAULT_CONDITIONS);
    expect(result.ok && result.used).toContain('your clear-sky threshold is -30.0 °C');
  });

  it('uses the assumed humidity when the BME280 is not responding', () => {
    const c = { ...DEFAULT_CONDITIONS, faults: { ...DEFAULT_CONDITIONS.faults, environment: true } };
    const result = sky(shortcut('clear', defaults, c));
    expect(result - c.ir.ambient - 0.0075 * 53).toBeCloseTo(-16);
  });

  it('reaches overcast at a 100% limit', () => {
    const config = { ...defaults, alpaca: { cloudCoverEnabled: true, cloudCoverUnsafePercent: 100 } };
    expect(cover(corrected(DEFAULT_CONDITIONS, sky(shortcut('cloudUnsafe', config, DEFAULT_CONDITIONS))), -13, -3)).toBe(100);
  });

  it('explains instead of acting when the cloud rule is off or the IR sensor is down', () => {
    const off = shortcut('cloudUnsafe', { ...defaults, alpaca: { cloudCoverEnabled: false } }, DEFAULT_CONDITIONS);
    expect(off).toMatchObject({ ok: false, reason: 'The cloud cover safety rule is off.', link: { route: '/settings?tab=safety' } });
    const faulted = { ...DEFAULT_CONDITIONS, faults: { ...DEFAULT_CONDITIONS.faults, infrared: true } };
    expect(shortcut('clear', defaults, faulted).ok).toBe(false);
  });

  it('notes when the needed sky temperature is outside the sensor range', () => {
    const extreme = { ...defaults, cloudDetection: { clearSkyThreshold: -95, cloudyThreshold: -90, humidityCorrection: 0 } };
    const result = shortcut('clear', extreme, DEFAULT_CONDITIONS);
    expect(result.ok && result.clamped).toContain("outside the sensor's range");
  });
});

describe('other shortcuts', () => {
  it('rain needs the rain sensor', () => {
    expect(shortcut('rain', defaults, DEFAULT_CONDITIONS)).toMatchObject({ ok: true, changes: { 'rain.rate': 2.5 } });
    expect(shortcut('rainStops', defaults, DEFAULT_CONDITIONS)).toMatchObject({ ok: true, changes: { 'rain.rate': 0 } });
    expect(shortcut('rain', { ...defaults, rain: { enabled: false } }, DEFAULT_CONDITIONS)).toMatchObject({ ok: false, link: { route: '/settings?tab=sensors' } });
  });

  it('dark sky inverts the device conversion, with the calibration offset', () => {
    const plain = shortcut('darkSky', defaults, DEFAULT_CONDITIONS);
    expect(plain.ok && luxToSqm(plain.changes['light.lux'] as number)).toBeCloseTo(DARK_SKY_SQM);
    const calibrated = shortcut('darkSky', { ...defaults, skyCalibration: { enabled: true, sqmOffset: 0.4 } }, DEFAULT_CONDITIONS);
    expect(calibrated.ok && luxToSqm(calibrated.changes['light.lux'] as number) + 0.4).toBeCloseTo(DARK_SKY_SQM);
  });

  it('dew risk puts the dew point inside the margin', () => {
    const result = shortcut('dewRisk', defaults, DEFAULT_CONDITIONS);
    if (!result.ok) throw new Error(result.reason);
    const gap = DEFAULT_CONDITIONS.air.temperature - dewpoint(DEFAULT_CONDITIONS.air.temperature, result.changes['air.humidity'] as number);
    expect(gap).toBeGreaterThan(0);
    expect(gap).toBeLessThan(2);
    expect(shortcut('dewRisk', { ...defaults, alerts: { dewRiskMarginC: 0 } }, DEFAULT_CONDITIONS).ok).toBe(false);
  });
});
