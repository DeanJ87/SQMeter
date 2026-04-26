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
  lightSensor: LightSensorReading;
  environment: EnvironmentReading;
  irTemperature: IRTemperatureReading;
  skyQuality: SkyQuality;
  cloudConditions: CloudConditions;
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
      initialized: boolean;
      status: number;
      lastUpdate: number;
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

export interface Config {
  deviceName: string;
  timezone: string;
  primaryTimeSource: number; // 0=NTP, 1=GPS
  secondaryTimeSource: number; // 0=NTP, 1=GPS
  wifi: WiFiConfig;
  mqtt: MQTTConfig;
  ntp: NTPConfig;
  gps: GPSConfig;
  sensor: SensorConfig;
  rain?: RainSensorConfig;
}

export interface RainSensorReading {
  isRaining: boolean;
  acc: number;
  eventAcc: number;
  totalAcc: number;
  rInt: number;
  lensBad: boolean;
  emSat: boolean;
}

export interface RainSensorConfig {
  enabled: boolean;
  rxPin: number;
  txPin: number;
  baudRate: number;
  mode: 'polling' | 'continuous';
  resolution: 'high' | 'low' | 'switch';
  units: 'metric' | 'imperial' | 'switch';
}

export interface WiFiNetwork {
  ssid: string;
  rssi: number;
  encryption: string;
}
