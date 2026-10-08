import { useEffect, useState } from 'preact/hooks';
import type { AlertSchedule } from '../types';

// Whether alerts are being sent, and Pause/Resume (specs/021). `live` is the
// device's status ("alerts"), which keeps it current when Home Assistant or
// a script pauses alerts; /api/alerts/armed is fetched once for a quick start
// and after the button, whose answer wins over an older status for a moment.

const PREFER_BUTTON_MS = 5000;

const fetchSchedule = async (): Promise<AlertSchedule | null> => {
  const response = await fetch('/api/alerts/armed');
  return response.ok ? response.json() : null;
};

export const useAlertSchedule = (live: AlertSchedule | undefined) => {
  const [fetched, setFetched] = useState<AlertSchedule | null>(null);
  const [busy, setBusy] = useState(false);
  const [preferFetchedUntil, setPreferFetchedUntil] = useState(0);

  useEffect(() => {
    fetchSchedule()
      .then((body) => setFetched(body ?? { armed: true }))
      .catch(() => setFetched({ armed: true }));
  }, []);

  // Pause (false) or resume (true), as the web UI. False if the device couldn't be reached.
  const pauseOrResume = async (resume: boolean) => {
    setBusy(true);
    try {
      const response = await fetch(`${resume ? '/api/alerts/arm' : '/api/alerts/disarm'}?source=ui`, { method: 'POST' });
      if (!response.ok) return false;
      setFetched((await fetchSchedule()) ?? { armed: resume });
      setPreferFetchedUntil(Date.now() + PREFER_BUTTON_MS);
      return true;
    } catch {
      return false;
    } finally {
      setBusy(false);
    }
  };

  const schedule = busy || Date.now() < preferFetchedUntil ? fetched : (live ?? fetched);
  return { schedule, busy, pauseOrResume };
};
