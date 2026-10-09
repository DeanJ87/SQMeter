import { z } from 'zod';
import { t } from '../i18n';
import { alertsConfigSchema } from './alertsSchema';

// Valid ESP32 GPIO pins
const validGPIOs = [0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39];

export const wifiConfigSchema = z.object({
  ssid: z.string().min(1, t('validation.configSchema.wifiSsidIsRequired')),
  password: z.string(),
  hostname: z
    .string()
    .min(1, t('validation.configSchema.hostnameIsRequired'))
    .max(32, t('validation.configSchema.hostnameCanBeAtMost'))
    .regex(/^[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?$/, t('validation.configSchema.useLettersNumbersAndHyphens')),
  mdns: z.boolean().optional(),
  autoReconnect: z.boolean(),
  reconnectDelayMs: z.number().int().positive(),
  maxReconnectDelayMs: z.number().int().positive(),
});

const MQTT_TOPIC = /^[a-zA-Z0-9_-]+(\/[a-zA-Z0-9_-]+)*$/;

export const mqttConfigSchema = z
  .object({
    enabled: z.boolean(),
    broker: z.string(),
    port: z.number().int().min(1, t('validation.configSchema.portMustBeAtLeast')).max(65535, t('validation.configSchema.portMustBeAtMost')),
    topic: z.string(),
    username: z.string(),
    password: z.string(),
    publishIntervalMs: z
      .number()
      .int()
      .min(1000, t('validation.configSchema.publishIntervalMustBeAt'))
      .max(86400000, t('validation.configSchema.publishIntervalCanBeAt')),
    publish: z
      .object({
        sky: z.boolean(),
        environment: z.boolean(),
        clouds: z.boolean(),
        gps: z.boolean(),
        rain: z.boolean(),
        wind: z.boolean(),
        safety: z.boolean(),
        diagnostics: z.boolean(),
      })
      .optional(),
    homeAssistant: z.object({ enabled: z.boolean(), discoveryPrefix: z.string() }).optional(),
  })
  // D-33 (a constraint): the device's own message.
  .refine((data) => !data.enabled || data.broker.trim().length > 0, {
    message: t('validation.configSchema.mqttBrokerAndTopicAre'),
    path: ['broker'],
  })
  .refine((data) => !data.enabled || data.topic.trim().length > 0, {
    message: t('validation.configSchema.mqttBrokerAndTopicAre'),
    path: ['topic'],
  })
  // Same rule as the device: letters, digits, _ - and / between levels.
  .refine((data) => !data.enabled || MQTT_TOPIC.test(data.topic), {
    message: t('validation.configSchema.useLettersNumbersAndWith'),
    path: ['topic'],
  })
  .refine((data) => !data.homeAssistant?.enabled || MQTT_TOPIC.test(data.homeAssistant.discoveryPrefix), {
    message: t('validation.configSchema.useLettersNumbersAndWith'),
    path: ['homeAssistant', 'discoveryPrefix'],
  });

// Command-line uploads without a password are kept but not in effect - a
// dependency (D-32), shown under the switch, not a rejected save.
export const otaConfigSchema = z.object({
  enabled: z.boolean(),
  password: z.string(),
});

export const authConfigSchema = z
  .object({
    enabled: z.boolean(),
    username: z.string(),
    password: z.string(),
  })
  // D-34 (a constraint): the device's own rule and message.
  .refine((data) => !data.enabled || data.password.length > 0, {
    message: t('validation.configSchema.httpAuthPasswordIsRequired'),
    path: ['password'],
  });

export const ntpConfigSchema = z.object({
  enabled: z.boolean(),
  server1: z.string().min(1, t('validation.configSchema.primaryNtpServerIsRequired')),
  server2: z.string(),
  timezone: z.string().min(1, t('validation.configSchema.timezoneIsRequired')),
  syncIntervalMs: z
    .number()
    .int()
    .min(600000, t('validation.configSchema.syncIntervalMustBeAt'))
    .max(86400000, t('validation.configSchema.syncIntervalCanBeAt')),
});

export const gpsConfigSchema = z
  .object({
    enabled: z.boolean(),
    rxPin: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: t('validation.configSchema.rxPinMustBeA', { join: validGPIOs.join(', ') }),
      }),
    txPin: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: t('validation.configSchema.txPinMustBeA', { join: validGPIOs.join(', ') }),
      }),
    baudRate: z
      .number()
      .int()
      .refine((val) => [4800, 9600, 19200, 38400, 57600, 115200].includes(val), {
        message: t('validation.configSchema.baudRateMustBeOne'),
      }),
  })
  .refine((data) => !data.enabled || data.rxPin !== data.txPin, {
    message: t('validation.configSchema.rxAndTxPinsMust'),
    path: ['rxPin'],
  });

