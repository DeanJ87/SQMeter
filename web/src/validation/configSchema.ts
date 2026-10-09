import { z } from 'zod';
import { NTP_IPV6_TEXT, isIpv6Literal, parseHost, parseHttpUrl } from '../lib/netAddress';

// Valid ESP32 GPIO pins
const validGPIOs = [0, 1, 2, 3, 4, 5, 12, 13, 14, 15, 16, 17, 18, 19, 21, 22, 23, 25, 26, 27, 32, 33, 34, 35, 36, 39];

export const wifiConfigSchema = z.object({
  ssid: z.string().min(1, 'WiFi SSID is required'),
  password: z.string(),
  hostname: z
    .string()
    .min(1, 'Hostname is required')
    .max(32, 'Hostname can be at most 32 characters')
    .regex(/^[a-zA-Z0-9]([a-zA-Z0-9-]*[a-zA-Z0-9])?$/, 'Use letters, numbers and hyphens (not at either end)'),
  mdns: z.boolean().optional(),
  ipv6: z.boolean().optional(),
  autoReconnect: z.boolean(),
  reconnectDelayMs: z.number().int().positive(),
  maxReconnectDelayMs: z.number().int().positive(),
});

const MQTT_TOPIC = /^[a-zA-Z0-9_-]+(\/[a-zA-Z0-9_-]+)*$/;

export const mqttConfigSchema = z
  .object({
    enabled: z.boolean(),
    broker: z.string(),
    port: z.number().int().min(1, 'Port must be at least 1').max(65535, 'Port must be at most 65535'),
    topic: z.string(),
    username: z.string(),
    password: z.string(),
    publishIntervalMs: z
      .number()
      .int()
      .min(1000, 'Publish interval must be at least 1 second')
      .max(86400000, 'Publish interval can be at most 24 hours'),
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
    message: 'MQTT broker and topic are required when MQTT is enabled',
    path: ['broker'],
  })
  // Host forms, IPv6 included - the device's rules (lib/NetAddress, spec 015).
  .superRefine((data, ctx) => {
    if (data.broker === '') return;
    const host = parseHost(data.broker);
    if (host.error) ctx.addIssue({ code: 'custom', path: ['broker'], message: host.error });
  })
  .refine((data) => !data.enabled || data.topic.trim().length > 0, {
    message: 'MQTT broker and topic are required when MQTT is enabled',
    path: ['topic'],
  })
  // Same rule as the device: letters, digits, _ - and / between levels.
  .refine((data) => !data.enabled || MQTT_TOPIC.test(data.topic), {
    message: 'Use letters, numbers, _ and -, with / between levels (not at either end)',
    path: ['topic'],
  })
  .refine((data) => !data.homeAssistant?.enabled || MQTT_TOPIC.test(data.homeAssistant.discoveryPrefix), {
    message: 'Use letters, numbers, _ and -, with / between levels (not at either end)',
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
    message: 'HTTP auth password is required when auth is enabled',
    path: ['password'],
  });

export const ntpConfigSchema = z.object({
  enabled: z.boolean(),
  server1: z
    .string()
    .min(1, 'Primary NTP server is required')
    .refine((v) => !isIpv6Literal(v), NTP_IPV6_TEXT),
  server2: z.string().refine((v) => !isIpv6Literal(v), NTP_IPV6_TEXT),
  timezone: z.string().min(1, 'Timezone is required'),
  syncIntervalMs: z
    .number()
    .int()
    .min(600000, 'Sync interval must be at least 10 minutes')
    .max(86400000, 'Sync interval can be at most 24 hours'),
});

export const gpsConfigSchema = z
  .object({
    enabled: z.boolean(),
    rxPin: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: `RX pin must be a valid GPIO: ${validGPIOs.join(', ')}`,
      }),
    txPin: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: `TX pin must be a valid GPIO: ${validGPIOs.join(', ')}`,
      }),
    baudRate: z
      .number()
      .int()
      .refine((val) => [4800, 9600, 19200, 38400, 57600, 115200].includes(val), {
        message: 'Baud rate must be one of: 4800, 9600, 19200, 38400, 57600, 115200',
      }),
  })
  .refine((data) => !data.enabled || data.rxPin !== data.txPin, {
    message: 'RX and TX pins must be different',
    path: ['rxPin'],
  });

export const sensorConfigSchema = z
  .object({
    readIntervalMs: z.number().int().min(100, 'Read interval must be at least 100ms').max(3600000, 'Read interval cannot exceed 1 hour'),
    i2cSDA: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: `SDA pin must be a valid GPIO: ${validGPIOs.join(', ')}`,
      }),
    i2cSCL: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: `SCL pin must be a valid GPIO: ${validGPIOs.join(', ')}`,
      }),
    i2cFrequency: z.number().int().min(10000, 'I2C frequency must be at least 10kHz').max(400000, 'I2C frequency must be at most 400kHz'),
  })
  .refine((data) => data.i2cSDA !== data.i2cSCL, {
    message: 'SDA and SCL pins must be different',
    path: ['i2cSDA'],
  });

