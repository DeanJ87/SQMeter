// What each simulated sensor reports - the demo's inputs (spec 019). The
// visitor sets these raw readings; the device core (the firmware's own
// code) derives cloud cover, SQM, dew point, the verdict and alerts from
// them, exactly as on the roof.

export type SensorId = 'light' | 'environment' | 'infrared' | 'rain' | 'wind' | 'gps';

export interface Conditions {
  air: { temperature: number; humidity: number; pressure: number }; // BME280
  ir: { sky: number; ambient: number }; // MLX90614 object and its own temperature
  light: { mode: 'sun' | 'set'; lux: number }; // TSL2591; 'sun' follows the sun's position
  rain: { rate: number; lensFault: boolean }; // RG-15, mm/h
  wind: { speed: number; gust: number; direction: number }; // m/s, degrees
  gps: { fix: boolean };
  faults: Record<SensorId, boolean>; // "not responding"
  steady: boolean; // no natural variation
}

export type NumericInput =
  | 'air.temperature'
  | 'air.humidity'
  | 'air.pressure'
  | 'ir.sky'
  | 'ir.ambient'
  | 'light.lux'
  | 'rain.rate'
  | 'wind.speed'
  | 'wind.gust'
  | 'wind.direction';

export interface InputSpec {
  label: string;
  unit: string;
  min: number;
  max: number;
  step: number;
  sensor: SensorId;
}

// The sensors' real measuring ranges (FR-004).
export const INPUTS: Record<NumericInput, InputSpec> = {
  'air.temperature': { label: 'Air temperature', unit: '°C', min: -40, max: 85, step: 0.1, sensor: 'environment' },
  'air.humidity': { label: 'Humidity', unit: '%', min: 0, max: 100, step: 0.5, sensor: 'environment' },
  'air.pressure': { label: 'Pressure', unit: 'hPa', min: 300, max: 1100, step: 1, sensor: 'environment' },
  'ir.sky': { label: 'Sky temperature', unit: '°C', min: -70, max: 380, step: 0.1, sensor: 'infrared' },
  'ir.ambient': { label: 'IR sensor temperature', unit: '°C', min: -40, max: 125, step: 0.1, sensor: 'infrared' },
  'light.lux': { label: 'Illuminance', unit: 'lux', min: 0.0001, max: 88000, step: 0.0001, sensor: 'light' },
  'rain.rate': { label: 'Rain rate', unit: 'mm/h', min: 0, max: 150, step: 0.1, sensor: 'rain' },
  'wind.speed': { label: 'Wind speed', unit: 'm/s', min: 0, max: 60, step: 0.1, sensor: 'wind' },
  'wind.gust': { label: 'Gust', unit: 'm/s', min: 0, max: 60, step: 0.1, sensor: 'wind' },
  'wind.direction': { label: 'Direction', unit: '°', min: 0, max: 359, step: 1, sensor: 'wind' },
};

// A typical clear night (spec 019 Assumptions).
export const DEFAULT_CONDITIONS: Conditions = {
  air: { temperature: 11, humidity: 62, pressure: 1013 },
  ir: { sky: -10, ambient: 11.4 },
  light: { mode: 'sun', lux: 0.0003 },
  rain: { rate: 0, lensFault: false },
  wind: { speed: 3, gust: 5.9, direction: 240 },
  gps: { fix: true },
  faults: { light: false, environment: false, infrared: false, rain: false, wind: false, gps: false },
  steady: false,
};

export const cloneConditions = (c: Conditions): Conditions => JSON.parse(JSON.stringify(c));

export function getInput(c: Conditions, field: NumericInput): number {
  const [group, key] = field.split('.') as [keyof Conditions, string];
  return (c[group] as unknown as Record<string, number>)[key];
}

/** A copy with one input changed (FR-002), clamped to the sensor's range (FR-004). */
export function withInput(c: Conditions, field: NumericInput, value: number): { conditions: Conditions; clamped: boolean } {
  const spec = INPUTS[field];
  const finite = Number.isFinite(value) ? value : spec.min;
  const next = Math.min(spec.max, Math.max(spec.min, finite));
  const conditions = cloneConditions(c);
  const [group, key] = field.split('.') as [keyof Conditions, string];
  (conditions[group] as unknown as Record<string, number>)[key] = next;
  if (field === 'light.lux') conditions.light.mode = 'set';
  return { conditions, clamped: next !== value };
}

/** Sky minus IR sensor temperature (FR-003). */
export const differential = (c: Conditions) => c.ir.sky - c.ir.ambient;

