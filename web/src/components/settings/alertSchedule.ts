import type { AlertSchedule, AlertSendMode, AlpacaClientState } from '../../types';
import { formatClock, formatDuration } from '../../lib/astro';
import { t } from '../../i18n';

// "When to send" (specs/021): the labels and the status sentence. The
// wording is part of the spec - change it there first.

export const SEND_MODE_OPTIONS: { value: AlertSendMode; label: string; help: string }[] = [
  { value: 'any', label: t('settings.alertSchedule.anyTime'), help: t('settings.alertSchedule.alertsGoOutWheneverSomething') },
  {
    value: 'whileConnected',
    label: t('settings.alertSchedule.onlyWhileAnImagingApp'),
    help: t('settings.alertSchedule.alertsStartWhenAnImaging') + t('settings.alertSchedule.ifItStopsRespondingWithout'),
  },
];

export const PAUSE_HINT = t('settings.alertSchedule.takesEffectStraightAwayHome');

// "at 21:04", "5 min ago", or nothing when it happened before this boot.
const when = (schedule: AlertSchedule) => {
  if (schedule.since) return t('settings.alertSchedule.sinceAt', { clock: formatClock(new Date(schedule.since)) });
  if (schedule.sinceAgeMs !== undefined && schedule.sinceAgeMs !== null)
    return t('settings.alertSchedule.sinceAgo', { duration: formatDuration(schedule.sinceAgeMs) });
  return '';
};

// The current state as a sentence with its reason and since when.
export const describeSchedule = (schedule: AlertSchedule): string => {
  const at = when(schedule);
  const whileConnected = schedule.mode === 'whileConnected';
  if (schedule.armed) {
    if (whileConnected && schedule.reason === 'client-connected') return t('settings.alertSchedule.sendingAlertsAnImagingApp', { at });
    return t('settings.alertSchedule.sendingAlerts');
  }
  const resume = whileConnected
    ? t('settings.alertSchedule.alertsResumeWhenAnImaging')
    : t('settings.alertSchedule.alertsResumeWhenYouResume');
  switch (schedule.reason) {
    case 'client-disconnected':
      return t('settings.alertSchedule.pausedTheImagingAppDisconnected', { at });
    case 'waiting-for-client':
      return t('settings.alertSchedule.waitingForAnImagingApp');
    case 'user-ui':
      return t('settings.alertSchedule.pausedByYouAtPause', { at, resume });
    case 'user-rest':
      return t('settings.alertSchedule.pausedByAScriptAt', { at, resume });
    case 'user-mqtt':
      return t('settings.alertSchedule.pausedFromHomeAssistantOr', { at, resume });
    case 'migrated':
      return t('settings.alertSchedule.pausedBeforeTheUpdateResume', { resume });
    default:
      return t('settings.alertSchedule.paused', { resume });
  }
};

// A silent imaging app, shown whether or not alerts are paused, e.g. "The
// imaging app has gone quiet - safety monitor last checked 4m ago."
export const describeSilentClients = (clients: { safetymonitor: AlpacaClientState; observingconditions: AlpacaClientState }) => {
  const silent = (
    [
      [t('settings.alertSchedule.safetyMonitor'), clients.safetymonitor],
      [t('settings.alertSchedule.weatherDevice'), clients.observingconditions],
    ] as const
  )
    .filter(([, state]) => state.silent)
    .map(([name, state]) =>
      state.lastCheckedAgeMs === null
        ? name
        : t('settings.alertSchedule.lastCheckedAgo', { name, ago: formatDuration(state.lastCheckedAgeMs) }),
    );
  return silent.length ? t('settings.alertSchedule.theImagingAppHasGone', { join: silent.join(', ') }) : null;
};

// Settings are in seconds, shown in minutes.
export const secondsToMinutes = (seconds: number) => Math.round((seconds / 60) * 10) / 10;
export const minutesToSeconds = (minutes: number) => Math.round(minutes * 60);
