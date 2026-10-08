import { INPUTS, type Conditions, type NumericInput } from './conditions';

// One-click shortcuts, worked out from the device's *current* settings
// (spec 019 FR-008/FR-009, research R3): they set raw sensor readings that
// the device's own logic turns into the wanted outcome, say what they used,
// or say why they can't.

// The settings the shortcuts read (a subset of the device's config).
export interface ShortcutConfig {
  cloudDetection?: { clearSkyThreshold?: number; cloudyThreshold?: number; humidityCorrection?: number };
  alpaca?: { cloudCoverEnabled?: boolean; cloudCoverUnsafePercent?: number };
  alerts?: { dewRiskMarginC?: number };
  skyCalibration?: { enabled?: boolean; sqmOffset?: number };
  rain?: { enabled?: boolean };
}

export type ShortcutId = 'clear' | 'overcast' | 'cloudUnsafe' | 'rain' | 'rainStops' | 'darkSky' | 'dewRisk';

export interface ShortcutOptions {
  sqm?: number; // Dark sky target
}

export type ShortcutResult =
  | { ok: true; changes: Partial<Record<NumericInput, number>>; used: string; clamped?: string }
  | { ok: false; reason: string; link?: { label: string; route: string } };

export const SHORTCUTS: { id: ShortcutId; label: string; cloud?: boolean }[] = [
  { id: 'clear', label: 'Clear', cloud: true },
  { id: 'overcast', label: 'Overcast', cloud: true },
  { id: 'cloudUnsafe', label: 'Cloud just unsafe', cloud: true },
  { id: 'rain', label: 'Rain' },
  { id: 'rainStops', label: 'Rain stops' },
  { id: 'darkSky', label: 'Dark sky' },
  { id: 'dewRisk', label: 'Dew risk' },
];

// How far past each limit a shortcut aims, so the device's smoothing and the
// natural variation (research R9) don't leave it hovering on the boundary.
const CLEAR_MARGIN_C = 3;
const OVERCAST_MARGIN_C = 1;
const UNSAFE_MARGIN_PERCENT = 3;
const RAIN_RATE_MM_H = 2.5;
export const DARK_SKY_SQM = 21.5;
// The cloud model uses this humidity when there's no reading (DeviceCore ASSUMED_HUMIDITY_PERCENT).
const ASSUMED_HUMIDITY = 53;
// SkyLogic dewpointMagnus
const MAGNUS_A = 17.27;
const MAGNUS_B = 237.7;

const SENSORS_LINK = { label: 'Sensors', route: '/settings?tab=sensors' };
const SAFETY_LINK = { label: 'Safety rules', route: '/settings?tab=safety' };
const ALERTS_LINK = { label: 'Alerts', route: '/settings?tab=alerts' };

const notResponding = (name: string): ShortcutResult => ({ ok: false, reason: `The ${name} is set to not responding - clear that first.` });

const fmt = (n: number, digits = 1) => n.toFixed(digits);

function cloudModel(config: ShortcutConfig, c: Conditions) {
  const d = config.cloudDetection ?? {};
  const humidity = c.faults.environment ? ASSUMED_HUMIDITY : c.air.humidity;
  return {
    clear: d.clearSkyThreshold ?? -13,
    cloudy: d.cloudyThreshold ?? -3,
    correctionC: ((d.humidityCorrection ?? 0.75) / 100) * Math.max(0, Math.min(100, humidity)),
  };
}

// The sky temperature at which the device's corrected difference is `corrected`.
function skyFor(config: ShortcutConfig, c: Conditions, corrected: number, why: string): ShortcutResult {
  const model = cloudModel(config, c);
  const wanted = c.ir.ambient + corrected + model.correctionC;
  const spec = INPUTS['ir.sky'];
  const sky = Math.min(spec.max, Math.max(spec.min, wanted));
  const side = corrected <= 0 ? `${fmt(-corrected)} °C below` : `${fmt(corrected)} °C above`;
  const result: ShortcutResult = {
    ok: true,
    changes: { 'ir.sky': sky },
    used: `Sky ${fmt(sky)} °C: ${side} the IR sensor's ${fmt(c.ir.ambient)} °C after the humidity correction - ${why}.`,
  };
  if (sky !== wanted) result.clamped = `The sky temperature needed (${fmt(wanted)} °C) is outside the sensor's range; set to ${fmt(sky)} °C.`;
  return result;
}

