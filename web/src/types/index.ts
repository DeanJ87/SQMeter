// The readings document: GET /api/sensors, /ws/sensors and MQTT <base>/state
// (specs/013-data-interfaces/contracts/readings.md). A group whose status
// isn't "ok" carries only status and ageMs.
export type SensorHealth = 'ok' | 'missing' | 'error' | 'stale';

interface Group {
  status: SensorHealth;
  ageMs?: number;
}

export interface LightReading extends Group {
  lux?: number;
  visible?: number;
  infrared?: number;
  full?: number;
  gain?: string;
  gainFactor?: number;
  integrationMs?: number;
  saturated?: boolean;
  nightMode?: boolean;
}

export interface SkyReading extends Group {
  sqm?: number;
  rawSqm?: number;
  nelm?: number;
  bortle?: number;
  description?: string;
  calibrated?: boolean;
  averagingWindowSeconds?: number;
}

export interface EnvironmentReading extends Group {
  temperature?: number;
  humidity?: number;
  pressure?: number;
  dewpoint?: number;
}

export interface InfraredReading extends Group {
  skyTemperature?: number;
  ambientTemperature?: number;
}

export interface CloudReading extends Group {
  coverPercent?: number;
  condition?: 'clear' | 'cloudy' | 'overcast' | 'unknown';
  description?: string;
  temperatureDelta?: number;
  correctedDelta?: number;
  humidity?: number;
  humiditySource?: 'measured' | 'assumed';
}

export interface GpsReading extends Group {
  fix?: boolean;
  satellites?: number;
  latitude?: number;
  longitude?: number;
  altitude?: number;
  hdop?: number;
}

// Always metric (mm, mm/h).
export interface RainReading extends Group {
  raining?: boolean;
  rainingNow?: boolean;
  intensity?: number;
  eventAccumulation?: number;
  sensorEventAccumulation?: number;
  totalAccumulation?: number;
  lensFault?: boolean;
  emitterSaturated?: boolean;
}

export interface WindReading extends Group {
  speed?: number;
  gust?: number;
  direction?: number;
  vaneFault?: boolean;
}

export interface SensorData {
  timestamp: number;
  timeValid: boolean;
  dataAgeMs: number;
  dataStale: boolean;
  light: LightReading;
  sky: SkyReading;
  environment: EnvironmentReading;
  infrared: InfraredReading;
  clouds: CloudReading;
  gps?: GpsReading;
  rain?: RainReading;
  wind?: WindReading;
  safety?: SafetyStatus;
}

// GET /api/status -> diagnostics.rain
export interface RainDiagnostics {
  state: string;
  uartOpened: boolean;
  rxPin: number;
  txPin: number;
  baudRate: number;
  uartPort: number;
  lastCommand?: string;
  lastAck?: string;
  lastResponse?: string;
  lastError?: string;
  timeouts: number;
  parseErrors: number;
  successfulReads: number;
  lastPollAgeMs?: number;
  lastResponseAgeMs?: number;
  lastSuccessfulReadAgeMs?: number;
  lastRainDetectedAgeMs?: number;
  lastTotalResetAgeMs?: number;
  lastRebootAgeMs?: number;
  softwareVersion?: string;
  softwareBuildDate?: string;
  resetReason?: string;
  powerOnDays?: number;
  emitterTotal?: number;
}

export interface LightDiagnostics {
  rollingVisible: number;
  correctedVisible: number;
  darkVisibleOffset: number;
  sampleCount: number;
  windowSamples?: number; // samples in a full averaging window
  nightMode?: boolean;
  rejectedSamples: number;
  consecutiveSaturatedSamples: number;
  consecutiveLowSamples: number;
}

export interface SensorStatusEntry {
  status: SensorHealth;
  ageMs?: number; // absent when missing
}

