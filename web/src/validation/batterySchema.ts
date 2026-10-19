import { z } from 'zod';
import { t, type MessageKey } from '../i18n';
import { validGPIOs } from './gpio';

// The battery (spec 032, contracts/config.md); pin clashes with the other
// buses, the anemometer and the display button are checked on the whole
// config below (D-51).
const BATTERY_PIN_UNSET = 255;
const percent = (min: number, max: number, key: MessageKey) => z.number().int().min(min, t(key)).max(max, t(key));
const batteryPin = z.number().int().min(0).max(255);
export const batteryConfigSchema = z
  .object({
    enabled: z.boolean(),
    lowPercent: percent(5, 50, 'validation.configSchema.batteryLowRange'),
    criticalPercent: percent(2, 25, 'validation.configSchema.batteryCriticalRange'),
    shutdownPercent: percent(2, 10, 'validation.configSchema.batteryShutdownRange'),
    capacityMah: z
      .number()
      .int()
      .refine((mah) => mah === 0 || (mah >= 500 && mah <= 50000), t('validation.configSchema.batteryCapacityRange')),
    powerSaving: z.boolean(),
    saveReadIntervalSeconds: percent(10, 300, 'validation.configSchema.batteryIntervalRange'),
    shutdownGraceSeconds: percent(10, 300, 'validation.configSchema.batteryGraceRange'),
    alertPin: batteryPin,
    stat1Pin: batteryPin,
    stat2Pin: batteryPin,
    pgPin: batteryPin,
  })
  .refine((b) => b.criticalPercent < b.lowPercent, {
    message: t('validation.configSchema.batteryCriticalRange'),
    path: ['criticalPercent'],
  })
  .refine((b) => b.shutdownPercent < b.criticalPercent, {
    message: t('validation.configSchema.batteryShutdownRange'),
    path: ['shutdownPercent'],
  });

// GPIOs that can wake the ESP32 from deep sleep (the device's isRtcGpio).
const rtcGPIOs = [0, 2, 4, 12, 13, 14, 15, 25, 26, 27, 32, 33, 34, 35, 36, 37, 38, 39];

interface BatteryPinInput {
  sensor: { i2cSDA: number; i2cSCL: number; i2c2SDA?: number; i2c2SCL?: number };
  gps: { enabled: boolean; rxPin: number; txPin: number };
  rain?: { enabled: boolean; rxPin: number; txPin: number };
  wind?: { enabled: boolean; speedPin: number; directionEnabled: boolean; directionPin: number };
  display?: { enabled: boolean; buttonPin: number };
  battery?: { alertPin: number; stat1Pin: number; stat2Pin: number; pgPin: number };
}
const batteryPinsInUse = (data: BatteryPinInput) => {
  const used = [data.sensor.i2cSDA, data.sensor.i2cSCL];
  const sda = data.sensor.i2c2SDA ?? BATTERY_PIN_UNSET;
  const scl = data.sensor.i2c2SCL ?? BATTERY_PIN_UNSET;
  if (sda !== BATTERY_PIN_UNSET && scl !== BATTERY_PIN_UNSET) used.push(sda, scl);
  if (data.gps.enabled) used.push(data.gps.rxPin, data.gps.txPin);
  if (data.rain?.enabled) used.push(data.rain.rxPin, data.rain.txPin);
  if (data.wind?.enabled) used.push(data.wind.speedPin, ...(data.wind.directionEnabled ? [data.wind.directionPin] : []));
  if (data.display?.enabled && data.display.buttonPin !== BATTERY_PIN_UNSET) used.push(data.display.buttonPin);
  return used;
};
// The device's message for the first problem with the battery pins, or null.
export const batteryPinIssue = (data: BatteryPinInput): { message: string; field: string } | null => {
  const b = data.battery;
  if (!b) return null;
  const fields = ['alertPin', 'stat1Pin', 'stat2Pin', 'pgPin'] as const;
  const set = fields.filter((f) => b[f] !== BATTERY_PIN_UNSET);
  for (const f of set) {
    const others = set.filter((o) => o !== f).map((o) => b[o]);
    if (!validGPIOs.includes(b[f]) || others.includes(b[f])) return { message: t('validation.configSchema.batteryPinsInvalid'), field: f };
  }
  const charger = set.filter((f) => f !== 'alertPin');
  if (charger.length !== 0 && charger.length !== 3)
    return {
      message: t('validation.configSchema.batteryChargerPinsAllOrNone'),
      field: fields.find((f) => f !== 'alertPin' && !set.includes(f))!,
    };
  const used = batteryPinsInUse(data);
  const clash = set.find((f) => used.includes(b[f]));
  if (clash) return { message: t('validation.configSchema.batteryPinInUse'), field: clash };
  if (b.pgPin !== BATTERY_PIN_UNSET && !rtcGPIOs.includes(b.pgPin))
    return { message: t('validation.configSchema.batteryPgPinCannotWake'), field: 'pgPin' };
  return null;
};
export const checkBatteryPins = (data: BatteryPinInput, ctx: z.RefinementCtx) => {
  const issue = batteryPinIssue(data);
  if (issue) ctx.addIssue({ code: 'custom', message: issue.message, path: ['battery', issue.field] });
};

// The safety tab's battery rule (alpaca.battery*); older firmware has none.
export const batteryRuleFields = {
  batteryEnabled: z.boolean().optional(),
  batteryMinPercent: z
    .number()
    .int()
    .min(2, t('validation.configSchema.batteryLimitRange'))
    .max(50, t('validation.configSchema.batteryLimitRange'))
    .optional(),
  batteryAlert: z.boolean().optional(),
  batteryPhoneAlarm: z.boolean().optional(),
};