export const sensorConfigSchema = z
  .object({
    readIntervalMs: z
      .number()
      .int()
      .min(100, t('validation.configSchema.readIntervalMustBeAt'))
      .max(3600000, t('validation.configSchema.readIntervalCannotExceed1')),
    i2cSDA: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: t('validation.configSchema.sdaPinMustBeA', { join: validGPIOs.join(', ') }),
      }),
    i2cSCL: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: t('validation.configSchema.sclPinMustBeA', { join: validGPIOs.join(', ') }),
      }),
    i2cFrequency: z
      .number()
      .int()
      .min(10000, t('validation.configSchema.i2cFrequencyMustBeAt'))
      .max(400000, t('validation.configSchema.i2cFrequencyMustBeAt2')),
  })
  .refine((data) => data.i2cSDA !== data.i2cSCL, {
    message: t('validation.configSchema.sdaAndSclPinsMust'),
    path: ['i2cSDA'],
  });

export const skyAveragingConfigSchema = z.object({
  windowSeconds: z
    .number()
    .int()
    .min(10, t('validation.configSchema.skyAveragingWindowMustBe'))
    .max(300, t('validation.configSchema.skyAveragingWindowCannotExceed')),
});

export const skyCalibrationConfigSchema = z.object({
  enabled: z.boolean(),
  sqmOffset: z.number().min(-5, t('validation.configSchema.sqmOffsetIsTooLow')).max(5, t('validation.configSchema.sqmOffsetIsTooHigh')),
  darkVisibleOffset: z.number().min(0, t('validation.configSchema.darkVisibleOffsetCannotBe')),
  darkFullOffset: z.number().min(0, t('validation.configSchema.darkFullOffsetCannotBe')),
  darkIrOffset: z.number().min(0, t('validation.configSchema.darkIrOffsetCannotBe')),
  darkSampleCount: z.number().int().min(0),
  darkCalibratedAt: z.number().int().min(0),
});

export const cloudDetectionConfigSchema = z
  .object({
    clearSkyThreshold: z.number().min(-30).max(0),
    cloudyThreshold: z.number().min(-20).max(10),
    humidityCorrection: z.number().min(0).max(2),
  })
  .refine((data) => data.clearSkyThreshold < data.cloudyThreshold, {
    message: t('validation.configSchema.clearSkyThresholdMustBe'),
    path: ['clearSkyThreshold'],
  });

export const alpacaConfigSchema = z.object({
  enabled: z.boolean(),
  manualOverrideUnsafe: z.boolean(),
  staleAfterSeconds: z
    .number()
    .int()
    .min(1, t('validation.configSchema.mustBeAtLeast1'))
    .max(3600, t('validation.configSchema.mustBeAtMost1')),
  cloudCoverEnabled: z.boolean(),
  cloudCoverUnsafePercent: z.number().min(0).max(100),
  sqmMinEnabled: z.boolean(),
  sqmMinSafe: z.number().min(0).max(30),
  humidityMaxEnabled: z.boolean(),
  humidityMaxSafe: z.number().min(0).max(100),
  dewpointMarginEnabled: z.boolean(),
  dewpointMarginMinC: z.number().min(0).max(20),
  rainUnsafeEnabled: z.boolean(),
  rainSensorRequired: z.boolean(),
  safeDelaySeconds: z
    .number()
    .int()
    .min(0, t('validation.configSchema.mustBe0OrMore'))
    .max(3600, t('validation.configSchema.mustBeAtMost1')),
  windSpeedUnsafeEnabled: z.boolean(),
  windSpeedUnsafeMs: z.number().gt(0, t('validation.configSchema.mustBeAbove0')).max(60, t('validation.configSchema.mustBeAtMost60')),
  windGustUnsafeEnabled: z.boolean(),
  windGustUnsafeMs: z.number().gt(0, t('validation.configSchema.mustBeAbove0')).max(80, t('validation.configSchema.mustBeAtMost80')),
});

export const windConfigSchema = z
  .object({
    enabled: z.boolean(),
    speedPin: z
      .number()
      .int()
      .refine((pin) => validGPIOs.includes(pin), { message: t('validation.configSchema.mustBeAValidGpio') }),
    directionEnabled: z.boolean(),
    directionPin: z
      .number()
      .int()
      .min(32, t('validation.configSchema.vaneNeedsAnAdc1Pin'))
      .max(39, t('validation.configSchema.vaneNeedsAnAdc1Pin')),
    kmhPerHz: z.number().gt(0).max(20),
    directionOffsetDeg: z.number().min(-360).max(360),
    vanePullupOhms: z.number().min(1000).max(100000),
  })
  .refine((data) => !data.enabled || !data.directionEnabled || data.speedPin !== data.directionPin, {
    message: t('validation.configSchema.anemometerAndVaneNeedDifferent'),
    path: ['directionPin'],
  });

