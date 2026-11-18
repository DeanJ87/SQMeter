import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { route } from 'preact-router';
import type { SafetyHold, SafetyStatus } from '../types';
import { Button, Card, Note, Pill } from './ui';
import { t } from '../i18n';
import { clearHold, request } from '../lib/api';
import { showToast } from './toast';
import { formatDateTime, formatTime } from '../i18n/format';
import { deviceText } from '../i18n/deviceMessage';
import { formatDuration } from '../lib/astro';

const verdict = (safety: SafetyStatus) => {
  if (safety.safe && (safety.secondsUntilUnsafe ?? 0) > 0) {
    return { text: t('safetyCard.unsafeInSecondsuntilunsafeS', { secondsUntilUnsafe: safety.secondsUntilUnsafe }), tone: 'pill-amber' };
  }
  if (safety.safe) return { text: t('safetyCard.safe'), tone: 'pill-green' };
  if (safety.rawSafe && safety.secondsUntilSafe > 0) {
    return { text: t('safetyCard.safeInSecondsuntilsafeS', { secondsUntilSafe: safety.secondsUntilSafe }), tone: 'pill-amber' };
  }
  return { text: t('safetyCard.unsafe'), tone: 'pill-red' };
};

const formatSince = (ms: number) => {
  const seconds = Math.floor(ms / 1000);
  if (seconds < 60) return t('units.secondsShort', { n: seconds });
  if (seconds < 3600) return t('units.minutes', { m: Math.floor(seconds / 60) });
  return t('units.hoursMinutes', { h: Math.floor(seconds / 3600), m: Math.floor((seconds % 3600) / 60) });
};

interface HistoryEntry {
  kind: 'boot' | 'change' | 'alert' | 'armed' | 'holdSet' | 'holdCleared';
  boot: number;
  uptime: number;
  timestamp?: number;
  safe?: boolean;
  held?: boolean;
  reasonFlags?: number;
  resetReason?: number;
  holdId?: string; // holdSet, holdCleared (spec 035)
  holds?: string[]; // the external holds' labels
}

// Another system's safety hold (spec 035), named in the history by its label.
const HOLD_FLAG = 1 << 17;

const REASON_LABELS: [number, string][] = [
  [1 << 0, t('safetyCard.reason.manualOverride')],
  [1 << 1, t('safetyCard.reason.noDataYet')],
  [1 << 2, t('safetyCard.reason.staleData')],
  [1 << 3, t('safetyCard.reason.sensorFault')],
  [1 << 4, t('safetyCard.reason.cloud')],
  [1 << 5, t('safetyCard.reason.sqm')],
  [1 << 6, t('safetyCard.reason.humidity')],
  [1 << 7, t('safetyCard.reason.dewPoint')],
  [1 << 8, t('safetyCard.reason.humiditySensorFault')],
  [1 << 9, t('safetyCard.reason.rain')],
  [1 << 10, t('safetyCard.reason.rainSensorFault')],
  [1 << 11, t('safetyCard.reason.wind')],
  [1 << 12, t('safetyCard.reason.gust')],
  [1 << 13, t('safetyCard.reason.anemometerFault')],
];

// ESP-IDF esp_reset_reason_t
const RESET_LABELS: Record<number, string> = {
  1: t('safetyCard.boot.poweredOn'),
  2: t('safetyCard.boot.resetButton'),
  3: t('safetyCard.restartedSettingsUpdateOrRestart'),
  4: t('safetyCard.boot.crashed'),
  5: t('safetyCard.watchdogInterrupt'),
  6: t('safetyCard.watchdogTask'),
  7: t('safetyCard.boot.watchdog'),
  9: t('safetyCard.boot.brownout'),
};

// The reasons a history entry names: the rules' labels, then each hold by name.
const reasonLabels = (entry: HistoryEntry) => {
  const flags = entry.reasonFlags ?? 0;
  const labels = REASON_LABELS.filter(([bit]) => flags & bit).map(([, label]) => label);
  if (entry.kind !== 'holdCleared' && entry.holds?.length)
    return [...labels, ...entry.holds.map((name) => t('safetyCard.reason.holdNamed', { name }))];
  return flags & HOLD_FLAG ? [...labels, t('safetyCard.reason.hold')] : labels;
};

