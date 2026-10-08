import { describe, expect, it } from 'vitest';
import { skyTemperatureFor } from '../simulator';

// The device's cloud cover: corrected = (sky - irAmbient) - correction/100 * humidity,
// clear below clearSkyThreshold, overcast above cloudyThreshold.
const corrected = (sky: number, irAmbient: number, humidity: number, correction: number) => sky - irAmbient - (correction / 100) * humidity;

describe('skyTemperatureFor', () => {
  it.each([
    { clearSkyThreshold: -13, cloudyThreshold: -3, humidityCorrection: 0.75 },
    { clearSkyThreshold: -30, cloudyThreshold: -20, humidityCorrection: 2 },
    { clearSkyThreshold: -5, cloudyThreshold: 2, humidityCorrection: 0 },
  ])('reaches clear and overcast with thresholds %o', (detection) => {
    const clear = corrected(skyTemperatureFor(0, 10, 70, detection), 10, 70, detection.humidityCorrection);
    const overcast = corrected(skyTemperatureFor(1, 10, 70, detection), 10, 70, detection.humidityCorrection);
    expect(clear).toBeLessThan(detection.clearSkyThreshold);
    expect(overcast).toBeGreaterThan(detection.cloudyThreshold);
  });
});
