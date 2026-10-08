import { sunPosition } from '../lib/astro';

// Simulated sensor inputs for the demo's emulated device. Only the raw
// readings are invented here; everything derived from them (SQM, cloud cover,
// the safety verdict, alerts) is the firmware's own code in the device core.

export type ScenarioId = 'night' | 'rain' | 'cloud' | 'clear' | 'fail-light' | 'fail-ir' | 'fail-environment' | 'fail-rain' | 'dawn';

export interface Scenario {
  id: ScenarioId;
  startedAtMs: number; // device clock (demo time)
}

export const SCENARIOS: { id: ScenarioId; label: string; hint: string }[] = [
  { id: 'night', label: 'Night sky', hint: 'A dark, clear sky for half an hour, whatever the time of day (the device still knows the real sun)' },
  { id: 'rain', label: 'Rain', hint: 'A shower: rain, then the rain clear delay' },
  { id: 'cloud', label: 'Cloud over', hint: 'Cloud rolls in until it is overcast' },
  { id: 'clear', label: 'Clear', hint: 'Back to a clear, dark sky' },
  { id: 'dawn', label: 'Dawn', hint: 'The sky brightens as if the sun were rising' },
  { id: 'fail-light', label: 'Light sensor fails', hint: 'The TSL2591 stops responding for a while' },
  { id: 'fail-ir', label: 'IR sensor fails', hint: 'The MLX90614 stops responding for a while' },
  { id: 'fail-environment', label: 'BME280 fails', hint: 'Temperature and humidity stop for a while' },
  { id: 'fail-rain', label: 'Rain sensor fails', hint: 'The RG-15 stops answering for a while' },
];

// How long each scenario lasts in demo time before the sky goes back to baseline.
const DURATION_MS: Record<ScenarioId, number> = {
  night: 30 * 60_000,
  rain: 4 * 60_000,
  cloud: 10 * 60_000,
  clear: 60_000,
  dawn: 6 * 60_000,
  'fail-light': 3 * 60_000,
  'fail-ir': 3 * 60_000,
  'fail-environment': 3 * 60_000,
  'fail-rain': 3 * 60_000,
};

// Where the demo's sky is, when the settings have no location (London).
const DEFAULT_LOCATION = { latitude: 51.5074, longitude: -0.1278 };

export interface SimulatorSettings {
  location?: { set: boolean; latitude: number; longitude: number };
  gpsEnabled: boolean;
}

export const scenarioActive = (scenario: Scenario | null, nowMs: number) =>
  scenario !== null && nowMs - scenario.startedAtMs < DURATION_MS[scenario.id];

export const scenarioRemainingMs = (scenario: Scenario | null, nowMs: number) =>
  scenario ? Math.max(0, DURATION_MS[scenario.id] - (nowMs - scenario.startedAtMs)) : 0;

// Smooth 0..1 ramp over `ms`.
const ramp = (elapsed: number, ms: number) => Math.min(1, Math.max(0, elapsed / ms));

// A little steady variation so the readings look alive.
const wobble = (nowMs: number, periodS: number, amplitude: number) => Math.sin((nowMs / 1000 / periodS) * 2 * Math.PI) * amplitude;

// Illuminance from the sun's altitude: daylight, twilight on a log scale, then a dark sky.
export const skyLux = (sunAltitudeDeg: number) => {
  if (sunAltitudeDeg > 0) return 400 + 100_000 * Math.sin((sunAltitudeDeg * Math.PI) / 180);
  if (sunAltitudeDeg > -18) {
    const t = -sunAltitudeDeg / 18; // 0 at sunset, 1 at astronomical dark
    return Math.pow(10, Math.log10(400) * (1 - t) + Math.log10(0.00028) * t);
  }
  return 0.00028;
};

export function simulate(nowMs: number, now: Date, settings: SimulatorSettings, scenario: Scenario | null) {
  const where = settings.location?.set ? settings.location : DEFAULT_LOCATION;
  const active = scenarioActive(scenario, nowMs) ? scenario : null;
  // "Night sky" lights the sensors as if the sun were well below the horizon.
  const sun = active?.id === 'night' ? -30 : sunPosition(now, where.latitude, where.longitude).altitude;
  const elapsed = active ? nowMs - active.startedAtMs : 0;

  // Cloud amount 0 (clear) .. 1 (overcast).
  let cloud = 0.05 + wobble(nowMs, 300, 0.03);
  if (active?.id === 'cloud') cloud = 0.05 + 0.92 * ramp(elapsed, 3 * 60_000);
  if (active?.id === 'rain') cloud = 0.97;

  let lux = skyLux(sun) * (1 + cloud * 0.5) * (1 + wobble(nowMs, 47, 0.04));
  if (active?.id === 'dawn') lux = Math.max(lux, 0.00028 * Math.pow(10, 6 * ramp(elapsed, 5 * 60_000)));
  const counts = Math.min(65535, Math.round(Math.max(lux, 0.0001) * 1_000_000));

  const ambient = 11 + 5 * Math.sin((sun * Math.PI) / 180) + wobble(nowMs, 900, 0.3);
  const humidity = Math.min(98, 62 + 20 * cloud - 8 * Math.sin((sun * Math.PI) / 180) + wobble(nowMs, 600, 1.5));
  const skyTemperature = ambient - 38 * (1 - cloud) - 2 + wobble(nowMs, 120, 0.4);

  const rainRate = active?.id === 'rain' ? (elapsed < 2.5 * 60_000 ? 2.4 + wobble(nowMs, 20, 0.6) : 0) : 0;
  const speed = Math.max(0, 3 + (active?.id === 'rain' ? 4 : 0) + wobble(nowMs, 90, 1.2));

  return {
    light: { present: true, failed: active?.id === 'fail-light', lux, visible: Math.round(counts * 0.86), infrared: Math.round(counts * 0.14), full: counts, nightMode: lux < 0.5 },
    environment: { present: true, failed: active?.id === 'fail-environment', temperature: ambient, humidity, pressure: 1013 + wobble(nowMs, 3600, 2) },
    infrared: { present: true, failed: active?.id === 'fail-ir', sky: skyTemperature, ambient: ambient + 0.4 },
    gps: settings.gpsEnabled ? { fix: true, latitude: where.latitude, longitude: where.longitude, altitude: 42, satellites: 9 } : { fix: false },
    rain: { failed: active?.id === 'fail-rain', rate: Math.max(0, rainRate) },
    wind: { speed, gust: speed * 1.8 + 0.5, direction: (240 + wobble(nowMs, 200, 25) + 360) % 360 },
  };
}