const describeHoldCleared = (entry: HistoryEntry) => {
  const name = entry.holdId === 'manual' ? t('safetyCard.reason.manualOverride') : (entry.holds?.[0] ?? entry.holdId ?? '');
  const still = reasonLabels(entry);
  return still.length
    ? t('safetyCard.holdClearedStillUnsafe', { name, reasons: still.join(t('common.listSeparator')) })
    : t('safetyCard.holdCleared', { name });
};

export const describeHistoryEntry = (entry: HistoryEntry) => {
  if (entry.kind === 'boot')
    return t('safetyCard.deviceValue', {
      value: RESET_LABELS[entry.resetReason ?? 0] ?? t('safetyCard.restartedReasonResetreason', { resetReason: entry.resetReason }),
    });
  if (entry.kind === 'alert') return entry.safe ? t('safetyCard.alertSentSafe') : t('safetyCard.alertSentUnsafe');
  if (entry.kind === 'armed') return entry.safe ? t('safetyCard.alertsResumed') : t('safetyCard.alertsPaused');
  if (entry.kind === 'holdCleared') return describeHoldCleared(entry);
  if (entry.kind === 'change' && entry.safe) return t('safetyCard.safe');
  if (entry.held) return t('safetyCard.unsafeWaitingOutTheSafe');
  const reasons = reasonLabels(entry);
  return reasons.length ? t('safetyCard.unsafeBecause', { reasons: reasons.join(t('common.listSeparator')) }) : t('safetyCard.unsafe');
};

const historyTime = (entry: HistoryEntry, currentBoot: number) => {
  if (entry.timestamp) {
    const date = new Date(entry.timestamp * 1000);
    const sameDay = date.toDateString() === new Date().toDateString();
    return sameDay ? formatTime(date) : formatDateTime(date, { weekday: 'short', hour: '2-digit', minute: '2-digit' });
  }
  return entry.boot === currentBoot ? `${entry.uptime}s after start` : 'earlier';
};

// A hold set reads like the unsafe change it adds to; a hold cleared like a note.
const historyClass = (entry: HistoryEntry) => {
  if (entry.kind === 'holdSet') return 'history-change is-unsafe';
  if (entry.kind === 'holdCleared') return 'history-alert';
  return `history-${entry.kind}${entry.kind === 'change' ? (entry.safe ? ' is-safe' : ' is-unsafe') : ''}`;
};

const SafetyHistoryList: FunctionalComponent = () => {
  const [data, setData] = useState<{ boot: number; entries: HistoryEntry[] } | null | 'error'>(null);
  useEffect(() => {
    request('/api/safety/history')
      .then((response) => (response.ok ? response.json() : Promise.reject()))
      .then(setData)
      .catch(() => setData('error'));
  }, []);
  if (data === null) return <Note>{t('common.loading')}</Note>;
  if (data === 'error') return <Note tone="bad">{t('safetyCard.couldnTLoadTheHistory')}</Note>;
  return (
    <ul class="history-list" aria-label={t('safetyCard.safetyHistory')}>
      {data.entries.map((entry, index) => (
        <li key={index} class={historyClass(entry)}>
          <span class="history-time">{historyTime(entry, data.boot)}</span>
          <span>{describeHistoryEntry(entry)}</span>
        </li>
      ))}
    </ul>
  );
};

