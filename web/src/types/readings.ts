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
  clearInSeconds?: number; // while the clear delay holds rain that has stopped (spec 025)
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
  rulesNotInEffect?: string[]; // safety rules on but not evaluated, e.g. rain sensor off (spec 020)
}
