import { describe, expect, it } from 'vitest';
import { advanceRamps, DEFAULT_CONDITIONS, differential, toCoreInputs, withDifferential, withInput } from '../conditions';

const context = { sunLux: 0.0003, gps: { enabled: false, latitude: 51.5, longitude: -0.1, altitude: 42 } };

describe('conditions', () => {
  it('changes only the input that was set', () => {
    const { conditions, clamped } = withInput(DEFAULT_CONDITIONS, 'air.temperature', 20);
    expect(clamped).toBe(false);
    expect(conditions.air.temperature).toBe(20);
    expect({ ...conditions, air: DEFAULT_CONDITIONS.air }).toEqual(DEFAULT_CONDITIONS);
    expect(DEFAULT_CONDITIONS.air.temperature).toBe(11); // not mutated
  });

  it('clamps to the sensor range and says so', () => {
    expect(withInput(DEFAULT_CONDITIONS, 'air.humidity', 140)).toMatchObject({ clamped: true, conditions: { air: { humidity: 100 } } });
    expect(withInput(DEFAULT_CONDITIONS, 'ir.sky', -90).conditions.ir.sky).toBe(-70);
    expect(withInput(DEFAULT_CONDITIONS, 'rain.rate', -1).conditions.rain.rate).toBe(0);
  });

  it('moves the sky temperature for a differential, keeping the IR sensor temperature', () => {
    const { conditions } = withDifferential(DEFAULT_CONDITIONS, -32);
    expect(conditions.ir.ambient).toBe(DEFAULT_CONDITIONS.ir.ambient);
    expect(differential(conditions)).toBeCloseTo(-32);
  });

  it('setting illuminance stops following the sun', () => {
    expect(withInput(DEFAULT_CONDITIONS, 'light.lux', 0.002).conditions.light.mode).toBe('set');
  });

  it('ramps on the device clock and drops finished ramps', () => {
    const ramp = { field: 'ir.sky' as const, from: -20, to: 0, startMs: 1000, durationMs: 40_000 };
    const half = advanceRamps(DEFAULT_CONDITIONS, [ramp], 21_000);
    expect(half.conditions.ir.sky).toBeCloseTo(-10);
    expect(half.ramps).toHaveLength(1);
    const done = advanceRamps(DEFAULT_CONDITIONS, [ramp], 60_000);
    expect(done.conditions.ir.sky).toBe(0);
    expect(done.ramps).toHaveLength(0);
  });

  it('ramps light on a log scale', () => {
    const ramp = { field: 'light.lux' as const, from: 0.001, to: 0.1, startMs: 0, durationMs: 10_000 };
    expect(advanceRamps(DEFAULT_CONDITIONS, [ramp], 5000).conditions.light.lux).toBeCloseTo(0.01, 5);
  });

  it('feeds the core exactly the inputs when held steady', () => {
    const steady = { ...DEFAULT_CONDITIONS, steady: true, faults: { ...DEFAULT_CONDITIONS.faults, infrared: true } };
    const inputs = toCoreInputs(steady, 123_456, context);
    expect(inputs.environment.temperature).toBe(11);
    expect(inputs.infrared).toMatchObject({ failed: true, sky: -10, ambient: 11.4 });
    expect(inputs.light.lux).toBeCloseTo(0.0003);
    expect(inputs.light.nightMode).toBe(true);
    expect(inputs.gps).toEqual({ fix: false });
  });

  it('reports the GPS position and altitude, or a GPS that stopped answering', () => {
    const on = { ...context, gps: { ...context.gps, enabled: true, altitude: 2396 } };
    expect(toCoreInputs(DEFAULT_CONDITIONS, 0, on).gps).toMatchObject({ fix: true, latitude: 51.5, altitude: 2396 });
    const silent = { ...DEFAULT_CONDITIONS, faults: { ...DEFAULT_CONDITIONS.faults, gps: true } };
    expect(toCoreInputs(silent, 0, on).gps).toEqual({ failed: true, fix: false });
  });

  it('keeps natural variation small', () => {
    for (let t = 0; t < 600_000; t += 7_000) {
      const inputs = toCoreInputs(DEFAULT_CONDITIONS, t, context);
      expect(Math.abs(inputs.infrared.sky - DEFAULT_CONDITIONS.ir.sky)).toBeLessThanOrEqual(0.4 + 1e-9);
      expect(inputs.wind.gust).toBeGreaterThanOrEqual(inputs.wind.speed);
    }
  });
});
