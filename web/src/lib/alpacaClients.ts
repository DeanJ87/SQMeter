import type { AlpacaClientState } from '../types';

// The imaging app as the device sees it (specs/021), e.g. "Connected, last
// checked 3 s ago". Ages come from the device's uptime clock.

const ago = (ms: number) => {
  const seconds = Math.round(ms / 1000);
  if (seconds < 60) return `${seconds} s ago`;
  const minutes = Math.round(seconds / 60);
  return minutes < 60 ? `${minutes} min ago` : `${Math.floor(minutes / 60)} h ${minutes % 60} min ago`;
};

export const describeClient = (state: AlpacaClientState): { text: string; tone: 'ok' | 'warn' | 'muted' } => {
  const checked = state.lastCheckedAgeMs === null ? '' : `, last checked ${ago(state.lastCheckedAgeMs)}`;
  if (state.silent) return { text: `Gone quiet${checked}`, tone: 'warn' };
  if (state.connected) return { text: `Connected${checked}`, tone: 'ok' };
  if (state.watching) return { text: `Checked without connecting${checked}`, tone: 'ok' };
  if (state.lastCheckedAgeMs !== null) return { text: `Disconnected${checked}`, tone: 'muted' };
  return { text: 'Waiting for an imaging app', tone: 'muted' };
};
