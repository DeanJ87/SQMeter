import { useState } from 'preact/hooks';
import { bodyOf, post } from '../lib/api';
import { t } from '../i18n';
import { deviceError } from '../i18n/deviceMessage';

// The phone-alarm buttons in Settings > Device: acknowledge, unpair (with a
// confirm step), and the result of the last one.

export const useDeviceActions = () => {
  const [confirmUnpair, setConfirmUnpair] = useState(false);
  const [actionResult, setActionResult] = useState<{ type: 'success' | 'error'; text: string } | null>(null);

  const act = async (url: string, success: string) => {
    setActionResult(null);
    try {
      const response = await post(url);
      const body = await bodyOf(response);
      setActionResult(
        response.ok ? { type: 'success', text: success } : { type: 'error', text: deviceError(body, t('settings.device.failed')) },
      );
    } catch {
      setActionResult({ type: 'error', text: t('settings.device.couldNotReachTheDevice') });
    }
  };

  return { act, confirmUnpair, setConfirmUnpair, actionResult };
};

export type DeviceActions = ReturnType<typeof useDeviceActions>;
