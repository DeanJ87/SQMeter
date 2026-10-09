import { FunctionalComponent, RefObject } from 'preact';
import { useEffect, useRef, useState } from 'preact/hooks';
import { route } from 'preact-router';
import type { AlertRecord } from '../types';
import { useAlertsRecent } from '../hooks/useAlertsRecent';
import { Button, Note } from './ui';
import { useAnnounceChange, useDialogFocus } from '../lib/a11y';
import { t } from '../i18n';
import { deviceText } from '../i18n/deviceMessage';
import { formatAgo } from '../i18n/format';
import { channelLabel, deliveryDetail, deliveryStatusLabel } from '../lib/alertDelivery';

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

export const formatAlertAge = (seconds: number) => formatAgo(seconds * 1000);

export const AlertList: FunctionalComponent<{ alerts: AlertRecord[] }> = ({ alerts }) => (
  <ul class="event-list" aria-label={t('alertsBell.recentAlerts')}>
    {alerts.map((record) => (
      <li key={record.id}>
        <div class="event-head">
          <strong>{deviceText(record.title)}</strong>
          <span>{formatAlertAge(record.ageSeconds)}</span>
        </div>
        <p>{deviceText(record.message)}</p>
        <div class="event-channels">
          {Object.entries(record.channels).map(([channel, result]) => (
            <span key={channel} class={`event-${result?.status ?? 'pending'}`} title={deliveryDetail(result?.detail)}>
              {channelLabel(channel)}: {deliveryStatusLabel(result?.status)}
            </span>
          ))}
        </div>
      </li>
    ))}
  </ul>
);

const BellIcon: FunctionalComponent<{ armed: boolean }> = ({ armed }) => (
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
);

type FlyoutProps = {
  alerts: AlertRecord[];
  armed: boolean;
  top: number;
  flyout: RefObject<HTMLDivElement>;
  onSwitch: () => void;
  onClear: () => void;
  onClose: () => void;
};

const AlertsFlyout: FunctionalComponent<FlyoutProps> = ({ alerts, armed, top, flyout, onSwitch, onClear, onClose }) => (
  <div class="alerts-flyout" role="dialog" aria-labelledby="alerts-flyout-title" ref={flyout} style={{ top: `${top}px` }}>
    <div class="alerts-flyout-head">
      <h2 id="alerts-flyout-title" tabIndex={-1} data-autofocus>
        {/* Shown as "Alerts"; read as "Recent alerts". */}
        <span class="sr-only">{t('alertsBell.recentAlerts')}</span>
        <span aria-hidden="true">{t('alertsBell.alerts')}</span>
      </h2>
      <Button variant="link" onClick={onSwitch} title={armed ? t('alertsBell.nothingIsSentUntilYou') : undefined}>
        {armed ? t('alertsBell.pause') : t('alertsBell.resume')}
      </Button>
      {alerts.length > 0 && (
        <Button variant="link" onClick={onClear}>
          {t('alertsBell.clear')}
        </Button>
      )}
      <Button
        variant="link"
        onClick={() => {
          onClose();
          route('/settings?tab=alerts');
        }}
      >
        {t('alertsBell.settings')}
      </Button>
    </div>
    {!armed && <Note tone="warn">{t('alertsBell.alertsArePausedNothingIs')}</Note>}
    {alerts.length === 0 ? <Note>{t('alertsBell.noAlerts')}</Note> : <AlertList alerts={alerts} />}
  </div>
);

const unreadCount = (alerts: AlertRecord[], seen: number) => alerts.filter((record) => record.id > seen && record.event !== 'test').length;

// Bell in the header with the last alerts the device sent. Only shown while
// alerts are switched on.
const AlertsBell: FunctionalComponent = () => {
  const { data, armed, switchAlerts, clear } = useAlertsRecent();
  const [open, setOpen] = useState(false);
  // The nav scrolls horizontally on phones, which would clip a dropdown, so
  // the flyout is fixed-positioned under the bell instead.
  const [anchorBottom, setAnchorBottom] = useState(0);
  const [seen, setSeen] = useState(readSeen);
  const root = useRef<HTMLDivElement>(null);
  const bell = useRef<HTMLButtonElement>(null);
  const flyout = useRef<HTMLDivElement>(null);
  useDialogFocus(open, flyout, bell);
  // A new alert is announced once (spec 022 FR-009).
  const newestRecord = data?.alerts[0];
  useAnnounceChange(newestRecord?.id, () =>
    newestRecord ? t('alertsBell.newAlertTitle', { title: deviceText(newestRecord.title) }) : null,
  );

  // Escape or a click outside closes the flyout.
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
  const unread = unreadCount(data.alerts, seen);

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
        ref={bell}
        type="button"
        class={`nav-button alerts-bell-button${armed ? '' : ' is-off'}`}
        aria-label={t(armed ? 'alertsBell.bellLabel' : 'alertsBell.bellLabelPaused', { count: unread })}
        aria-expanded={open}
        aria-haspopup="dialog"
        onClick={toggle}
      >
        <BellIcon armed={armed} />
        {unread > 0 && <span class="alerts-bell-count">{unread > 9 ? '9+' : unread}</span>}
      </button>
      {open && (
        <AlertsFlyout
          alerts={data.alerts}
          armed={armed}
          top={anchorBottom + 6}
          flyout={flyout}
          onSwitch={switchAlerts}
          onClear={clear}
          onClose={() => setOpen(false)}
        />
      )}
    </div>
  );
};

export default AlertsBell;
