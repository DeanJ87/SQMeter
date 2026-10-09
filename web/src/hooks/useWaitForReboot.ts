import { useEffect, useRef } from 'preact/hooks';
import { request } from '../lib/api';

const CHECK_MS = 2000;

// While `waiting`, ask /api/status every 2 s; the first answer means the device
// is back from its restart and `onBack` runs once. `onStart` runs when the wait begins.
export const useWaitForReboot = (waiting: boolean, onBack: () => void, onStart?: () => void) => {
  const back = useRef(onBack);
  back.current = onBack;
  const start = useRef(onStart);
  start.current = onStart;

  useEffect(() => {
    let checkInterval: number | undefined;

    if (waiting) {
      start.current?.();
      checkInterval = window.setInterval(async () => {
        try {
          const response = await request('/api/status');
          if (response.ok) {
            back.current();
            window.clearInterval(checkInterval);
          }
        } catch {
          // Still offline, keep waiting
        }
      }, CHECK_MS);
    }

    return () => {
      if (checkInterval) window.clearInterval(checkInterval);
    };
  }, [waiting]);
};
