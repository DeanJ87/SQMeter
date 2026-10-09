import { useEffect, useState } from 'preact/hooks';
import type { AlertsRecent } from '../types';
import { getJson, post } from '../lib/api';

const POLL_MS = 20000;

// The last alerts the device sent (GET /api/alerts/recent, polled), with
// pause/resume and clear. Requests that fail leave what's shown unchanged.
export const useAlertsRecent = () => {
  const [data, setData] = useState<AlertsRecent | null>(null);

  useEffect(() => {
    const load = () =>
      getJson<AlertsRecent>('/api/alerts/recent')
        .then((body) => body && setData(body))
        .catch(() => undefined);
    load();
    const timer = setInterval(load, POLL_MS);
    return () => clearInterval(timer);
  }, []);

  const armed = data?.armed !== false;

  const switchAlerts = () =>
    post(`${armed ? '/api/alerts/disarm' : '/api/alerts/arm'}?source=ui`)
      .then((response) => response.ok && data && setData({ ...data, armed: !armed }))
      .catch(() => undefined);

  const clear = () =>
    post('/api/alerts/clear')
      .then((response) => response.ok && data && setData({ ...data, alerts: [] }))
      .catch(() => undefined);

  return { data, armed, switchAlerts, clear };
};
