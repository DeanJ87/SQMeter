import { FunctionalComponent } from 'preact';
import { route } from 'preact-router';
import type { SafetyStatus } from '../types';
import { Card, Pill } from './ui';

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

const SafetyCard: FunctionalComponent<{ safety?: SafetyStatus | null }> = ({ safety }) => {
  if (!safety) return null;
  const state = verdict(safety);

  return (
    <Card
      title="Safety Monitor"
      icon="eye"
      tone={safety.isSafe ? 'green' : 'red'}
      actions={<Pill tone={state.tone}>{state.text}</Pill>}
    >
      {safety.reasons.length > 0 ? (
        <ul class="space-y-1 text-sm" aria-label="Unsafe reasons">
          {safety.reasons.map((reason) => (
            <li key={reason} class="text-red-300">• {reason}</li>
          ))}
        </ul>
      ) : safety.isSafe ? (
        <p class="text-sm text-gray-300">All enabled safety rules pass.</p>
      ) : (
        <p class="text-sm text-amber-300">
          Conditions are safe; holding for the configured safe delay before reporting safe.
        </p>
      )}
      <p class="mt-3 text-xs text-gray-500">
        {safety.isSafe ? 'Safe' : 'Unsafe'} for {formatSince(safety.changedAgeMs)}
        {!safety.alpacaEnabled && ' · Alpaca disabled, so clients like N.I.N.A. are not receiving this verdict'}
      </p>
      <button
        type="button"
        class="mt-2 text-xs text-cyan-300 hover:underline"
        onClick={() => route('/settings?section=alpaca')}
      >
        Safety rules
      </button>
    </Card>
  );
};

export default SafetyCard;
