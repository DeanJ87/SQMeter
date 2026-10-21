import type { Config, SensorData, SensorHealth, SkyRole, SystemStatus } from '../types';

// Two sky sensors on the dashboard (spec 030 FR-010, FR-012, contracts/ui.md).
// The Sky Quality card shows the authoritative reading; with both sensors on
// it carries that sensor's name, and the other reading and the difference when
// "Show other reading" is on and the other sensor reports. A missing
// authoritative sensor is named, with a switch to the other; the device never
// falls back on its own. With one sensor (every existing device) nothing here
// changes the card.

export const otherSky = (role: SkyRole): SkyRole => (role === 'sqm' ? 'sqml' : 'sqm');

// Above this the two readings disagree enough to look at (spec edge case).
export const DIFFERENCE_WARN = 1.5;

export interface SkyChoice {
  enabled: Record<SkyRole, boolean>;
  authoritative: SkyRole;
  showOther: boolean;
  both: boolean;
  // The SQM-L is in play (on, or still chosen): names are shown.
  named: boolean;
}

/** The sky settings, with the device's defaults for firmware that has none. */
export const skyChoice = (config: Config | null): SkyChoice => {
  const sky = config?.sky;
  const enabled = { sqm: sky?.sqmEnabled ?? true, sqml: sky?.sqmlEnabled ?? false };
  const authoritative = sky?.authoritative ?? 'sqm';
  return {
    enabled,
    authoritative,
    showOther: sky?.showOther ?? true,
    both: enabled.sqm && enabled.sqml,
    named: enabled.sqml || authoritative === 'sqml',
  };
};

export interface OtherReading {
  role: SkyRole;
  sqm: number;
  difference?: number; // SQM-L - SQM
  warn: boolean;
}

// SQM-L - SQM: the device's, or worked out from the two readings.
const differenceOf = (sensors: SensorData): number | undefined => {
  const { sqm, sqml, deltaSqm } = sensors.skySensors ?? {};
  if (typeof deltaSqm === 'number') return deltaSqm;
  return typeof sqm?.sqm === 'number' && typeof sqml?.sqm === 'number' ? sqml.sqm - sqm.sqm : undefined;
};

/** The non-authoritative reading, when the card should show it. */
export const otherReading = (sensors: SensorData, config: Config | null): OtherReading | null => {
  const choice = skyChoice(config);
  if (!choice.both || !choice.showOther) return null;
  const role = otherSky(choice.authoritative);
  const entry = sensors.skySensors?.[role];
  if (entry?.status !== 'ok' || typeof entry.sqm !== 'number') return null;
  const difference = differenceOf(sensors);
  return { role, sqm: entry.sqm, difference, warn: difference !== undefined && Math.abs(difference) > DIFFERENCE_WARN };
};

export interface SkyMissing {
  role: SkyRole; // the authoritative sensor
  health: SensorHealth | 'off';
  switchTo: SkyRole | null; // the other sensor, when it is on and reporting
}

/** Why there is no sky reading, naming the sensor, when the SQM-L is in play. */
export const authoritativeMissing = (sensors: SensorData, config: Config | null): SkyMissing | null => {
  const choice = skyChoice(config);
  if (!choice.named) return null;
  const role = choice.authoritative;
  const health: SensorHealth | 'off' = choice.enabled[role]
    ? (sensors.skySensors?.[role]?.status ?? sensors.sky?.status ?? 'missing')
    : 'off';
  if (health === 'ok') return null;
  const other = otherSky(role);
  const switchTo = choice.enabled[other] && sensors.skySensors?.[other]?.status === 'ok' ? other : null;
  return { role, health, switchTo };
};

/**
 * The non-authoritative sensor once it has failed: it keeps a fault card
 * (spec 025 D6). One that hasn't answered since boot ("missing") has none.
 */
export const otherSkyFault = (
  sensors: SensorData,
  status: SystemStatus | null,
  config: Config | null,
): { role: SkyRole; health: SensorHealth } | null => {
  const choice = skyChoice(config);
  if (!choice.both) return null;
  const role = otherSky(choice.authoritative);
  const health = status?.skySensors?.[role]?.status ?? sensors.skySensors?.[role]?.status;
  return health === 'error' || health === 'stale' ? { role, health } : null;
};

export interface SqmHistory {
  source: SkyRole;
  values: number[];
}

/** The trend under the big value: the last 24 readings, restarted when the authoritative sensor changes. */
export const nextSqmHistory = (history: SqmHistory, source: SkyRole, sqm: number | undefined): SqmHistory => {
  const kept = history.source === source ? history.values : [];
  if (typeof sqm !== 'number' || !Number.isFinite(sqm)) return { source, values: kept };
  return { source, values: [...kept.slice(-23), sqm] };
};