export function shortcut(id: ShortcutId, config: ShortcutConfig, c: Conditions, options: ShortcutOptions = {}): ShortcutResult {
  switch (id) {
    case 'clear': {
      if (c.faults.infrared) return notResponding('IR sensor');
      const { clear } = cloudModel(config, c);
      return skyFor(config, c, clear - CLEAR_MARGIN_C, `your clear-sky threshold is ${fmt(clear)} °C`);
    }
    case 'overcast': {
      if (c.faults.infrared) return notResponding('IR sensor');
      const { cloudy } = cloudModel(config, c);
      return skyFor(config, c, cloudy + OVERCAST_MARGIN_C, `your overcast threshold is ${fmt(cloudy)} °C`);
    }
    case 'cloudUnsafe': {
      if (!config.alpaca?.cloudCoverEnabled) return { ok: false, reason: 'The cloud cover safety rule is off.', link: SAFETY_LINK };
      if (c.faults.infrared) return notResponding('IR sensor');
      const { clear, cloudy } = cloudModel(config, c);
      const limit = config.alpaca.cloudCoverUnsafePercent ?? 90;
      const cover = Math.min(100, limit + UNSAFE_MARGIN_PERCENT);
      const corrected = cover >= 100 ? cloudy + OVERCAST_MARGIN_C : clear + (cover / 100) * (cloudy - clear);
      return skyFor(config, c, corrected, `about ${fmt(cover, 0)}% cover; your safety limit is ${fmt(limit, 0)}%`);
    }
    case 'rain':
    case 'rainStops': {
      if (!config.rain?.enabled) return { ok: false, reason: 'The rain sensor is switched off.', link: SENSORS_LINK };
      if (c.faults.rain) return notResponding('rain sensor');
      const rate = id === 'rain' ? RAIN_RATE_MM_H : 0;
      return { ok: true, changes: { 'rain.rate': rate }, used: `Rain ${fmt(rate)} mm/h.` };
    }
    case 'darkSky': {
      if (c.faults.light) return notResponding('light sensor');
      const sqm = options.sqm ?? DARK_SKY_SQM;
      const offset = config.skyCalibration?.enabled ? config.skyCalibration.sqmOffset ?? 0 : 0;
      const wanted = Math.pow(10, (12.6 - (sqm - offset)) / 2.5);
      const spec = INPUTS['light.lux'];
      const lux = Math.min(spec.max, Math.max(spec.min, wanted));
      const calibrated = offset ? ` with your calibration offset of ${offset >= 0 ? '+' : ''}${fmt(offset, 2)}` : '';
      const result: ShortcutResult = { ok: true, changes: { 'light.lux': lux }, used: `Illuminance ${lux.toPrecision(3)} lux: SQM ${fmt(sqm, 2)}${calibrated}.` };
      if (lux !== wanted) result.clamped = `SQM ${fmt(sqm, 2)} needs ${wanted.toPrecision(3)} lux, outside the sensor's range; set to ${lux.toPrecision(3)} lux.`;
      return result;
    }
    case 'dewRisk': {
      if (c.faults.environment) return notResponding('BME280');
      const margin = config.alerts?.dewRiskMarginC ?? 2;
      if (margin <= 0)
        return { ok: false, reason: 'Your dew-risk margin is 0 °C, and the dew point can never be above the air temperature.', link: ALERTS_LINK };
      const gap = margin > 0.5 ? margin - 0.5 : margin / 2;
      const t = c.air.temperature;
      const dewpoint = t - gap;
      const humidity = 100 * Math.exp((MAGNUS_A * dewpoint) / (MAGNUS_B + dewpoint) - (MAGNUS_A * t) / (MAGNUS_B + t));
      return {
        ok: true,
        changes: { 'air.humidity': Math.round(humidity * 10) / 10 },
        used: `Humidity ${fmt(humidity)}%: dew point ${fmt(dewpoint)} °C, ${fmt(gap)} °C below the air - your dew-risk margin is ${fmt(margin)} °C.`,
      };
    }
  }
}
