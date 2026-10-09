import type { SensorData, SystemStatus, GithubRelease, AlpacaConfiguredDevice, AlertRecord, WiFiNetwork } from '../types';

export { mockConfig } from './config';

const jitter = (base: number, range: number) => base + (Math.random() - 0.5) * range;

const round = (value: number, digits: number) => parseFloat(value.toFixed(digits));

const lightReadings = (sqm: number): Pick<SensorData, 'light' | 'sky'> => {
  const lux = Math.pow(10, (12.6 - sqm) / 2.5);
  return {
    light: {
      status: 'ok',
      ageMs: 400,
      lux: round(lux, 6),
      visible: Math.round(jitter(312, 10)),
      infrared: Math.round(jitter(48, 4)),
      full: Math.round(jitter(360, 12)),
      gain: 'MAX',
      gainFactor: 9876,
      integrationMs: 600,
      saturated: false,
      nightMode: true,
    },
    sky: {
      status: 'ok',
      sqm: round(sqm, 2),
      rawSqm: round(sqm - 0.07, 2),
      nelm: round(jitter(6.18, 0.05), 1),
      bortle: 2,
      description: 'Typical truly dark site',
      calibrated: false,
      averagingWindowSeconds: 90,
    },
  };
};

const climateReadings = (): Pick<SensorData, 'environment' | 'infrared' | 'clouds'> => ({
  environment: {
    status: 'ok',
    ageMs: 3100,
    temperature: round(jitter(12.4, 0.2), 1),
    humidity: round(jitter(64.8, 0.5), 1),
    pressure: round(jitter(1013.25, 0.3), 1),
    dewpoint: round(jitter(6.1, 0.2), 1),
  },
  infrared: {
    status: 'ok',
    ageMs: 3100,
    skyTemperature: round(jitter(-24.8, 0.4), 1),
    ambientTemperature: round(jitter(12.4, 0.1), 1),
  },
  clouds: {
    status: 'ok',
    coverPercent: Math.round(jitter(3.0, 1.0)),
    condition: 'clear',
    description: 'Clear',
    temperatureDelta: round(jitter(-37.2, 0.5), 1),
    correctedDelta: round(jitter(-32.8, 0.4), 1),
    humidity: 64.8,
    humiditySource: 'measured',
  },
});

const fieldReadings = (): Pick<SensorData, 'gps' | 'wind' | 'rain' | 'safety'> => ({
  gps: {
    status: 'ok',
    ageMs: Math.round(jitter(800, 100)),
    fix: true,
    satellites: 9,
    latitude: 51.5074,
    longitude: -0.1278,
    altitude: 42.0,
    hdop: 1.1,
  },
  wind: {
    status: 'ok',
    ageMs: 900,
    speed: round(jitter(3.2, 0.4), 1),
    gust: round(jitter(6.8, 0.3), 1),
    direction: Math.round(jitter(247, 8)),
    vaneFault: false,
  },
  rain: {
    status: 'ok',
    ageMs: 40,
    raining: true,
    rainingNow: true,
    intensity: 2.4,
    eventAccumulation: 0.4,
    sensorEventAccumulation: 0.4,
    totalAccumulation: 12.6,
    lensFault: false,
    emitterSaturated: false,
  },
  // Demo data shows rain, so the SafetyMonitor reports unsafe.
  safety: {
    safe: false,
    rawSafe: false,
    alpacaEnabled: true,
    reasonFlags: 1 << 9,
    reasons: ['Rain detected'],
    secondsUntilSafe: 0,
    evaluatedAgeMs: 400,
    changedAgeMs: 1260000,
  },
});

export function generateSensorData(): SensorData {
  const sqm = jitter(21.45, 0.08);
  return {
    timestamp: Math.floor(Date.now() / 1000),
    timeValid: true,
    dataAgeMs: 400,
    dataStale: false,
    ...lightReadings(sqm),
    ...climateReadings(),
    ...fieldReadings(),
  };
}

