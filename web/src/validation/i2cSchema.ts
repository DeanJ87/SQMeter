import { z } from 'zod';
import { t } from '../i18n';
import { validGPIOs } from './gpio';

// The second I2C bus and the SQM-L (spec 030, D-42): both pins unset (255),
// or two different valid GPIOs no other bus uses; the SQM-L needs them. The
// anemometer and vane pins count too: the device refuses that clash in its
// wind check, so it is shown here, on the I2C-2 field, rather than on save.
const I2C2_UNSET = 255;
interface SecondI2cInput {
  sensor: { i2cSDA: number; i2cSCL: number; i2c2SDA?: number; i2c2SCL?: number };
  gps: { enabled: boolean; rxPin: number; txPin: number };
  rain?: { enabled: boolean; rxPin: number; txPin: number };
  wind?: { enabled: boolean; speedPin: number; directionEnabled: boolean; directionPin: number };
  sky?: { sqmlEnabled: boolean };
}
const secondI2cPinsInUse = (data: SecondI2cInput, sda: number, scl: number) => {
  const used = [data.sensor.i2cSDA, data.sensor.i2cSCL];
  if (data.gps.enabled) used.push(data.gps.rxPin, data.gps.txPin);
  if (data.rain?.enabled) used.push(data.rain.rxPin, data.rain.txPin);
  if (data.wind?.enabled) used.push(data.wind.speedPin, ...(data.wind.directionEnabled ? [data.wind.directionPin] : []));
  return used.includes(sda) || used.includes(scl);
};
const secondI2cIssue = (data: SecondI2cInput, sda: number, scl: number) => {
  if (!validGPIOs.includes(sda) || !validGPIOs.includes(scl) || sda === scl) return t('validation.configSchema.secondI2cBusPinsAreInvalid');
  return secondI2cPinsInUse(data, sda, scl) ? t('validation.configSchema.secondI2cBusPinsAreInUse') : null;
};
export const checkSecondI2c = (data: SecondI2cInput, ctx: z.RefinementCtx) => {
  const sda = data.sensor.i2c2SDA ?? I2C2_UNSET;
  const scl = data.sensor.i2c2SCL ?? I2C2_UNSET;
  const unset = sda === I2C2_UNSET && scl === I2C2_UNSET;
  const pinIssue = unset ? null : secondI2cIssue(data, sda, scl);
  if (pinIssue) ctx.addIssue({ code: 'custom', message: pinIssue, path: ['sensor', 'i2c2SDA'] });
  if (data.sky?.sqmlEnabled && unset)
    ctx.addIssue({ code: 'custom', message: t('validation.configSchema.sqmLNeedsTheSecondI2cBusPins'), path: ['sky', 'sqmlEnabled'] });
};
