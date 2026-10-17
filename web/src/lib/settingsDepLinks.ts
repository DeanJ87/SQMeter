import type { Config } from '../types';
import type { DepEntry, Unmet } from './settingsDeps';
import { REASONS } from './settingsDepReasons';

// The building blocks of lib/settingsDeps.ts: dependency links, three-valued
// logic and reading settings with the device's defaults.

// One link of a dependency chain: met, or the catalogue ID and reason that
// make the setting inactive. `met === null`: unknown (no facts yet).
export type Link = [id: string, reason: string, met: boolean | null];

// Three-valued logic for links whose facts may be unknown (null).
export const and = (...values: (boolean | null)[]): boolean | null => {
  if (values.includes(false)) return false;
  return values.includes(null) ? null : true;
};
export const or = (...values: (boolean | null)[]): boolean | null => {
  if (values.includes(true)) return true;
  return values.includes(null) ? null : false;
};

// A setting from the config, or the device's default when the config
// doesn't carry it (older firmware, partial mocks).
export const read = <T>(config: Config, path: string, fallback: T): T => {
  let value: unknown = config;
  for (const key of path.split('.')) value = value && typeof value === 'object' ? (value as Record<string, unknown>)[key] : undefined;
  return value === undefined || value === null ? fallback : (value as T);
};
export const on = (config: Config, path: string, fallback = false) => read(config, path, fallback);

export type Shown = { unmet?: Unmet; neutral?: boolean };
export const NEUTRAL: Shown = { neutral: true };
export const FAIL_SAFE: Shown = { unmet: 'fail-safe' };

// The first unmet link makes an entry inactive; a link that's unknown makes
// it unknown unless a later one is unmet.
export const decide = (entry: DepEntry, chain: Link[]) => {
  entry.state = 'active';
  for (const [linkId, reason, met] of chain) {
    if (met === null) entry.state = 'unknown';
    if (met !== false) continue;
    Object.assign(entry, { state: 'inactive', id: linkId, reason, ...REASONS[reason] });
    return;
  }
};

// While off: what would make it inactive if it were switched on.
export const blocker = (chain: Link[]) => {
  const unmet = chain.find(([, , met]) => met === false);
  return unmet ? { id: unmet[0], reason: unmet[1], ...REASONS[unmet[1]] } : undefined;
};

// The battery events and their default levels (spec 032); older firmware has none.
export const BATTERY_EVENT_DEFAULTS: Record<string, number> = {
  battery_low: 2,
  battery_critical: 4,
  battery_shutdown: 4,
  power_restored: 0,
  charge_complete: 0,
  charge_fault: 2,
};