export const rainSensorConfigSchema = z
  .object({
    enabled: z.boolean(),
    rxPin: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: t('validation.configSchema.rxPinMustBeA', { join: validGPIOs.join(', ') }),
      }),
    txPin: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: t('validation.configSchema.txPinMustBeA', { join: validGPIOs.join(', ') }),
      }),
    baudRate: z
      .number()
      .int()
      .refine((val) => [2400, 4800, 9600, 19200].includes(val), {
        message: t('validation.configSchema.baudRateMustBeOne2'),
      }),
    debugUart: z.boolean(),
    mode: z.literal('polling'),
    resolution: z.enum(['high', 'low', 'switch']),
    units: z.enum(['metric', 'imperial', 'switch']),
    pollIntervalMs: z
      .number()
      .int()
      .min(1000, t('validation.configSchema.pollIntervalMustBeAt'))
      .max(3600000, t('validation.configSchema.pollIntervalMustBeAt2')),
    rainClearDelayMs: z
      .number()
      .int()
      .min(60000, t('validation.configSchema.rainClearDelayMustBe'))
      .max(86400000, t('validation.configSchema.rainClearDelayMustBe2')),
    dailyResetEnabled: z.boolean(),
    dailyResetHour: z.number().int().min(0).max(23),
    dailyResetMinute: z.number().int().min(0).max(59),
  })
  .refine((data) => !data.enabled || data.rxPin !== data.txPin, {
    message: t('validation.configSchema.rxAndTxPinsMust'),
    path: ['rxPin'],
  });

export { alertsConfigSchema };

export const configSchema = z
  .object({
    deviceName: z.string().min(1, t('validation.configSchema.deviceNameIsRequired')),
    primaryTimeSource: z.number().int().min(0).max(1),
    secondaryTimeSource: z.number().int().min(0).max(1),
    wifi: wifiConfigSchema,
    mqtt: mqttConfigSchema,
    ota: otaConfigSchema,
    auth: authConfigSchema,
    ntp: ntpConfigSchema,
    gps: gpsConfigSchema,
    sensor: sensorConfigSchema,
    skyAveraging: skyAveragingConfigSchema.optional(),
    skyCalibration: skyCalibrationConfigSchema.optional(),
    rain: rainSensorConfigSchema.optional(),
    cloudDetection: cloudDetectionConfigSchema,
    alpaca: alpacaConfigSchema.optional(),
    alerts: alertsConfigSchema.optional(),
    ble: z
      .object({
        enabled: z.boolean(),
        // 6 digits, or the masked stored value
        passkey: z
          .string()
          .regex(/^(\d{6}|\*{8})?$/, t('validation.configSchema.passkeyMustBe6Digits'))
          .refine((value) => value !== '000000', t('validation.configSchema.passkeyCanTBe000000')),
      })
      .optional(),
    wind: windConfigSchema.optional(),
    location: z
      .object({
        set: z.boolean(),
        latitude: z.number().min(-90, t('validation.configSchema.latitudeIs90To90')).max(90, t('validation.configSchema.latitudeIs90To90')),
        longitude: z
          .number()
          .min(-180, t('validation.configSchema.longitudeIs180To180'))
          .max(180, t('validation.configSchema.longitudeIs180To180')),
        showSunMoon: z.boolean().optional(),
      })
      .optional(),
  })
  .superRefine((data, ctx) => {
    if (!data.ntp.enabled && !data.gps.enabled) {
      ctx.addIssue({
        code: 'custom',
        message: t('validation.configSchema.atLeastOneTimeSource'), // D-27
        path: ['ntp', 'enabled'],
      });
      return;
    }

    const sourceEnabled = (source: number) => (source === 0 ? data.ntp.enabled : data.gps.enabled);

    if (!sourceEnabled(data.primaryTimeSource)) {
      ctx.addIssue({
        code: 'custom',
        message: t('validation.configSchema.primaryTimeSourceIsDisabled'),
        path: ['primaryTimeSource'],
      });
    }

    if (data.ntp.enabled && data.gps.enabled && !sourceEnabled(data.secondaryTimeSource)) {
      ctx.addIssue({
        code: 'custom',
        message: t('validation.configSchema.secondaryTimeSourceIsDisabled'),
        path: ['secondaryTimeSource'],
      });
    }

    if (data.ntp.enabled && data.gps.enabled && data.primaryTimeSource === data.secondaryTimeSource) {
      ctx.addIssue({
        code: 'custom',
        message: t('validation.configSchema.timeSourcesMustBeDifferent'),
        path: ['secondaryTimeSource'],
      });
    }
  });

export type ConfigSchema = z.infer<typeof configSchema>;
export type ValidationErrors = Record<string, string>;

export const getConfigValidationErrors = (candidate: unknown): ValidationErrors => {
  const result = configSchema.safeParse(candidate);
  if (result.success) return {};

  return Object.fromEntries(result.error.issues.map((issue) => [issue.path.join('.') || 'config', issue.message]));
};

export const hasConfigValidationErrors = (errors: ValidationErrors): boolean => Object.keys(errors).length > 0;

export const getConfigValidationMessage = (errors: ValidationErrors): string => {
  const messages = Object.values(errors);
  if (messages.length === 1) return t('validation.configSchema.pleaseFix1ValidationError', { value: messages[0] });
  return t('validation.configSchema.pleaseFixLengthValidationErrors', { length: messages.length });
};
