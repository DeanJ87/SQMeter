import { useEffect, useState } from 'preact/hooks';
import type { AlertsRecent } from '../types';
import type { SystemStatus } from '../types';
import { getJson, post } from '../lib/api';
import { useWebSocket } from './useWebSocket';

// Only a fallback: the device pushes alerts.recentRevision over /ws/status as
// soon as the list changes, and the list is fetched then.
const POLL_MS = 60000;

const load = (setData: (data: AlertsRecent) => void) =>
  getJson<AlertsRecent>('/api/alerts/recent')
    .then((body) => body && setData(body))
    .catch(() => undefined);

// The last alerts the device sent (GET /api/alerts/recent), with pause/resume
// and clear. Requests that fail leave what's shown unchanged.
export const useAlertsRecent = () => {
  const [data, setData] = useState<AlertsRecent | null>(null);
  const { data: status } = useWebSocket<SystemStatus>('/ws/status');
  const revision = status?.alerts?.recentRevision;

  useEffect(() => {
    load(setData);
  }, [revision]);

  useEffect(() => {
    const timer = setInterval(() => load(setData), POLL_MS);
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
