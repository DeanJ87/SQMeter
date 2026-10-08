import type { AlertsConfig, AlpacaConfig, BleConfig, Config, LocationConfig, WindConfig } from '../../types';

// Defaults for every settings section - used to fill in fields that configs
// from older firmware don't have yet.

export const defaultAuthConfig: NonNullable<Config['auth']> = {
  enabled: false,
  username: 'admin',
  password: '',
};

export const defaultRainConfig: NonNullable<Config['rain']> = {
  enabled: false,
  rxPin: 18,
  txPin: 19,
  baudRate: 9600,
  debugUart: false,
  mode: 'polling',
  resolution: 'high',
  units: 'metric',
  pollIntervalMs: 5000,
  rainClearDelayMs: 15 * 60 * 1000,
  dailyResetEnabled: false,
  dailyResetHour: 0,
  dailyResetMinute: 0,
};

export const defaultCloudDetectionConfig: Config['cloudDetection'] = {
  clearSkyThreshold: -13.0,
  cloudyThreshold: -3.0,
  humidityCorrection: 0.75,
};

export const defaultAlpacaConfig: AlpacaConfig = {
  enabled: false,
  manualOverrideUnsafe: false,
  staleAfterSeconds: 30,
  cloudCoverEnabled: true,
  cloudCoverUnsafePercent: 90,
  sqmMinEnabled: false,
  sqmMinSafe: 0,
  humidityMaxEnabled: false,
  humidityMaxSafe: 100,
  dewpointMarginEnabled: false,
  dewpointMarginMinC: 0,
  rainUnsafeEnabled: true,
  rainSensorRequired: true,
  safeDelaySeconds: 0,
  windSpeedUnsafeEnabled: false,
  windSpeedUnsafeMs: 10,
  windGustUnsafeEnabled: false,
  windGustUnsafeMs: 15,
};

export const defaultWindConfig: WindConfig = {
  enabled: false,
  speedPin: 27,
  directionEnabled: false,
  directionPin: 35,
  kmhPerHz: 2.4,
  directionOffsetDeg: 0,
  vanePullupOhms: 10000,
};

export const defaultAlertsConfig: AlertsConfig = {
  enabled: false,
  events: {
    unsafe: { level: 3, sound: '' },
    safe: { level: 2, sound: '' },
    rain_started: { level: 4, sound: '' },
    rain_stopped: { level: 2, sound: '' },
    sensor_fault: { level: 4, sound: '' },
    sensor_recovered: { level: 1, sound: '' },
    dew_risk: { level: 0, sound: '' },
    clear_sky: { level: 0, sound: '' },
    clouded_over: { level: 0, sound: '' },
  },
  dewRiskMarginC: 2,
  clearSkyCloudPercent: 20,
  cloudedOverCloudPercent: 70,
  skyNightOnly: true,
  safetyNightOnly: true,
  nightSunAltitudeDeg: -12,
  cooldownSeconds: 300,
  pushover: { enabled: false, userKey: '', appToken: '', sound: '' },
  ntfy: { enabled: false, server: 'https://ntfy.sh', topic: '', token: '' },
  webhook: { enabled: false, url: '', authHeader: '', insecureTls: false },
  mqtt: { enabled: false },
};

// Fill in fields missing from configs saved by older firmware.
export const mergeAlertsConfig = (source?: Partial<AlertsConfig>): AlertsConfig => ({
  ...defaultAlertsConfig,
  ...source,
  events: { ...defaultAlertsConfig.events, ...source?.events },
  pushover: { ...defaultAlertsConfig.pushover, ...source?.pushover },
  ntfy: { ...defaultAlertsConfig.ntfy, ...source?.ntfy },
  webhook: { ...defaultAlertsConfig.webhook, ...source?.webhook },
  mqtt: { ...defaultAlertsConfig.mqtt, ...source?.mqtt },
});

export const defaultBleConfig: BleConfig = {
  enabled: false,
  passkey: '',
};

export const defaultLocationConfig: LocationConfig = { set: false, latitude: 0, longitude: 0, showSunMoon: true };
