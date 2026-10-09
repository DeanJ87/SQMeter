import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { route } from 'preact-router';
import type { SafetyStatus } from '../types';
import { Button, Card, InfoTip, Note, Pill } from './ui';
import { t } from '../i18n';
import { request } from '../lib/api';
import { formatDateTime, formatTime } from '../i18n/format';
import { deviceText } from '../i18n/deviceMessage';
import { formatDuration } from '../lib/astro';
import { appHref } from '../lib/appHref';

const verdict = (safety: SafetyStatus) => {
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
  kind: 'boot' | 'change' | 'alert' | 'armed';
  boot: number;
  uptime: number;
  timestamp?: number;
  safe?: boolean;
  held?: boolean;
  reasonFlags?: number;
  resetReason?: number;
}

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

export const describeHistoryEntry = (entry: HistoryEntry) => {
  if (entry.kind === 'boot')
    return t('safetyCard.deviceValue', {
      value: RESET_LABELS[entry.resetReason ?? 0] ?? t('safetyCard.restartedReasonResetreason', { resetReason: entry.resetReason }),
    });
  if (entry.kind === 'alert') return entry.safe ? t('safetyCard.alertSentSafe') : t('safetyCard.alertSentUnsafe');
  if (entry.kind === 'armed') return entry.safe ? t('safetyCard.alertsResumed') : t('safetyCard.alertsPaused');
  if (entry.safe) return t('safetyCard.safe');
  if (entry.held) return t('safetyCard.unsafeWaitingOutTheSafe');
  const reasons = REASON_LABELS.filter(([bit]) => (entry.reasonFlags ?? 0) & bit).map(([, label]) => label);
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
        <li key={index} class={`history-${entry.kind}${entry.kind === 'change' ? (entry.safe ? ' is-safe' : ' is-unsafe') : ''}`}>
          <span class="history-time">{historyTime(entry, data.boot)}</span>
          <span>{describeHistoryEntry(entry)}</span>
        </li>
      ))}
    </ul>
  );
};

// Rain that has stopped but still holds the verdict, and when it lets go (specs/025 FR-012).
const RainHold: FunctionalComponent<{ seconds?: number }> = ({ seconds }) =>
  seconds ? (
    <div data-inventory="rain-hold">
      <Note tone="warn">{t('glance.rainHeld', { duration: formatDuration(seconds * 1000) })}</Note>
    </div>
  ) : null;

// What the verdict doesn't cover, one row each (DS-05, DS-24): rules switched
// on whose sensor is off (spec 020, 025 FR-012) - the device lists them only
// while the rain sensor is off - and, with Alpaca off, imaging apps that
// can't see the verdict (spec 007).
const Limits: FunctionalComponent<{ rules?: string[]; alpacaEnabled: boolean }> = ({ rules = [], alpacaEnabled }) =>
  rules.length || !alpacaEnabled ? (
    <ul class="status-rows" aria-label={t('glance.rulesNotInEffect')}>
      {rules.map((rule) => (
        <li key={rule} class="status-row" data-inventory="rules-not-in-effect">
          <a class="status-row-main" href={appHref('/settings?tab=sensors')} title={t('glance.openSettings')}>
            <span class="status-row-label">{deviceText(rule)}</span>
            <Pill tone="pill-amber">{t('status.notInEffect')}</Pill>
          </a>
          <InfoTip text={t('settingsDeps.rainSensorIsOff')} />
        </li>
      ))}
      {!alpacaEnabled && (
        <li class="status-row" data-inventory="safety-not-shared">
          <a class="status-row-main" href={appHref('/settings?tab=safety&section=alpaca')} title={t('glance.openSettings')}>
            <span class="status-row-label">{t('safetyCard.imagingApps')}</span>
            <Pill tone="pill-amber">{t('safetyCard.notShared')}</Pill>
          </a>
          <InfoTip text={t('safetyCard.notSharedHint')} />
        </li>
      )}
    </ul>
  ) : null;

const SafetyCard: FunctionalComponent<{ safety?: SafetyStatus | null; showRulesLink?: boolean; rainClearInSeconds?: number }> = ({
  safety,
  showRulesLink = true,
  rainClearInSeconds,
}) => {
  const [showHistory, setShowHistory] = useState(false);
  if (!safety) return null;
  const state = verdict(safety);

  return (
    <Card
      title={t('safetyCard.safetyMonitor')}
      icon="eye"
      tone={safety.safe ? 'green' : 'red'}
      actions={<Pill tone={state.tone}>{state.text}</Pill>}
    >
      <div class="card-body">
        {safety.reasons.length > 0 ? (
          <ul class="reason-list" aria-label={t('safetyCard.unsafeReasons')}>
            {safety.reasons.map((reason) => (
              <li key={reason}>{deviceText(reason)}</li>
            ))}
          </ul>
        ) : safety.safe ? (
          <Note>{t('safetyCard.allRulesPass')}</Note>
        ) : (
          <Note tone="warn">{t('safetyCard.waitingOutTheSafeDelay')}</Note>
        )}
        <RainHold seconds={rainClearInSeconds} />
        <Limits rules={safety.rulesNotInEffect} alpacaEnabled={safety.alpacaEnabled} />
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
