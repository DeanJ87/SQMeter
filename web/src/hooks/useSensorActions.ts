import { useState } from 'preact/hooks';
import type { ConfigPath } from '../components/settings/context';
import type { SkyRole } from '../types';
import { post } from '../lib/api';
import { t } from '../i18n';
import { deviceError } from '../i18n/deviceMessage';

// The sensor actions in Settings > Sensors: TSL2591 dark calibration and the
// RG-15 communication test.

export type ActionResult = { type: 'success' | 'error'; text: string } | null;

// `sensor`: which sky sensor (spec 030); its own calibration section is updated.
export const useDarkCalibration = (applyStored: (changes: [ConfigPath, unknown][]) => void, sensor: SkyRole = 'sqm') => {
  const [calibrating, setCalibrating] = useState(false);
  const [calibrationResult, setCalibrationResult] = useState<ActionResult>(null);

  const calibrateDark = async () => {
    setCalibrating(true);
    setCalibrationResult(null);
    try {
      const response = await post(`/api/sensors/tsl2591/calibrate-dark?sensor=${sensor}`);
      const result = await response.json();
      if (response.ok) {
        // The device saved it; keep the form in step so a later Save doesn't undo it.
        const section = sensor === 'sqml' ? 'skyCalibrationL' : 'skyCalibration';
        applyStored([
          [[section, 'darkVisibleOffset'], result.darkVisibleOffset],
          [[section, 'darkSampleCount'], result.sampleCount],
          [[section, 'darkCalibratedAt'], result.darkCalibratedAt],
        ]);
        setCalibrationResult({ type: 'success', text: t('settings.sensors.darkOffsetSaved') });
      } else {
        setCalibrationResult({ type: 'error', text: deviceError(result, t('settings.sensors.calibrationFailed')) });
      }
    } catch {
      setCalibrationResult({ type: 'error', text: t('settings.sensors.couldNotReachTheDevice') });
    } finally {
      setCalibrating(false);
    }
  };

  return { calibrating, calibrationResult, calibrateDark };
};

const rainReply = (result: { rawResponse?: string }) =>
  result.rawResponse ? t('settings.sensors.repliedWith', { response: result.rawResponse }) : t('settings.sensors.replied');

export const useRainTest = () => {
  const [testingRain, setTestingRain] = useState(false);
  const [rainResult, setRainResult] = useState<ActionResult>(null);

  const testRain = async () => {
    setTestingRain(true);
    setRainResult(null);
    try {
      const response = await post('/api/sensors/rg15/test');
      const result = await response.json();
      setRainResult(
        response.ok
          ? { type: 'success', text: rainReply(result) }
          : { type: 'error', text: deviceError(result, result.hint || t('settings.sensors.noReply')) },
      );
    } catch {
      setRainResult({ type: 'error', text: t('settings.sensors.couldNotReachTheDevice') });
    } finally {
      setTestingRain(false);
    }
  };

  return { testingRain, rainResult, testRain };
};
