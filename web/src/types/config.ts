import type { AlertsConfig } from './alerts';

export interface WiFiConfig {
  ssid: string;
  password: string;
  hostname: string;
  mdns?: boolean;
  ipv6?: boolean;
  autoReconnect: boolean;
  reconnectDelayMs: number;
  maxReconnectDelayMs: number;
}

export interface MQTTPublishGroups {
  sky: boolean;
  environment: boolean;
  clouds: boolean;
  gps: boolean;
  rain: boolean;
  wind: boolean;
  safety: boolean;
  diagnostics: boolean;
}

export interface MQTTConfig {
  enabled: boolean;
  broker: string;
  port: number;
  username: string;
  password: string;
  topic: string; // base topic
  publishIntervalMs: number;
  publish?: MQTTPublishGroups;
  homeAssistant?: { enabled: boolean; discoveryPrefix: string };
}

export interface OTAConfig {
  enabled: boolean;
  password: string;
}

export interface AuthConfig {
  enabled: boolean;
  username: string;
  password: string;
}

export interface NTPConfig {
  enabled: boolean;
  server1: string;
  server2: string;
  timezone: string; // POSIX, e.g. "GMT0BST,M3.5.0/1,M10.5.0"
  syncIntervalMs: number;
}

export interface GPSConfig {
  enabled: boolean;
  rxPin: number;
  txPin: number;
  baudRate: number;
}

export interface SensorConfig {
  readIntervalMs: number;
  i2cSDA: number;
  i2cSCL: number;
  i2cFrequency: number;
}

export interface SkyAveragingConfig {
  windowSeconds: number;
}

export interface SkyCalibrationConfig {
  enabled: boolean;
  sqmOffset: number;
  darkVisibleOffset: number;
  darkFullOffset: number;
  darkIrOffset: number;
  darkSampleCount: number;
  darkCalibratedAt: number;
}

export interface CloudDetectionConfig {
  clearSkyThreshold: number;
  cloudyThreshold: number;
  humidityCorrection: number;
}

export interface AlpacaConfig {
  enabled: boolean;
  manualOverrideUnsafe: boolean;
  staleAfterSeconds: number;
  cloudCoverEnabled: boolean;
  cloudCoverUnsafePercent: number;
  sqmMinEnabled: boolean;
  sqmMinSafe: number;
  humidityMaxEnabled: boolean;
  humidityMaxSafe: number;
  dewpointMarginEnabled: boolean;
  dewpointMarginMinC: number;
  rainUnsafeEnabled: boolean;
  rainSensorRequired: boolean;
  safeDelaySeconds: number;
  windSpeedUnsafeEnabled: boolean;
  windSpeedUnsafeMs: number;
  windGustUnsafeEnabled: boolean;
  windGustUnsafeMs: number;
}

export interface BleConfig {
  enabled: boolean;
  passkey: string;
}

export interface WindConfig {
  enabled: boolean;
  speedPin: number;
  directionEnabled: boolean;
  directionPin: number;
  kmhPerHz: number;
  directionOffsetDeg: number;
  vanePullupOhms: number;
}

export interface Config {
  deviceName: string;
  language?: string; // "en" or a supported code (specs/023-i18n); older firmware has none
  primaryTimeSource: number; // 0=NTP, 1=GPS
  secondaryTimeSource: number; // 0=NTP, 1=GPS
  wifi: WiFiConfig;
  mqtt: MQTTConfig;
  ota: OTAConfig;
  auth: AuthConfig;
  ntp: NTPConfig;
  gps: GPSConfig;
  sensor: SensorConfig;
  skyAveraging?: SkyAveragingConfig;
  skyCalibration?: SkyCalibrationConfig;
  cloudDetection: CloudDetectionConfig;
  rain?: RainSensorConfig;
  alpaca?: AlpacaConfig;
  alerts?: AlertsConfig;
  ble?: BleConfig;
  wind?: WindConfig;
  location?: LocationConfig;
}

export interface LocationConfig {
  set: boolean;
  latitude: number;
  longitude: number;
  showSunMoon?: boolean;
}

export interface RainSensorConfig {
  enabled: boolean;
  rxPin: number;
  txPin: number;
  baudRate: number;
  debugUart: boolean;
  mode: 'polling';
  resolution: 'high' | 'low' | 'switch';
  units: 'metric' | 'imperial' | 'switch';
  pollIntervalMs: number;
  rainClearDelayMs: number;
  dailyResetEnabled: boolean;
  dailyResetHour: number;
  dailyResetMinute: number;
}
