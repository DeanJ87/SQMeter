import { describe, expect, it } from 'vitest';
import rangesJson from '../../../test/fixtures/config-ranges.json';
import { mockConfig } from '../mocks/data';
import { configSchema } from '../validation/configSchema';

// Setting ranges shared with the device (test/fixtures/config-ranges.json,
// spec 011 FR-004): the web UI accepts each bound and rejects just outside
// it, exactly as test/test_config_ranges does on the device.

interface Range {
  path: string;
  int?: boolean;
  min?: number;
  max?: number;
  gt?: number;
  lt?: number;
  with?: Record<string, number | boolean>;
}

const ranges = (rangesJson as { ranges: Range[] }).ranges;

const setPath = (target: Record<string, unknown>, path: string, value: unknown) => {
  const [section, key] = path.split('.');
  target[section] = { ...(target[section] as object), [key]: value };
};

const accepts = (range: Range, value: number) => {
  const candidate = structuredClone(mockConfig) as unknown as Record<string, unknown>;
  for (const [path, companion] of Object.entries(range.with ?? {})) setPath(candidate, path, companion);
  setPath(candidate, range.path, value);
  return configSchema.safeParse(candidate).success;
};

const checks = (range: Range): [number, boolean][] => {
  const step = range.int ? 1 : 0.1;
  const low: [number, boolean][] = range.gt !== undefined ? [[range.gt, false], [range.gt + step, true]] : [[range.min!, true], [range.min! - step, false]];
  const high: [number, boolean][] = range.lt !== undefined ? [[range.lt, false], [range.lt - step, true]] : [[range.max!, true], [range.max! + step, false]];
  return [...low, ...high];
};

describe('shared setting ranges (device and web agree)', () => {
  it('the base config is valid', () => {
    expect(configSchema.safeParse(mockConfig).success).toBe(true);
  });

  it.each(ranges.map((range) => [range.path, range] as const))('%s', (_path, range) => {
    for (const [value, accepted] of checks(range)) expect({ value, accepted: accepts(range, value) }).toEqual({ value, accepted });
  });
});