export interface SystemStatus {
  firmware?: {
    name: string;
    version: string;
    buildDate: string;
    buildTime: string;
    variant?: 'standard' | 'ble';
  };
  sky?: {
    locationSource: 'gps' | 'manual' | 'none';
    nightKnown: boolean;
    isNight?: boolean;
    sunAltitudeDeg?: number;
    latitude?: number;
    longitude?: number;
  };
  ble?: {
    available: boolean;
    active: boolean;
    clients: number;
    alarm?: {
      serviceActive: boolean;
      active: boolean;
      sequence: number;
      acknowledgedSequence: number;
      bondedPhones: number;
    };
  };
  uptime: number;
  bootCount?: number;
  resetReason?: number; // ESP-IDF esp_reset_reason_t
  freeHeap: number;
  minFreeHeap?: number;
  maxAllocHeap?: number;
  heapSize: number;
  stackFree?: { asyncTcp: number; loop: number };
  sensorSnapshotBytes?: number;
  heapStages?: { stage: string; free: number; largest: number }[];
  cpuFreqMHz: number;
  flashSize: number;
  sketchSize: number;
  freeSketchSpace: number;
  fsTotal: number;
  fsUsed: number;
  partitions?: {
    runningSlot: string;
    runningAddress: number;
    runningSize: number;
    bootSlot: string;
    nextSlot: string;
    nextSize: number;
    nvs?: {
      usedEntries: number;
      freeEntries: number;
      totalEntries: number;
      namespaceCount: number;
    };
    fsAddress: number;
    fsSize: number;
  };
  time: {
    iso: string;
    timezone: string;
  };
  ntp?: {
    enabled: boolean;
    synced: boolean;
    status: number;
    lastSync: number;
    nextSync: number;
    drift: number;
    server: string;
    activeSource: number; // 0=None, 1=NTP, 2=GPS
    gpsEnabled: boolean;
    gpsHasFix: boolean;
    gpsTimeUTC: string;
    gpsSatellites: number;
  };
  wifi: {
    connected: boolean;
    ssid: string;
    ip: string;
    rssi: number;
    mac: string;
    connectPending?: boolean;
    apMode?: boolean; // the "SQM-Setup" hotspot is up
    hostname?: string;
    mdns?: boolean;
  };
  mqtt?: {
    enabled: boolean;
    connected: boolean;
    state: number;
    lastPublish: number;
    lastReconnectAttempt: number;
    broker: string;
    port: number;
    topic: string;
    availabilityTopic?: string;
    clientId?: string;
  };
  // Present hardware only; readings are in /api/sensors.
  sensors: {
    light: SensorStatusEntry;
    environment: SensorStatusEntry;
    infrared: SensorStatusEntry;
    gps?: SensorStatusEntry;
    rain?: SensorStatusEntry;
    wind?: SensorStatusEntry & { vaneStatus: 'ok' | 'fault' | 'off' };
  };
  diagnostics?: {
    light?: LightDiagnostics;
    rain?: RainDiagnostics;
  };
  alerts?: AlertSchedule;
  alpaca?: {
    enabled: boolean;
    clients: { safetymonitor: AlpacaClientState; observingconditions: AlpacaClientState };
  };
}