/** Sets the differential by moving the sky temperature; the IR sensor's own temperature stays. */
export const withDifferential = (c: Conditions, value: number) => withInput(c, 'ir.sky', c.ir.ambient + value);

// --- Ramps ---------------------------------------------------------------------

export interface Ramp {
  field: NumericInput;
  from: number;
  to: number;
  startMs: number; // device uptime clock
  durationMs: number;
}

// Light spans decades, so it ramps on a log scale.
const interpolate = (ramp: Ramp, progress: number) =>
  ramp.field === 'light.lux'
    ? Math.pow(10, Math.log10(Math.max(ramp.from, 1e-4)) + (Math.log10(Math.max(ramp.to, 1e-4)) - Math.log10(Math.max(ramp.from, 1e-4))) * progress)
    : ramp.from + (ramp.to - ramp.from) * progress;

/** Applies the ramps at `nowMs`; finished ramps are dropped. */
export function advanceRamps(c: Conditions, ramps: Ramp[], nowMs: number): { conditions: Conditions; ramps: Ramp[] } {
  let conditions = c;
  const remaining: Ramp[] = [];
  for (const ramp of ramps) {
    const progress = ramp.durationMs <= 0 ? 1 : Math.min(1, Math.max(0, (nowMs - ramp.startMs) / ramp.durationMs));
    conditions = withInput(conditions, ramp.field, interpolate(ramp, progress)).conditions;
    if (progress < 1) remaining.push(ramp);
  }
  return { conditions, ramps: remaining };
}

export const rampRemainingMs = (ramp: Ramp, nowMs: number) => Math.max(0, ramp.startMs + ramp.durationMs - nowMs);

// --- To the device core ---------------------------------------------------------

// A little steady variation so the readings look alive (research R9);
// smaller than every shortcut margin.
const wobble = (nowMs: number, periodS: number, amplitude: number) => Math.sin((nowMs / 1000 / periodS) * 2 * Math.PI) * amplitude;

export interface InputContext {
  sunLux: number; // illuminance from the sun's position, for light mode 'sun'
  gps: { enabled: boolean; latitude: number; longitude: number; altitude: number };
}

function gpsInput(c: Conditions, context: InputContext) {
  if (c.faults.gps) return { failed: true, fix: false };
  if (!context.gps.enabled || !c.gps.fix) return { fix: false };
  const { latitude, longitude, altitude } = context.gps;
  return { fix: true, latitude, longitude, altitude, satellites: 9 };
}

/** The JSON the device core's tick() reads (tools/demo-core/bridge.cpp readSensors). */
export function toCoreInputs(c: Conditions, nowMs: number, context: InputContext) {
  const w = (periodS: number, amplitude: number) => (c.steady ? 0 : wobble(nowMs, periodS, amplitude));
  const clamp = (field: NumericInput, value: number) => Math.min(INPUTS[field].max, Math.max(INPUTS[field].min, value));

  const baseLux = c.light.mode === 'sun' ? context.sunLux : c.light.lux;
  const lux = clamp('light.lux', baseLux * (1 + w(47, 0.04)));
  const counts = Math.min(65535, Math.round(Math.max(lux, 0.0001) * 1_000_000));
  const speed = clamp('wind.speed', c.wind.speed + w(90, 1.2));
  const gust = clamp('wind.gust', Math.max(speed, c.wind.gust + w(90, 1.2)));

  return {
    light: { present: true, failed: c.faults.light, lux, visible: Math.round(counts * 0.86), infrared: Math.round(counts * 0.14), full: counts, nightMode: lux < 0.5 },
    environment: {
      present: true,
      failed: c.faults.environment,
      temperature: clamp('air.temperature', c.air.temperature + w(900, 0.3)),
      humidity: clamp('air.humidity', c.air.humidity + w(600, 1.5)),
      pressure: clamp('air.pressure', c.air.pressure + w(3600, 2)),
    },
    infrared: { present: true, failed: c.faults.infrared, sky: clamp('ir.sky', c.ir.sky + w(120, 0.4)), ambient: clamp('ir.ambient', c.ir.ambient + w(900, 0.3)) },
    gps: gpsInput(c, context),
    rain: { failed: c.faults.rain, rate: c.rain.rate, lensFault: c.rain.lensFault },
    wind: { failed: c.faults.wind, speed, gust, direction: (c.wind.direction + w(200, 25) + 360) % 360 },
  };
}
