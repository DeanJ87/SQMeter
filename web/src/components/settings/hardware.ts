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

const OK = 0;

const i2cSensor = (sensor?: { initialized: boolean; status: number }): SensorAvailability => ({
  enabled: true,
  detected: sensor ? sensor.initialized && sensor.status === OK : null,
  savedEnabled: true,
});

export const deriveHardware = (config: Config, status: SystemStatus | null): Hardware => {
  const sensors = status?.sensors;
  const rainRunning = sensors?.rg15?.enabled ?? null;
  const windRunning = sensors?.wind?.enabled ?? null;
  const windEnabled = config.wind?.enabled ?? false;

  return {
    statusLoaded: status !== null,
    skyLight: i2cSensor(sensors?.tsl2591),
    irSky: i2cSensor(sensors?.mlx90614),
    environment: i2cSensor(sensors?.bme280),
    rain: {
      enabled: config.rain?.enabled ?? false,
      // Only meaningful if the device is already running with the sensor on.
      detected: rainRunning ? Boolean(sensors?.rg15?.online && !sensors?.rg15?.stale) : null,
      savedEnabled: rainRunning,
    },
    wind: {
      enabled: windEnabled,
      detected: windRunning ? sensors?.wind?.status === OK : null,
      savedEnabled: windRunning,
    },
    windVane: {
      enabled: windEnabled && (config.wind?.directionEnabled ?? false),
      detected: windRunning ? !(sensors?.wind?.vaneFault ?? false) : null,
      savedEnabled: windRunning,
    },
    gps: {
      enabled: config.gps.enabled,
      detected: sensors?.gps ? sensors.gps.initialized : null,
      savedEnabled: null,
    },
    mqtt: {
      enabled: config.mqtt.enabled,
      connected: status?.mqtt ? status.mqtt.connected : null,
    },
    bleAvailable: status ? Boolean(status.ble?.available) : null,
  };
};

// Reason a feature depending on `sensor` can't be switched on, or null if it can.
export const unavailableReason = (sensor: SensorAvailability, name: string, how: 'enable' | 'wire'): string | null => {
  if (!sensor.enabled) return `${name} is turned off.`;
  if (sensor.detected === false) {
    return how === 'wire'
      ? `${name} wasn't detected - check its wiring, then restart the device.`
      : `${name} isn't responding.`;
  }
  return null;
};