// `Hold "smoke" · Home Assistant · since 21:14 · lifts 01:14` (spec 035 FR-011).
const holdMeta = (hold: SafetyHold) => {
  const since =
    hold.sinceEpoch !== undefined
      ? new Date(hold.sinceEpoch * 1000)
      : hold.sinceAgeMs !== undefined
        ? new Date(Date.now() - hold.sinceAgeMs)
        : null;
  const lifts =
    hold.expiresEpoch !== undefined
      ? new Date(hold.expiresEpoch * 1000)
      : hold.expiresInMs !== undefined
        ? new Date(Date.now() + hold.expiresInMs)
        : null;
  return [
    t('safetyCard.holdMeta', { id: hold.id, source: hold.source }),
    ...(since ? [t('safetyCard.holdSince', { time: formatTime(since) })] : []),
    ...(lifts ? [t('safetyCard.holdLifts', { time: formatTime(lifts) })] : []),
  ].join(' · '); // i18n-ignore: a separator, the same in every language
};

const clear = (id: string) =>
  clearHold(id)
    .then((response) => {
      if (!response.ok && response.status !== 404) throw new Error(String(response.status));
    })
    .catch(() => showToast({ message: t('safetyCard.couldNotClearHold'), tone: 'bad' }));

// Each hold under the other reasons: its reason, who set it and when, and Clear.
const HoldItems: FunctionalComponent<{ holds: SafetyHold[] }> = ({ holds }) => (
  <>
    {holds.map((hold) => (
      <li key={hold.id} class="hold-item" data-inventory="safety-holds">
        {deviceText(hold.reason)}
        <span class="hold-meta">
          {holdMeta(hold)}
          <button type="button" class="note-action" onClick={() => void clear(hold.id)}>
            {t('safetyCard.clearHold')}
          </button>
        </span>
      </li>
    ))}
  </>
);

// Rain that has stopped but still holds the verdict, and when it lets go (specs/025 FR-012).
// Rules that aren't in effect are shown in Settings → Safety, not here (spec 025 amended).
const RainHold: FunctionalComponent<{ seconds?: number }> = ({ seconds }) =>
  seconds ? (
    <div data-inventory="rain-hold">
      <Note tone="warn">{t('glance.rainHeld', { duration: formatDuration(seconds * 1000) })}</Note>
    </div>
  ) : null;

const SafetyCard: FunctionalComponent<{ safety?: SafetyStatus | null; showRulesLink?: boolean; rainClearInSeconds?: number }> = ({
  safety,
  showRulesLink = true,
  rainClearInSeconds,
}) => {
  const [showHistory, setShowHistory] = useState(false);
  if (!safety) return null;
  const state = verdict(safety);
  const holds = safety.holds ?? [];
  // A hold's reason is in `reasons` too: said once, with the hold.
  const holdReasons = new Set(holds.map((hold) => hold.reason));
  const reasons = safety.reasons.filter((reason) => !holdReasons.has(reason));

  return (
    <Card
      title={t('safetyCard.safetyMonitor')}
      icon="eye"
      tone={safety.safe ? 'green' : 'red'}
      actions={<Pill tone={state.tone}>{state.text}</Pill>}
    >
      <div class="card-body">
        {reasons.length > 0 || holds.length > 0 ? (
          <ul class="reason-list" aria-label={t('safetyCard.unsafeReasons')}>
            {reasons.map((reason) => (
              <li key={reason}>{deviceText(reason)}</li>
            ))}
            <HoldItems holds={holds} />
          </ul>
        ) : safety.safe ? (
          <Note>{t('safetyCard.allRulesPass')}</Note>
        ) : (
          <Note tone="warn">{t('safetyCard.waitingOutTheSafeDelay')}</Note>
        )}
        <RainHold seconds={rainClearInSeconds} />
        <Note action={showRulesLink ? { label: t('safetyCard.rules'), onClick: () => route('/settings?tab=safety') } : undefined}>
          {t(safety.safe ? 'safetyCard.safeFor' : 'safetyCard.unsafeFor', { duration: formatSince(safety.changedAgeMs) })}
        </Note>
        <div class="btn-row">
          <Button variant="link" onClick={() => setShowHistory(!showHistory)}>
            {showHistory ? t('safetyCard.hideHistory') : t('safetyCard.history')}
          </Button>
        </div>
        {showHistory && <SafetyHistoryList />}
      </div>
    </Card>
  );
};

export default SafetyCard;