export interface WiFiConfig {
  ssid: string;
  password: string;
  hostname: string;
  mdns?: boolean;
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

// Live SafetyMonitor verdict (GET /api/safety, and `safety` on /ws/sensors)
export interface SafetyStatus {
  safe: boolean;
  rawSafe: boolean;
  alpacaEnabled: boolean;
  reasonFlags: number;
  reasons: string[];
  secondsUntilSafe: number;
  evaluatedAgeMs: number;
  changedAgeMs: number;
}

// 0 off, 1 quiet, 2 normal, 3 urgent, 4 wake me
export type AlertLevel = 0 | 1 | 2 | 3 | 4;

export type AlertEventKey =
  | 'unsafe'
  | 'safe'
  | 'rain_started'
  | 'rain_stopped'
  | 'sensor_fault'
  | 'sensor_recovered'
  | 'dew_risk'
  | 'clear_sky'
  | 'clouded_over'
  | 'client_lost'
  | 'client_back'
  | 'client_disconnected';

// sound: Pushover sound name; empty uses the Pushover default.
export interface AlertEventSetting {
  level: AlertLevel;
  sound: string;
  // Custom wording with {variables}; empty or missing uses the default.
  title?: string;
  message?: string;
}

export interface AlertsConfig {
  enabled: boolean;
  events: Record<AlertEventKey, AlertEventSetting>;
  dewRiskMarginC: number;
  clearSkyCloudPercent: number;
  cloudedOverCloudPercent: number;
  skyNightOnly: boolean;
  safetyNightOnly: boolean;
  // When alerts are sent (specs/021); older firmware has only armWithAlpaca.
  sendMode?: AlertSendMode;
  armWithAlpaca?: boolean;
  clientSilentSafetySeconds?: number;
  clientSilentWeatherSeconds?: number;
  nightSunAltitudeDeg: number;
  cooldownSeconds: number;
  pushover: { enabled: boolean; userKey: string; appToken: string; sound: string };
  ntfy: { enabled: boolean; server: string; topic: string; token: string };
  webhook: { enabled: boolean; url: string; authHeader: string; insecureTls: boolean };
  mqtt: { enabled: boolean };
}

export type AlertChannelName = 'mqtt' | 'pushover' | 'ntfy' | 'webhook';

export type AlertSendMode = 'any' | 'whileConnected';

export type AlertScheduleReason =
  'none' | 'user-ui' | 'user-rest' | 'user-mqtt' | 'client-connected' | 'client-disconnected' | 'waiting-for-client' | 'migrated';

// GET /api/alerts/armed, and /api/status "alerts". Older firmware sends only
// armed and armWithAlpaca.
export interface AlertSchedule {
  armed: boolean;
  armWithAlpaca?: boolean;
  mode?: AlertSendMode;
  reason?: AlertScheduleReason;
  since?: string | null; // ISO 8601 UTC, null without a clock
  sinceAgeMs?: number | null; // null: before this boot
}

// One Alpaca device as the imaging app sees it (/api/status "alpaca").
export interface AlpacaClientState {
  connected: boolean;
  watching: boolean;
  silent: boolean;
  lastCheckedAgeMs: number | null;
  clientId: number | null;
}

export interface AlertRecord {
  id: number;
  event: string;
  title: string;
  message: string;
  level: 'quiet' | 'normal' | 'urgent' | 'wake' | 'off';
  ageSeconds: number;
  timestamp?: number;
  channels: Partial<Record<AlertChannelName, { status: 'pending' | 'sent' | 'failed' | 'skipped'; detail: string }>>;
}

export interface AlertsRecent {
  enabled: boolean;
  // Sending (true) or paused; missing from older firmware = sending.
  armed?: boolean;
  alerts: AlertRecord[];
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

export interface WiFiNetwork {
  ssid: string;
  rssi: number;
  encryption: 'open' | 'secured';
}

export interface GithubRelease {
  tag: string;
  name: string;
  prerelease: boolean;
  publishedAt: string;
  firmwareAssetUrl: string;
  firmwareAssetSize: number;
  fsAssetUrl: string;
  fsAssetSize: number;
}

// ASCOM Alpaca response envelope (https://ascom-standards.org/api/)
export interface AlpacaResponse<T> {
  Value: T;
  ClientTransactionID: number;
  ServerTransactionID: number;
  ErrorNumber: number;
  ErrorMessage: string;
}

export interface AlpacaConfiguredDevice {
  DeviceName: string;
  DeviceType: string;
  DeviceNumber: number;
  UniqueID: string;
}

export interface AlpacaDeviceStateItem {
  Name: string;
  Value: number | boolean | string;
}