export const mockStatus: SystemStatus = {
  alerts: { armed: true, armWithAlpaca: false, mode: 'any', reason: 'none', since: null, sinceAgeMs: null },
  alpaca: {
    enabled: true,
    clients: {
      safetymonitor: { connected: true, watching: true, silent: false, lastCheckedAgeMs: 2100, clientId: 4021 },
      observingconditions: { connected: false, watching: false, silent: false, lastCheckedAgeMs: null, clientId: null },
    },
  },
  sky: { locationSource: 'gps', nightKnown: true, isNight: true, sunAltitudeDeg: -24.3 },
  firmware: {
    name: 'SQMeter',
    version: '0.2.0-beta.1',
    buildDate: 'Apr 24 2026',
    buildTime: '12:00:00',
    variant: 'standard',
  },
  ble: {
    available: false,
    active: false,
    clients: 0,
    alarm: { serviceActive: false, active: false, sequence: 0, acknowledgedSequence: 0, bondedPhones: 0 },
  },
  uptime: 7200,
  bootCount: 3,
  resetReason: 3,
  freeHeap: 214320,
  minFreeHeap: 156056,
  maxAllocHeap: 110580,
  heapSize: 327680,
  stackFree: { asyncTcp: 3360, loop: 3612 },
  sensorSnapshotBytes: 808,
  heapStages: [
    { stage: 'boot', free: 279836, largest: 110580 },
    { stage: 'sensors', free: 271484, largest: 110580 },
    { stage: 'wifi', free: 214276, largest: 110580 },
    { stage: 'setup complete', free: 190660, largest: 110580 },
  ],
  cpuFreqMHz: 240,
  flashSize: 4194304,
  sketchSize: 1245184,
  freeSketchSpace: 917504,
  fsTotal: 524288,
  fsUsed: 204800,
  partitions: {
    runningSlot: 'app0',
    runningAddress: 0x10000,
    runningSize: 1572864,
    bootSlot: 'app0',
    nextSlot: 'app1',
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
    epoch: Math.floor(Date.now() / 1000),
    iso: new Date().toISOString(),
    timezone: 'GMT0',
  },
  ntp: {
    enabled: true,
    synced: true,
    status: 1,
    lastSync: Date.now() - 3600000,
    nextSync: Date.now() + 3600000,
    drift: 0.003,
    server: 'pool.ntp.org',
    activeSource: 1,
    gpsEnabled: true,
    gpsHasFix: true,
    gpsTimeUTC: new Date().toISOString(),
    gpsSatellites: 9,
  },
  wifi: {
    connected: true,
    ssid: 'DarkSkyLab',
    ip: '192.168.1.42',
    rssi: -58,
    mac: 'AA:BB:CC:DD:EE:FF',
    connectPending: false,
    apMode: false,
    hostname: 'sqmeter',
    mdns: true,
    ipv6: {
      enabled: true,
      addresses: [
        { address: 'fe80::a3b2:c3ff:fed4:e5f6', scope: 'link-local' },
        { address: '2001:db8:4a2c:1:a3b2:c3ff:fed4:e5f6', scope: 'global' },
      ],
    },
  },
  mqtt: {
    enabled: true,
    connected: true,
    state: 0,
    lastPublish: Date.now() - 60000,
    lastReconnectAttempt: 0,
    broker: 'mqtt.example.com',
    port: 1883,
    topic: 'sqmeter',
    availabilityTopic: 'sqmeter/availability',
    clientId: 'SQMeter-D4E5F6',
  },
  sensors: {
    light: { status: 'ok', ageMs: 400 },
    environment: { status: 'ok', ageMs: 3100 },
    infrared: { status: 'ok', ageMs: 3100 },
    gps: { status: 'ok', ageMs: 800 },
    rain: { status: 'ok', ageMs: 40 },
    wind: { status: 'ok', ageMs: 900, vaneStatus: 'ok' },
  },
  diagnostics: {
    light: {
      rollingVisible: 3.1,
      correctedVisible: 3.1,
      darkVisibleOffset: 0,
      sampleCount: 150,
      windowSamples: 150,
      nightMode: true,
      rejectedSamples: 0,
      consecutiveSaturatedSamples: 0,
      consecutiveLowSamples: 0,
    },
    rain: {
      state: 'online',
      uartOpened: true,
      rxPin: 18,
      txPin: 19,
      baudRate: 9600,
      uartPort: 1,
      lastCommand: 'R',
      lastResponse: 'Acc 0.01 mm, EventAcc 0.40 mm, TotalAcc 12.60 mm, RInt 2.40 mmph',
      timeouts: 0,
      parseErrors: 0,
      successfulReads: 1424,
      lastPollAgeMs: 40,
      lastResponseAgeMs: 40,
      lastSuccessfulReadAgeMs: 40,
      lastRainDetectedAgeMs: 1260000,
      softwareVersion: '1.000',
      softwareBuildDate: 'Sep 23 2020',
      resetReason: 'PwrUp',
      powerOnDays: 13,
      emitterTotal: 19,
    },
  },
};

