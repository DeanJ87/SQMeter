import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { route } from 'preact-router';
import type { SafetyStatus } from '../types';
import { Button, Card, Note, Pill } from './ui';

const verdict = (safety: SafetyStatus) => {
  if (safety.safe) return { text: 'Safe', tone: 'pill-green' };
  if (safety.rawSafe && safety.secondsUntilSafe > 0) {
    return { text: `Safe in ${safety.secondsUntilSafe}s`, tone: 'pill-amber' };
  }
  return { text: 'Unsafe', tone: 'pill-red' };
};

const formatSince = (ms: number) => {
  const seconds = Math.floor(ms / 1000);
  if (seconds < 60) return `${seconds}s`;
  if (seconds < 3600) return `${Math.floor(seconds / 60)}m`;
  return `${Math.floor(seconds / 3600)}h ${Math.floor((seconds % 3600) / 60)}m`;
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
  [1 << 0, 'manual override'],
  [1 << 1, 'no data yet'],
  [1 << 2, 'stale data'],
  [1 << 3, 'sensor fault'],
  [1 << 4, 'cloud'],
  [1 << 5, 'SQM'],
  [1 << 6, 'humidity'],
  [1 << 7, 'dew point'],
  [1 << 8, 'humidity sensor fault'],
  [1 << 9, 'rain'],
  [1 << 10, 'rain sensor fault'],
  [1 << 11, 'wind'],
  [1 << 12, 'gust'],
  [1 << 13, 'anemometer fault'],
];

// ESP-IDF esp_reset_reason_t
const RESET_LABELS: Record<number, string> = {
  1: 'powered on',
  2: 'reset button',
  3: 'restarted (settings, update or restart)',
  4: 'crashed',
  5: 'watchdog (interrupt)',
  6: 'watchdog (task)',
  7: 'watchdog',
  9: 'brownout - power dipped',
};

export const describeHistoryEntry = (entry: HistoryEntry) => {
  if (entry.kind === 'boot') return `Device ${RESET_LABELS[entry.resetReason ?? 0] ?? `restarted (reason ${entry.resetReason})`}`;
  if (entry.kind === 'alert') return `Alert sent: ${entry.safe ? 'safe' : 'unsafe'}`;
  if (entry.kind === 'armed') return entry.safe ? 'Alerts resumed' : 'Alerts paused';
  if (entry.safe) return 'Safe';
  if (entry.held) return 'Unsafe - waiting out the safe delay';
  const reasons = REASON_LABELS.filter(([bit]) => (entry.reasonFlags ?? 0) & bit).map(([, label]) => label);
  return `Unsafe${reasons.length ? `: ${reasons.join(', ')}` : ''}`;
};

const historyTime = (entry: HistoryEntry, currentBoot: number) => {
  if (entry.timestamp) {
    const date = new Date(entry.timestamp * 1000);
    const sameDay = date.toDateString() === new Date().toDateString();
    return sameDay
      ? date.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })
      : date.toLocaleString([], { weekday: 'short', hour: '2-digit', minute: '2-digit' });
  }
  return entry.boot === currentBoot ? `${entry.uptime}s after start` : 'earlier';
};

const SafetyHistoryList: FunctionalComponent = () => {
  const [data, setData] = useState<{ boot: number; entries: HistoryEntry[] } | null | 'error'>(null);
  useEffect(() => {
    fetch('/api/safety/history')
      .then((response) => (response.ok ? response.json() : Promise.reject()))
      .then(setData)
      .catch(() => setData('error'));
  }, []);
  if (data === null) return <Note>Loading...</Note>;
  if (data === 'error') return <Note tone="bad">Couldn't load the history.</Note>;
  return (
    <ul class="history-list" aria-label="Safety history">
      {data.entries.map((entry, index) => (
        <li key={index} class={`history-${entry.kind}${entry.kind === 'change' ? (entry.safe ? ' is-safe' : ' is-unsafe') : ''}`}>
          <span class="history-time">{historyTime(entry, data.boot)}</span>
          <span>{describeHistoryEntry(entry)}</span>
        </li>
      ))}
    </ul>
  );
};

const SafetyCard: FunctionalComponent<{ safety?: SafetyStatus | null; showRulesLink?: boolean }> = ({ safety, showRulesLink = true }) => {
  const [showHistory, setShowHistory] = useState(false);
  if (!safety) return null;
  const state = verdict(safety);

  return (
    <Card title="Safety Monitor" icon="eye" tone={safety.safe ? 'green' : 'red'} actions={<Pill tone={state.tone}>{state.text}</Pill>}>
      <div class="card-body">
        {safety.reasons.length > 0 ? (
          <ul class="reason-list" aria-label="Unsafe reasons">
            {safety.reasons.map((reason) => (
              <li key={reason}>{reason}</li>
            ))}
          </ul>
        ) : safety.safe ? (
          <Note>All rules pass.</Note>
        ) : (
          <Note tone="warn">Waiting out the safe delay.</Note>
        )}
        <Note action={showRulesLink ? { label: 'Rules', onClick: () => route('/settings?tab=safety') } : undefined}>
          {safety.safe ? 'Safe' : 'Unsafe'} for {formatSince(safety.changedAgeMs)}
          {!safety.alpacaEnabled && ' · not shared with N.I.N.A. (Alpaca off)'}
        </Note>
        <div class="btn-row">
          <Button variant="link" onClick={() => setShowHistory(!showHistory)}>
            {showHistory ? 'Hide history' : 'History'}
          </Button>
        </div>
        {showHistory && <SafetyHistoryList />}
      </div>
    </Card>
  );
};

export default SafetyCard;
