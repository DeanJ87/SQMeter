export interface LightSensorReading {
  lux: number;
  visible: number;
  infrared: number;
  full: number;
  status: number;
}

export interface EnvironmentReading {
  temperature: number;
  humidity: number;
  pressure: number;
  dewpoint: number;
  status: number;
}

export interface IRTemperatureReading {
  objectTemp: number;
  ambientTemp: number;
  status: number;
}

export interface SkyQuality {
  sqm: number;
  nelm: number;
  bortle: number;
  description: string;
}

export interface CloudConditions {
  temperatureDelta: number;
  correctedDelta: number;
  cloudCoverPercent: number;
  condition: number; // 0=Unknown, 1=Clear, 2=Cloudy, 3=Overcast
  description: string;
  humidityUsed: number;
}

export interface SensorData {
  dataTimestamp?: number;
  lightSensor?: LightSensorReading;
  environment?: EnvironmentReading;
  irTemperature?: IRTemperatureReading;
  skyQuality?: SkyQuality;
  cloudConditions?: CloudConditions;
  gps?: {
    hasFix: boolean;
    satellites: number;
    latitude: number;
    longitude: number;
    altitude: number;
    hdop: number;
    age: number;
  };
  rainSensor?: RainSensorReading;
}

export interface RG15UartDiagnostics {
  configured: boolean;
  opened: boolean;
  rx_pin: number;
  tx_pin: number;
  baud_rate: number;
  uart_port: number;
  mode: string;
  resolution: string;
  units: string;
  debug_uart: boolean;
  poll_interval_ms?: number;
  rain_clear_delay_ms?: number;
  daily_reset_enabled?: boolean;
  daily_reset_hour?: number;
  daily_reset_minute?: number;
  last_command?: string | null;
  last_command_ms?: number | null;
  last_bytes_written?: number;
  expected_ack?: string | null;
  last_ack?: string | null;
  last_ack_ms?: number | null;
  last_raw_response?: string | null;
  last_response_ms?: number | null;
  last_error?: string | null;
  timeouts: number;
  parse_errors: number;
  successful_reads: number;
  response_timeout_ms: number;
  stale_timeout_ms: number;
  last_health_check_ms?: number | null;
  last_health_check_age_ms?: number | null;
  last_poll_ms?: number | null;
  last_poll_age_ms?: number | null;
  last_rain_detected_ms?: number | null;
  last_rain_detected_age_ms?: number | null;
  last_total_reset_ms?: number | null;
  last_total_reset_age_ms?: number | null;
  last_reboot_command_ms?: number | null;
  last_reboot_command_age_ms?: number | null;
  last_status_line?: string | null;
  software_version?: string | null;
  software_build_date?: string | null;
  reset_reason?: string | null;
  power_on_days?: number | null;
  emitter_1?: number | null;
  emitter_2?: number | null;
  emitter_total?: number | null;
  last_response_age_ms?: number | null;
  last_successful_read_ms?: number | null;
  last_successful_read_age_ms?: number | null;
}

export interface RG15SensorDiagnostics {
  enabled: boolean;
  sensor: string;
  initialized: boolean;
  online: boolean;
  stale: boolean;
  state: string;
  timestamp: number;
  ageMs: number;
  status: number;
  isRaining: boolean;
  raining?: boolean;
  acc: number;
  eventAcc: number;
  totalAcc: number;
  rInt: number;
  accumulation_since_last_read?: number;
  event_accumulation?: number;
  local_event_accumulation?: number;
  hydreon_event_accumulation?: number;
  total_accumulation?: number;
  rain_intensity?: number;
  lensBad: boolean;
  emSat: boolean;
  uart: RG15UartDiagnostics;
}

export interface SystemStatus {
  firmware?: {
    name: string;
    version: string;
    buildDate: string;
    buildTime: string;
  };
  uptime: number;
  freeHeap: number;
  heapSize: number;
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
  };
  sensors: {
    tsl2591: {
      initialized: boolean;
      status: number;
      lastUpdate: number;
    };
    bme280: {
      initialized: boolean;
      status: number;
      lastUpdate: number;
    };
    mlx90614: {
      initialized: boolean;
      status: number;
      lastUpdate: number;
    };
    gps: {
      initialized: boolean;
      status: number;
      lastUpdate: number;
    };
    rg15?: {
      enabled: boolean;
      initialized: boolean;
      online: boolean;
      stale: boolean;
      state: string;
      status: number;
      lastUpdate: number;
      timestamp?: number;
      ageMs?: number;
      isRaining?: boolean;
      raining?: boolean;
      acc?: number;
      eventAcc?: number;
      totalAcc?: number;
      rInt?: number;
      accumulation_since_last_read?: number;
      event_accumulation?: number;
      local_event_accumulation?: number;
      hydreon_event_accumulation?: number;
      total_accumulation?: number;
      rain_intensity?: number;
      lensBad?: boolean;
      emSat?: boolean;
      uart?: RG15UartDiagnostics;
    };
  };
  gpsData?: {
    hasFix: boolean;
    satellites: number;
    latitude: number;
    longitude: number;
    altitude: number;
    hdop: number;
    age: number;
  };
}

export interface WiFiConfig {
  ssid: string;
  password: string;
  hostname: string;
  autoReconnect: boolean;
  reconnectDelayMs: number;
  maxReconnectDelayMs: number;
}

export interface MQTTConfig {
  enabled: boolean;
  broker: string;
  port: number;
  username: string;
  password: string;
  topic: string;
  publishIntervalMs: number;
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
  timezone: string;
  gmtOffsetSec: number;
  daylightOffsetSec: number;
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

export interface Config {
  deviceName: string;
  timezone: string;
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
}

export interface RainSensorReading {
  enabled: boolean;
  sensor: string;
  initialized: boolean;
  online: boolean;
  stale: boolean;
  status: number;
  timestamp: number;
  ageMs: number;
  isRaining: boolean;
  raining?: boolean;
  acc: number;
  eventAcc: number;
  totalAcc: number;
  rInt: number;
  accumulation_since_last_read?: number;
  event_accumulation?: number;
  local_event_accumulation?: number;
  hydreon_event_accumulation?: number;
  total_accumulation?: number;
  rain_intensity?: number;
  lensBad: boolean;
  emSat: boolean;
  uart: RG15UartDiagnostics;
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
  encryption: string;
}
