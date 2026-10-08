import { FunctionalComponent } from 'preact';
import { useEffect, useRef, useState } from 'preact/hooks';
import { route } from 'preact-router';
import type { AlertRecord, AlertsRecent } from '../types';
import { Button, Note } from './ui';

const POLL_MS = 20000;
const SEEN_KEY = 'sqm.alerts.lastSeenId';

const readSeen = () => {
  try {
    return Number(localStorage.getItem(SEEN_KEY)) || 0;
  } catch {
    return 0;
  }
};

const writeSeen = (id: number) => {
  try {
    localStorage.setItem(SEEN_KEY, String(id));
  } catch {
    // Private mode etc. - unread counts just won't persist.
  }
};

export const formatAlertAge = (seconds: number) => {
  if (seconds < 60) return `${seconds}s ago`;
  if (seconds < 3600) return `${Math.floor(seconds / 60)}m ago`;
  if (seconds < 86400) return `${Math.floor(seconds / 3600)}h ago`;
  return `${Math.floor(seconds / 86400)}d ago`;
};

export const AlertList: FunctionalComponent<{ alerts: AlertRecord[] }> = ({ alerts }) => (
  <ul class="event-list" aria-label="Recent alerts">
    {alerts.map((record) => (
      <li key={record.id}>
        <div class="event-head">
          <strong>{record.title}</strong>
          <span>{formatAlertAge(record.ageSeconds)}</span>
        </div>
        <p>{record.message}</p>
        <div class="event-channels">
          {Object.entries(record.channels).map(([channel, result]) => (
            <span key={channel} class={`event-${result?.status ?? 'pending'}`} title={result?.detail}>
              {channel}: {result?.status}
              {result?.status === 'failed' && result.detail ? ` - ${result.detail}` : ''}
            </span>
          ))}
        </div>
      </li>
    ))}
  </ul>
);

// Bell in the header with the last alerts the device sent. Only shown while
// alerts are switched on.
const AlertsBell: FunctionalComponent = () => {
  const [data, setData] = useState<AlertsRecent | null>(null);
  const [open, setOpen] = useState(false);
  // The nav scrolls horizontally on phones, which would clip a dropdown, so
  // the flyout is fixed-positioned under the bell instead.
  const [anchorBottom, setAnchorBottom] = useState(0);
  const [seen, setSeen] = useState(readSeen);
  const root = useRef<HTMLDivElement>(null);

  useEffect(() => {
    const load = () =>
      fetch('/api/alerts/recent')
        .then((response) => (response.ok ? response.json() : null))
        .then((body: AlertsRecent | null) => body && setData(body))
        .catch(() => undefined);
    load();
    const timer = setInterval(load, POLL_MS);
    return () => clearInterval(timer);
  }, []);

  useEffect(() => {
    if (!open) return undefined;
    const close = (event: Event) => {
      if (event instanceof KeyboardEvent ? event.key === 'Escape' : !root.current?.contains(event.target as Node)) setOpen(false);
    };
    document.addEventListener('mousedown', close);
    document.addEventListener('keydown', close);
    return () => {
      document.removeEventListener('mousedown', close);
      document.removeEventListener('keydown', close);
    };
  }, [open]);

  if (!data?.enabled) return null;

  const newest = data.alerts[0]?.id ?? 0;
  const unread = data.alerts.filter((record) => record.id > seen && record.event !== 'test').length;

  const armed = data.armed !== false;
  const switchAlerts = () =>
    fetch(`${armed ? '/api/alerts/disarm' : '/api/alerts/arm'}?source=ui`, { method: 'POST' })
      .then((response) => response.ok && setData({ ...data, armed: !armed }))
      .catch(() => undefined);

  const clear = () =>
    fetch('/api/alerts/clear', { method: 'POST' })
      .then((response) => response.ok && setData({ ...data, alerts: [] }))
      .catch(() => undefined);

  const toggle = () => {
    if (!open && newest > seen) {
      writeSeen(newest);
      setSeen(newest);
    }
    setAnchorBottom(root.current?.getBoundingClientRect().bottom ?? 56);
    setOpen(!open);
  };

  return (
    <div class="alerts-bell" ref={root}>
      <button
        type="button"
        class={`nav-button alerts-bell-button${armed ? '' : ' is-off'}`}
        aria-label={`Alerts${armed ? '' : ' (paused)'}${unread ? `, ${unread} new` : ''}`}
        aria-expanded={open}
        onClick={toggle}
      >
        <svg class="nav-icon-svg" width="15" height="15" viewBox="0 0 24 24" aria-hidden="true">
          <path
            stroke="currentColor"
            stroke-width="1.7"
            fill="none"
            stroke-linecap="round"
            stroke-linejoin="round"
            d="M6 16V11a6 6 0 1 1 12 0v5l2 2H4l2-2ZM10 20a2 2 0 0 0 4 0"
          />
          {!armed && <path stroke="currentColor" stroke-width="1.7" stroke-linecap="round" d="M4 4l16 16" />}
        </svg>
        {unread > 0 && <span class="alerts-bell-count">{unread > 9 ? '9+' : unread}</span>}
      </button>
      {open && (
        <div class="alerts-flyout" role="dialog" aria-label="Recent alerts" style={{ top: `${anchorBottom + 6}px` }}>
          <div class="alerts-flyout-head">
            <h2>Alerts</h2>
            <Button variant="link" onClick={switchAlerts} title={armed ? 'Nothing is sent until you resume them' : undefined}>
              {armed ? 'Pause' : 'Resume'}
            </Button>
            {data.alerts.length > 0 && (
              <Button variant="link" onClick={clear}>
                Clear
              </Button>
            )}
            <Button
              variant="link"
              onClick={() => {
                setOpen(false);
                route('/settings?tab=alerts');
              }}
            >
              Settings
            </Button>
          </div>
          {!armed && <Note tone="warn">Alerts are paused - nothing is sent until they're resumed.</Note>}
          {data.alerts.length === 0 ? <Note>No alerts.</Note> : <AlertList alerts={data.alerts} />}
        </div>
      )}
    </div>
  );
};

export default AlertsBell;
