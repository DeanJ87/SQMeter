import type { SensorData, SystemStatus, Config } from "../types";

const jitter = (base: number, range: number) =>
  base + (Math.random() - 0.5) * range;

export function generateSensorData(): SensorData {
  const sqm = jitter(21.45, 0.08);
  const lux = Math.pow(10, (12.59 - sqm) / 2.5);

  return {
    lightSensor: {
      lux: parseFloat(lux.toFixed(4)),
      visible: Math.round(jitter(312, 10)),
      infrared: Math.round(jitter(48, 4)),
      full: Math.round(jitter(360, 12)),
      status: 0,
    },
    skyQuality: {
      sqm: parseFloat(sqm.toFixed(2)),
      nelm: parseFloat(jitter(6.18, 0.05).toFixed(2)),
      bortle: 2,
      description: "Typical truly dark site",
    },
    environment: {
      temperature: parseFloat(jitter(12.4, 0.2).toFixed(1)),
      humidity: parseFloat(jitter(64.8, 0.5).toFixed(1)),
      pressure: parseFloat(jitter(1013.25, 0.3).toFixed(2)),
      dewpoint: parseFloat(jitter(6.1, 0.2).toFixed(1)),
      status: 0,
    },
    irTemperature: {
      objectTemp: parseFloat(jitter(-24.8, 0.4).toFixed(1)),
      ambientTemp: parseFloat(jitter(12.4, 0.1).toFixed(1)),
      status: 0,
    },
    cloudConditions: {
      temperatureDelta: parseFloat(jitter(-37.2, 0.5).toFixed(1)),
      correctedDelta: parseFloat(jitter(-32.8, 0.4).toFixed(1)),
      cloudCoverPercent: parseFloat(jitter(3.0, 1.0).toFixed(1)),
      condition: 1,
      description: "Clear",
      humidityUsed: 64.8,
    },
    gps: {
      hasFix: true,
      satellites: 9,
      latitude: 51.5074,
      longitude: -0.1278,
      altitude: 42.0,
      hdop: 1.1,
      age: Math.round(jitter(800, 100)),
    },
    rainSensor: {
      rainRate: parseFloat(jitter(2.4, 0.3).toFixed(1)),
      accumulated: parseFloat(jitter(12.6, 0.1).toFixed(1)),
      eventCount: Math.round(jitter(84, 2)),
      status: 1,
    },
  };
}

export const mockStatus: SystemStatus = {
  firmware: {
    name: "SQMeter",
    version: "0.0.1",
    buildDate: "Apr 24 2026",
    buildTime: "12:00:00",
  },
  uptime: 7200,
  freeHeap: 214320,
  heapSize: 327680,
  cpuFreqMHz: 240,
  flashSize: 4194304,
  sketchSize: 1245184,
  freeSketchSpace: 917504,
  fsTotal: 524288,
  fsUsed: 204800,
  partitions: {
    runningSlot: "app0",
    runningAddress: 0x10000,
    runningSize: 1572864,
    bootSlot: "app0",
    nextSlot: "app1",
    nextSize: 1572864,
    nvs: {
      usedEntries: 12,
      freeEntries: 488,
      totalEntries: 500,
      namespaceCount: 1,
    },
    fsAddress: 0x310000,
    fsSize: 524288,
  },
  time: {
    iso: new Date().toISOString(),
    timezone: "GMT0",
  },
  ntp: {
    enabled: true,
    synced: true,
    status: 1,
    lastSync: Date.now() - 3600000,
    nextSync: Date.now() + 3600000,
    drift: 0.003,
    server: "pool.ntp.org",
    activeSource: 1,
    gpsEnabled: true,
    gpsHasFix: true,
    gpsTimeUTC: new Date().toISOString(),
    gpsSatellites: 9,
  },
  wifi: {
    connected: true,
    ssid: "DarkSkyLab",
    ip: "192.168.1.42",
    rssi: -58,
    mac: "AA:BB:CC:DD:EE:FF",
  },
  mqtt: {
    enabled: true,
    connected: true,
    state: 0,
    lastPublish: Date.now() - 60000,
    lastReconnectAttempt: 0,
    broker: "mqtt.example.com",
    port: 1883,
    topic: "sqmeter/data",
  },
  sensors: {
    tsl2591: { initialized: true, status: 0, lastUpdate: Date.now() - 5000 },
    bme280: { initialized: true, status: 0, lastUpdate: Date.now() - 5000 },
    mlx90614: { initialized: true, status: 0, lastUpdate: Date.now() - 5000 },
    gps: { initialized: true, status: 0, lastUpdate: Date.now() - 1000 },
    rg15: { initialized: true, status: 0, lastUpdate: Date.now() - 5000 },
  },
};

export const mockConfig: Config = {
  deviceName: "SQMeter Demo",
  timezone: "GMT0",
  primaryTimeSource: 0,
  secondaryTimeSource: 1,
  wifi: {
    ssid: "DarkSkyLab",
    password: "",
    hostname: "sqmeter",
    autoReconnect: true,
    reconnectDelayMs: 1000,
    maxReconnectDelayMs: 300000,
  },
  ntp: {
    enabled: true,
    server1: "pool.ntp.org",
    server2: "time.cloudflare.com",
    timezone: "GMT0",
    gmtOffsetSec: 0,
    daylightOffsetSec: 3600,
    syncIntervalMs: 3600000,
  },
  gps: {
    enabled: true,
    rxPin: 16,
    txPin: 17,
    baudRate: 9600,
  },
  mqtt: {
    enabled: true,
    broker: "mqtt.example.com",
    port: 1883,
    username: "",
    password: "",
    topic: "sqmeter/data",
    publishIntervalMs: 60000,
  },
  sensor: {
    readIntervalMs: 5000,
    i2cSDA: 21,
    i2cSCL: 22,
    i2cFrequency: 100000,
  },
  rain: {
    enabled: true,
    rxPin: 18,
    txPin: 19,
    baudRate: 9600,
  },
};

export const mockWifiNetworks = [
  { ssid: "DarkSkyLab", rssi: -42, encryption: "WPA2" },
  { ssid: "NeighbourNet", rssi: -71, encryption: "WPA2" },
  { ssid: "TeleCom_5G", rssi: -85, encryption: "WPA3" },
];