export const mockWifiNetworks: WiFiNetwork[] = [
  { ssid: 'DarkSkyLab', rssi: -42, encryption: 'secured' },
  { ssid: 'NeighbourNet', rssi: -71, encryption: 'secured' },
  { ssid: 'Observatory-Guest', rssi: -78, encryption: 'open' },
  { ssid: 'TeleCom_5G', rssi: -85, encryption: 'secured' },
];

export const mockGithubReleases: GithubRelease[] = [
  {
    tag: 'v0.1.4',
    name: 'v0.1.4 - RG-15 diagnostics',
    prerelease: false,
    publishedAt: '2026-07-01T12:00:00Z',
    firmwareAssetUrl: 'https://github.com/DeanJ87/SQMeter/releases/download/v0.1.4/sqmeter-firmware-v0.1.4.bin',
    firmwareAssetSize: 1273285,
    fsAssetUrl: 'https://github.com/DeanJ87/SQMeter/releases/download/v0.1.4/sqmeter-littlefs-v0.1.4.bin',
    fsAssetSize: 274432,
  },
  {
    tag: 'v0.2.0-beta.2',
    name: 'v0.2.0-beta.2 - MQTT, Home Assistant and WiFi setup',
    prerelease: true,
    publishedAt: '2026-08-10T09:30:00Z',
    firmwareAssetUrl: 'https://github.com/DeanJ87/SQMeter/releases/download/v0.2.0-beta.2/sqmeter-firmware-v0.2.0-beta.2.bin',
    firmwareAssetSize: 1301022,
    fsAssetUrl: 'https://github.com/DeanJ87/SQMeter/releases/download/v0.2.0-beta.2/sqmeter-littlefs-v0.2.0-beta.2.bin',
    fsAssetSize: 280100,
  },
];

export const mockAlpacaDevices: AlpacaConfiguredDevice[] = [
  { DeviceName: 'SQMeter SafetyMonitor', DeviceType: 'SafetyMonitor', DeviceNumber: 0, UniqueID: 'sqmeter-a1b2c3d4e5f6-safetymonitor-0' },
  {
    DeviceName: 'SQMeter ObservingConditions',
    DeviceType: 'ObservingConditions',
    DeviceNumber: 0,
    UniqueID: 'sqmeter-a1b2c3d4e5f6-observingconditions-0',
  },
];

export const mockRecentAlerts: AlertRecord[] = [
  {
    id: 2,
    event: 'unsafe',
    title: 'Observatory UNSAFE',
    message: '\u2022 SQM 18.21 < 19.50\n\u2022 Cloud 62% >= 35%\n\u2022 Humidity 92% > 90%',
    level: 'urgent',
    ageSeconds: 1260,
    channels: { pushover: { status: 'sent', detail: 'HTTP 200' } },
  },
  {
    id: 1,
    event: 'rain_started',
    title: 'Rain detected',
    message: 'The rain sensor reports rain (2.4 mm/h).',
    level: 'urgent',
    ageSeconds: 1261,
    channels: { pushover: { status: 'sent', detail: 'HTTP 200' } },
  },
];
