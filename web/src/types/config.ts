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
  // Second I2C bus for the SQM-L (spec 030); 255 = not wired. Older firmware has none.
  i2c2SDA?: number;
  i2c2SCL?: number;
}

export type SkyRole = 'sqm' | 'sqml';

// Which sky sensors run and whose reading counts (spec 030).
export interface SkyConfig {
  sqmEnabled: boolean;
  sqmlEnabled: boolean;
  authoritative: SkyRole;
  showOther: boolean;
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

// Optional OLED display (spec 031, contracts/config.md).
export type DisplayPage = 'sky' | 'environment' | 'cloud' | 'safety' | 'gps' | 'battery' | 'network';
export const DISPLAY_PAGES: DisplayPage[] = ['sky', 'environment', 'cloud', 'safety', 'gps', 'battery', 'network'];
export const DISPLAY_BUTTON_UNSET = 255;

export interface DisplayConfig {
  enabled: boolean;
  pages: DisplayPage[]; // 1..7, unique, in display order
  cycleSeconds: number; // 2..60
  brightness: 'low' | 'medium' | 'high';
  timeoutSeconds: number; // 0 = never, or 10..1800
  wakeOnBoot: boolean;
  stayOnAtNight: boolean;
  rotate180: boolean;
  buttonPin: number; // 255 = none
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
  sky?: SkyConfig;
  skyAveragingL?: SkyAveragingConfig;
  skyCalibrationL?: SkyCalibrationConfig;
  cloudDetection: CloudDetectionConfig;
  rain?: RainSensorConfig;
  alpaca?: AlpacaConfig;
  alerts?: AlertsConfig;
  ble?: BleConfig;
  wind?: WindConfig;
  location?: LocationConfig;
  display?: DisplayConfig; // older firmware has none
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
