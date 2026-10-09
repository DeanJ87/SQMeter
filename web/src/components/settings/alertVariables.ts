import type { AlertEventKey } from '../../types';

// The {variables} alert wording can use. The device fills the same names;
// lib/AlertLogic/template-variables.json is the shared list, and
// __tests__/alertVariables.test.ts fails if this file drifts from it.

// Every event.
export const COMMON_VARS = [
  'event',
  'device',
  'time',
  'date',
  'level',
  'sqm',
  'sqm_min',
  'cloud',
  'cloud_max',
  'clear_below',
  'cloudy_above',
  'sky_temp',
  'temp',
  'humidity',
  'humidity_max',
  'dewpoint',
  'dew_margin',
  'pressure',
  'rain_rate',
  'wind',
  'gust',
  'sun_alt',
];

// The extra values an event adds.
export const EVENT_VARS: Partial<Record<AlertEventKey, string[]>> = {
  unsafe: ['reasons', 'reasons_inline', 'reason_count'],
  sensor_fault: ['sensor'],
  sensor_recovered: ['sensor'],
  dew_risk: ['dew_margin_min'],
  client_lost: ['silent_for', 'last_checked', 'client_id'],
  client_back: ['last_checked', 'client_id'],
  client_disconnected: ['client_id'],
};