export const skyAveragingConfigSchema = z.object({
  windowSeconds: z
    .number()
    .int()
    .min(10, 'Sky averaging window must be at least 10 seconds')
    .max(300, 'Sky averaging window cannot exceed 300 seconds'),
});

export const skyCalibrationConfigSchema = z.object({
  enabled: z.boolean(),
  sqmOffset: z.number().min(-5, 'SQM offset is too low').max(5, 'SQM offset is too high'),
  darkVisibleOffset: z.number().min(0, 'Dark visible offset cannot be negative'),
  darkFullOffset: z.number().min(0, 'Dark full offset cannot be negative'),
  darkIrOffset: z.number().min(0, 'Dark IR offset cannot be negative'),
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
    message: 'Clear sky threshold must be less than cloudy threshold',
    path: ['clearSkyThreshold'],
  });

export const alpacaConfigSchema = z.object({
  enabled: z.boolean(),
  manualOverrideUnsafe: z.boolean(),
  staleAfterSeconds: z.number().int().min(1, 'Must be at least 1 second').max(3600, 'Must be at most 1 hour'),
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
  safeDelaySeconds: z.number().int().min(0, 'Must be 0 or more').max(3600, 'Must be at most 1 hour'),
  windSpeedUnsafeEnabled: z.boolean(),
  windSpeedUnsafeMs: z.number().gt(0, 'Must be above 0').max(60, 'Must be at most 60 m/s'),
  windGustUnsafeEnabled: z.boolean(),
  windGustUnsafeMs: z.number().gt(0, 'Must be above 0').max(80, 'Must be at most 80 m/s'),
});

export const windConfigSchema = z
  .object({
    enabled: z.boolean(),
    speedPin: z
      .number()
      .int()
      .refine((pin) => validGPIOs.includes(pin), { message: 'Must be a valid GPIO' }),
    directionEnabled: z.boolean(),
    directionPin: z.number().int().min(32, 'Vane needs an ADC1 pin (GPIO 32-39)').max(39, 'Vane needs an ADC1 pin (GPIO 32-39)'),
    kmhPerHz: z.number().gt(0).max(20),
    directionOffsetDeg: z.number().min(-360).max(360),
    vanePullupOhms: z.number().min(1000).max(100000),
  })
  .refine((data) => !data.enabled || !data.directionEnabled || data.speedPin !== data.directionPin, {
    message: 'Anemometer and vane need different pins',
    path: ['directionPin'],
  });

export const rainSensorConfigSchema = z
  .object({
    enabled: z.boolean(),
    rxPin: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: `RX pin must be a valid GPIO: ${validGPIOs.join(', ')}`,
      }),
    txPin: z
      .number()
      .int()
      .refine((val) => validGPIOs.includes(val), {
        message: `TX pin must be a valid GPIO: ${validGPIOs.join(', ')}`,
      }),
    baudRate: z
      .number()
      .int()
      .refine((val) => [2400, 4800, 9600, 19200].includes(val), {
        message: 'Baud rate must be one of: 2400, 4800, 9600, 19200',
      }),
    debugUart: z.boolean(),
    mode: z.literal('polling'),
    resolution: z.enum(['high', 'low', 'switch']),
    units: z.enum(['metric', 'imperial', 'switch']),
    pollIntervalMs: z
      .number()
      .int()
      .min(1000, 'Poll interval must be at least 1 second')
      .max(3600000, 'Poll interval must be at most 1 hour'),
    rainClearDelayMs: z
      .number()
      .int()
      .min(60000, 'Rain clear delay must be at least 1 minute')
      .max(86400000, 'Rain clear delay must be at most 24 hours'),
    dailyResetEnabled: z.boolean(),
    dailyResetHour: z.number().int().min(0).max(23),
    dailyResetMinute: z.number().int().min(0).max(59),
  })
  .refine((data) => !data.enabled || data.rxPin !== data.txPin, {
    message: 'RX and TX pins must be different',
    path: ['rxPin'],
  });

