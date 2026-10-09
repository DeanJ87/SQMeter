import type { AlertSchedule, AlpacaClientState } from './alerts';
import type { SensorHealth } from './readings';

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
  configRevision?: number; // goes up whenever the device saves its settings
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
    /** Unix seconds; 0 until the device's clock is set. Missing from firmware before this field existed. */
    epoch?: number;
    /** Local time with its offset, for display. */
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
    // spec 015: the setting the device is running with and its current addresses
    ipv6?: { enabled: boolean; addresses: { address: string; scope: 'link-local' | 'unique-local' | 'global' }[] };
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
