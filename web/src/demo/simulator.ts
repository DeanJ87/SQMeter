import { sunPosition } from '../lib/astro';
import type { Place } from './presets';

// The sky the demo's sensors sit under: daylight from the sun's position at
// the device's location and clock. Every other reading is what the visitor
// sets (./conditions.ts); everything derived from them (SQM, cloud cover,
// the safety verdict, alerts) is the firmware's own code in the device core.

// Where the demo's sky is when the settings have no location (London).
export const DEFAULT_LOCATION: Place = { latitude: 51.5074, longitude: -0.1278 };

export const simulatorLocation = (location?: { set?: boolean; latitude: number; longitude: number }): Place =>
  location?.set ? { latitude: location.latitude, longitude: location.longitude } : DEFAULT_LOCATION;

// Illuminance from the sun's altitude: daylight, twilight on a log scale, then a dark sky.
export const skyLux = (sunAltitudeDeg: number) => {
  if (sunAltitudeDeg > 0) return 400 + 100_000 * Math.sin((sunAltitudeDeg * Math.PI) / 180);
  if (sunAltitudeDeg > -18) {
    const t = -sunAltitudeDeg / 18; // 0 at sunset, 1 at astronomical dark
    return Math.pow(10, Math.log10(400) * (1 - t) + Math.log10(0.00028) * t);
  }
  return 0.00028;
};

/** Illuminance the light sensor sees from the sun at `now` (the device's clock) and `place`. */
export const sunLux = (now: Date, place: Place) => skyLux(sunPosition(now, place.latitude, place.longitude).altitude);