export const alertsConfigSchema = z
  .object({
    enabled: z.boolean(),
    events: z.record(
      z.string(),
      z.object({
        level: z.number().int().min(0).max(4),
        sound: z
          .string()
          .max(32)
          .regex(/^[a-z0-9_-]*$/i, 'Not a Pushover sound name'),
        title: z.string().max(80, 'Titles are up to 80 characters').optional(),
        message: z.string().max(240, 'Messages are up to 240 characters').optional(),
      }),
    ),
    dewRiskMarginC: z.number().min(0).max(10),
    clearSkyCloudPercent: z.number().min(0).max(100),
    cloudedOverCloudPercent: z.number().min(0).max(100),
    skyNightOnly: z.boolean(),
    safetyNightOnly: z.boolean().optional(),
    sendMode: z.enum(['any', 'whileConnected']).optional(),
    armWithAlpaca: z.boolean().optional(),
    clientSilentSafetySeconds: z.number().int().min(30, 'At least 30 seconds').max(3600, 'At most 60 minutes').optional(),
    clientSilentWeatherSeconds: z.number().int().min(30, 'At least 30 seconds').max(3600, 'At most 60 minutes').optional(),
    nightSunAltitudeDeg: z.number().min(-20).max(0),
    cooldownSeconds: z.number().int().min(0).max(86400, 'Must be at most 24 hours'),
    pushover: z.object({
      enabled: z.boolean(),
      userKey: z.string(),
      appToken: z.string(),
      sound: z.string(),
    }),
    ntfy: z.object({ enabled: z.boolean(), server: z.string(), topic: z.string(), token: z.string() }),
    webhook: z.object({ enabled: z.boolean(), url: z.string(), authHeader: z.string(), insecureTls: z.boolean() }),
    mqtt: z.object({ enabled: z.boolean() }),
  })
  .superRefine((data, ctx) => {
    if (data.cloudedOverCloudPercent <= data.clearSkyCloudPercent) {
      ctx.addIssue({ code: 'custom', path: ['cloudedOverCloudPercent'], message: 'Must be above the clear threshold' });
    }
    // Pushover keys are exactly 30 letters/digits; "********" is the stored, masked value.
    const pushoverKey = /^([A-Za-z0-9]{30}|\*{8})$/;
    if (data.pushover.enabled && !pushoverKey.test(data.pushover.userKey.trim())) {
      ctx.addIssue({
        code: 'custom',
        path: ['pushover', 'userKey'],
        message: 'The user key is the 30-character key on your Pushover dashboard',
      });
    }
    if (data.pushover.enabled && !pushoverKey.test(data.pushover.appToken.trim())) {
      ctx.addIssue({
        code: 'custom',
        path: ['pushover', 'appToken'],
        message: 'The app token is the 30-character API token of your Pushover application',
      });
    }
    if (data.ntfy.enabled) {
      const server = parseHttpUrl(data.ntfy.server);
      if (server.error) ctx.addIssue({ code: 'custom', path: ['ntfy', 'server'], message: server.error });
      if (!data.ntfy.topic) {
        ctx.addIssue({ code: 'custom', path: ['ntfy', 'topic'], message: 'Topic is required' });
      }
    }
    if (data.webhook.enabled) {
      const url = parseHttpUrl(data.webhook.url);
      if (url.error) ctx.addIssue({ code: 'custom', path: ['webhook', 'url'], message: url.error });
    }
  });

export const configSchema = z
  .object({
    deviceName: z.string().min(1, 'Device name is required'),
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
          .regex(/^(\d{6}|\*{8})?$/, 'Passkey must be 6 digits')
          .refine((value) => value !== '000000', "Passkey can't be 000000"),
      })
      .optional(),
    wind: windConfigSchema.optional(),
    location: z
      .object({
        set: z.boolean(),
        latitude: z.number().min(-90, 'Latitude is -90 to 90').max(90, 'Latitude is -90 to 90'),
        longitude: z.number().min(-180, 'Longitude is -180 to 180').max(180, 'Longitude is -180 to 180'),
        showSunMoon: z.boolean().optional(),
      })
      .optional(),
  })
  .superRefine((data, ctx) => {
    if (!data.ntp.enabled && !data.gps.enabled) {
      ctx.addIssue({
        code: 'custom',
        message: 'At least one time source must be enabled', // D-27
        path: ['ntp', 'enabled'],
      });
      return;
    }

    const sourceEnabled = (source: number) => (source === 0 ? data.ntp.enabled : data.gps.enabled);

    if (!sourceEnabled(data.primaryTimeSource)) {
      ctx.addIssue({
        code: 'custom',
        message: 'Primary time source is disabled',
        path: ['primaryTimeSource'],
      });
    }

    if (data.ntp.enabled && data.gps.enabled && !sourceEnabled(data.secondaryTimeSource)) {
      ctx.addIssue({
        code: 'custom',
        message: 'Secondary time source is disabled',
        path: ['secondaryTimeSource'],
      });
    }

    if (data.ntp.enabled && data.gps.enabled && data.primaryTimeSource === data.secondaryTimeSource) {
      ctx.addIssue({
        code: 'custom',
        message: 'Time sources must be different when both NTP and GPS are enabled',
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
  if (messages.length === 1) return `Please fix 1 validation error: ${messages[0]}`;
  return `Please fix ${messages.length} validation errors`;
};
