import type { AlertSchedule, AlertSendMode, AlpacaClientState } from '../../types';
import { formatClock, formatDuration } from '../../lib/astro';

// "When to send" (specs/021): the labels and the status sentence. The
// wording is part of the spec - change it there first.

export const SEND_MODE_OPTIONS: { value: AlertSendMode; label: string; help: string }[] = [
  { value: 'any', label: 'Any time', help: 'Alerts go out whenever something happens, unless you pause them.' },
  {
    value: 'whileConnected',
    label: 'Only while an imaging app is connected',
    help:
      'Alerts start when an imaging app (e.g. N.I.N.A.) connects the safety monitor or weather device, and stop when it disconnects. ' +
      "If it stops responding without disconnecting, alerts keep coming - and you're told it went quiet.",
  },
];

export const PAUSE_HINT =
  'Takes effect straight away. Home Assistant and scripts can do the same: MQTT <topic>/alerts/armed/set, or POST /api/alerts/disarm and /arm.';

// "at 21:04", "5 min ago", or nothing when it happened before this boot.
const when = (schedule: AlertSchedule) => {
  if (schedule.since) return ` at ${formatClock(new Date(schedule.since))}`;
  if (schedule.sinceAgeMs !== undefined && schedule.sinceAgeMs !== null) return ` ${formatDuration(schedule.sinceAgeMs)} ago`;
  return '';
};

// The current state as a sentence with its reason and since when.
export const describeSchedule = (schedule: AlertSchedule): string => {
  const at = when(schedule);
  const whileConnected = schedule.mode === 'whileConnected';
  if (schedule.armed) {
    if (whileConnected && schedule.reason === 'client-connected') return `Sending alerts - an imaging app connected${at}.`;
    return 'Sending alerts.';
  }
  const resume = whileConnected
    ? ' Alerts resume when an imaging app connects, or when you resume them.'
    : ' Alerts resume when you resume them.';
  switch (schedule.reason) {
    case 'client-disconnected':
      return `Paused - the imaging app disconnected${at}. Alerts resume when it connects again.`;
    case 'waiting-for-client':
      return 'Waiting for an imaging app to connect - nothing is sent until then.';
    case 'user-ui':
      return `Paused by you${at} (Pause button).${resume}`;
    case 'user-rest':
      return `Paused by a script${at} (REST).${resume}`;
    case 'user-mqtt':
      return `Paused from Home Assistant or MQTT${at}.${resume}`;
    case 'migrated':
      return `Paused (before the update).${resume}`;
    default:
      return `Paused.${resume}`;
  }
};

// A silent imaging app, shown whether or not alerts are paused, e.g. "The
// imaging app has gone quiet - safety monitor last checked 4m ago."
export const describeSilentClients = (clients: { safetymonitor: AlpacaClientState; observingconditions: AlpacaClientState }) => {
  const silent = (
    [
      ['safety monitor', clients.safetymonitor],
      ['weather device', clients.observingconditions],
    ] as const
  )
    .filter(([, state]) => state.silent)
    .map(([name, state]) =>
      state.lastCheckedAgeMs === null ? name : `${name} last checked ${formatDuration(state.lastCheckedAgeMs)} ago`,
    );
  return silent.length ? `The imaging app has gone quiet - ${silent.join(', ')}.` : null;
};

// Settings are in seconds, shown in minutes.
export const secondsToMinutes = (seconds: number) => Math.round((seconds / 60) * 10) / 10;
export const minutesToSeconds = (minutes: number) => Math.round(minutes * 60);
