import type { Config, SystemStatus } from '../../types';

// What the device can actually do right now, derived from the (unsaved)
// config plus the live /api/status. Settings use this to grey out options
// whose hardware is switched off or missing, and to warn when something is
// switched on but not working.
//
// `detected === null` means "unknown" (status not loaded yet) - unknown never
// blocks anything; it just doesn't show a warning.

export interface SensorAvailability {
  enabled: boolean;           // switched on in this config (always true for built-in I2C sensors)
  detected: boolean | null;   // found / responding on the device
  savedEnabled: boolean | null; // enabled in the config the device is running
}

export interface Hardware {
  statusLoaded: boolean;
  skyLight: SensorAvailability;    // TSL2591
  irSky: SensorAvailability;       // MLX90614
  environment: SensorAvailability; // BME280
  rain: SensorAvailability;        // RG-15
  wind: SensorAvailability;        // anemometer
  windVane: SensorAvailability;
  gps: SensorAvailability;
  mqtt: { enabled: boolean; connected: boolean | null };
  bleAvailable: boolean | null;
}

const i2cSensor = (sensor?: { status: string }): SensorAvailability => ({
  enabled: true,
  detected: sensor ? sensor.status === 'ok' || sensor.status === 'stale' : null,
  savedEnabled: true,
});

export const deriveHardware = (config: Config, status: SystemStatus | null): Hardware => {
  const sensors = status?.sensors;
  // Optional hardware only appears in status while the device runs with it on.
  const rainRunning = sensors ? Boolean(sensors.rain) : null;
  const windRunning = sensors ? Boolean(sensors.wind) : null;
  const windEnabled = config.wind?.enabled ?? false;

  return {
    statusLoaded: status !== null,
    skyLight: i2cSensor(sensors?.light),
    irSky: i2cSensor(sensors?.infrared),
    environment: i2cSensor(sensors?.environment),
    rain: {
      enabled: config.rain?.enabled ?? false,
      // Only meaningful if the device is already running with the sensor on.
      detected: rainRunning ? sensors?.rain?.status === 'ok' : null,
      savedEnabled: rainRunning,
    },
    wind: {
      enabled: windEnabled,
      detected: windRunning ? sensors?.wind?.status === 'ok' : null,
      savedEnabled: windRunning,
    },
    windVane: {
      enabled: windEnabled && (config.wind?.directionEnabled ?? false),
      detected: windRunning ? sensors?.wind?.vaneStatus !== 'fault' : null,
      savedEnabled: windRunning,
    },
    gps: {
      enabled: config.gps.enabled,
      // 'missing' while enabled: switched on but not started (needs a restart).
      detected: sensors?.gps ? sensors.gps.status !== 'missing' : null,
      savedEnabled: sensors?.gps ? sensors.gps.status !== 'missing' : null,
    },
    mqtt: {
      enabled: config.mqtt.enabled,
      connected: status?.mqtt ? status.mqtt.connected : null,
    },
    bleAvailable: status ? Boolean(status.ble?.available) : null,
  };
};
