import { t, type MessageKey } from '../i18n';

// One name per sensor everywhere in the UI (specs/026 DS-27): the names are
// in tools/i18n/glossary/en.json and checked by tools/ui/label_check.py. The
// part number is extra detail, shown where hardware matters (System page).

export type SensorId = 'light' | 'infrared' | 'environment' | 'rain' | 'wind' | 'gps';

const NAME: Record<SensorId, MessageKey> = {
  light: 'sensor.light',
  infrared: 'sensor.infrared',
  environment: 'sensor.environment',
  rain: 'sensor.rain',
  wind: 'sensor.wind',
  gps: 'sensor.gps',
};

export const SENSOR_MODEL: Record<SensorId, string> = {
  light: 'TSL2591',
  infrared: 'MLX90614',
  environment: 'BME280',
  rain: 'RG-15',
  wind: '',
  gps: '',
};

export const sensorName = (sensor: SensorId) => t(NAME[sensor]);
