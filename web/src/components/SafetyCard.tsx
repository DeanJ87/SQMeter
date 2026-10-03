import { FunctionalComponent } from 'preact';
import { route } from 'preact-router';
import type { SafetyStatus } from '../types';
import { Card, Note, Pill } from './ui';

const verdict = (safety: SafetyStatus) => {
  if (safety.isSafe) return { text: 'Safe', tone: 'pill-green' };
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

const SafetyCard: FunctionalComponent<{ safety?: SafetyStatus | null; showRulesLink?: boolean }> = ({ safety, showRulesLink = true }) => {
  if (!safety) return null;
  const state = verdict(safety);

  return (
    <Card title="Safety Monitor" icon="eye" tone={safety.isSafe ? 'green' : 'red'} actions={<Pill tone={state.tone}>{state.text}</Pill>}>
      <div class="card-body">
        {safety.reasons.length > 0 ? (
          <ul class="reason-list" aria-label="Unsafe reasons">
            {safety.reasons.map((reason) => (
              <li key={reason}>{reason}</li>
            ))}
          </ul>
        ) : safety.isSafe ? (
          <Note>All rules pass.</Note>
        ) : (
          <Note tone="warn">Waiting out the safe delay.</Note>
        )}
        <Note action={showRulesLink ? { label: 'Rules', onClick: () => route('/settings?tab=safety') } : undefined}>
          {safety.isSafe ? 'Safe' : 'Unsafe'} for {formatSince(safety.changedAgeMs)}
          {!safety.alpacaEnabled && ' · not shared with N.I.N.A. (Alpaca off)'}
        </Note>
      </div>
    </Card>
  );
};

export default SafetyCard;
