import type { AlertsConfig, AlpacaConfig, BleConfig, Config, WindConfig } from '../../types';

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
  onSafetyChange: true,
  onRain: true,
  onSensorFault: true,
  onDewRisk: false,
  dewRiskMarginC: 2,
  onClearSky: false,
  clearSkyCloudPercent: 20,
  cooldownSeconds: 300,
  pushover: { enabled: false, userKey: '', appToken: '', highPriority: 1, sound: '' },
  ntfy: { enabled: false, server: 'https://ntfy.sh', topic: '', token: '' },
  webhook: { enabled: false, url: '', authHeader: '', insecureTls: false },
  mqtt: { enabled: false },
};

// Fill in fields missing from configs saved by older firmware.
export const mergeAlertsConfig = (source?: Partial<AlertsConfig>): AlertsConfig => ({
  ...defaultAlertsConfig,
  ...source,
  pushover: { ...defaultAlertsConfig.pushover, ...source?.pushover },
  ntfy: { ...defaultAlertsConfig.ntfy, ...source?.ntfy },
  webhook: { ...defaultAlertsConfig.webhook, ...source?.webhook },
  mqtt: { ...defaultAlertsConfig.mqtt, ...source?.mqtt },
});

export const defaultBleConfig: BleConfig = {
  enabled: false,
  passkey: '',
  alarmOnUnsafe: true,
  alarmOnRain: true,
  alarmOnSensorFault: false,
};
