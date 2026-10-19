import { z } from 'zod';
import { t } from '../i18n';
import { validGPIOs } from './gpio';

// The OLED display (spec 031, contracts/config.md); the wake button's pin
// clash is checked on the whole config below (D-45).
const DISPLAY_BUTTON_UNSET = 255; // no wake button; also an unset I2C-2 pin
export const displayConfigSchema = z.object({
  enabled: z.boolean(),
  pages: z
    .array(z.enum(['sky', 'environment', 'cloud', 'safety', 'gps', 'battery', 'network']))
    .min(1, t('validation.configSchema.displayChoosePages'))
    .max(7, t('validation.configSchema.displayChoosePages'))
    .refine((pages) => new Set(pages).size === pages.length, t('validation.configSchema.displayChoosePages')),
  cycleSeconds: z
    .number()
    .int()
    .min(2, t('validation.configSchema.displayCycleRange'))
    .max(60, t('validation.configSchema.displayCycleRange')),
  brightness: z.enum(['low', 'medium', 'high']),
  timeoutSeconds: z
    .number()
    .int()
    .refine((s) => s === 0 || (s >= 10 && s <= 1800), t('validation.configSchema.displayTimeoutRange')),
  wakeOnBoot: z.boolean(),
  stayOnAtNight: z.boolean(),
  rotate180: z.boolean(),
  buttonPin: z
    .number()
    .int()
    .refine((pin) => pin === DISPLAY_BUTTON_UNSET || validGPIOs.includes(pin), t('validation.configSchema.wakeButtonPinIsNotAValidGpio')),
});

// D-45: while the display is on, its wake button can't share a pin with
// I2C-1, I2C-2, GPS, the rain sensor or the anemometer and vane.
export interface DisplayPinInput {
  sensor: { i2cSDA: number; i2cSCL: number; i2c2SDA?: number; i2c2SCL?: number };
  gps: { enabled: boolean; rxPin: number; txPin: number };
  rain?: { enabled: boolean; rxPin: number; txPin: number };
  wind?: { enabled: boolean; speedPin: number; directionEnabled: boolean; directionPin: number };
  display?: { enabled: boolean; buttonPin: number };
  battery?: { alertPin: number; stat1Pin: number; stat2Pin: number; pgPin: number };
}
const secondBus = (data: DisplayPinInput) => {
  const { i2c2SDA = DISPLAY_BUTTON_UNSET, i2c2SCL = DISPLAY_BUTTON_UNSET } = data.sensor;
  return i2c2SDA !== DISPLAY_BUTTON_UNSET && i2c2SCL !== DISPLAY_BUTTON_UNSET ? [i2c2SDA, i2c2SCL] : [];
};
const serialAndWind = (data: DisplayPinInput) => [
  ...(data.gps.enabled ? [data.gps.rxPin, data.gps.txPin] : []),
  ...(data.rain?.enabled ? [data.rain.rxPin, data.rain.txPin] : []),
  ...(data.wind?.enabled ? [data.wind.speedPin] : []),
  ...(data.wind?.enabled && data.wind.directionEnabled ? [data.wind.directionPin] : []),
  // The battery's wired pins (spec 032).
  ...(data.battery ? [data.battery.alertPin, data.battery.stat1Pin, data.battery.stat2Pin, data.battery.pgPin] : []).filter(
    (pin) => pin !== DISPLAY_BUTTON_UNSET,
  ),
];
export const checkDisplayButton = (data: DisplayPinInput, ctx: z.RefinementCtx) => {
  const pin = data.display?.buttonPin ?? DISPLAY_BUTTON_UNSET;
  if (!data.display?.enabled || pin === DISPLAY_BUTTON_UNSET) return;
  const used = [data.sensor.i2cSDA, data.sensor.i2cSCL, ...secondBus(data), ...serialAndWind(data)];
  if (used.includes(pin))
    ctx.addIssue({ code: 'custom', message: t('validation.configSchema.wakeButtonPinIsAlreadyInUse'), path: ['display', 'buttonPin'] });
};
