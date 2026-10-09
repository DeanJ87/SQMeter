import type { AlpacaClientState } from '../types';
import { t } from '../i18n';
import { formatAgo } from '../i18n/format';

// The imaging app as the device sees it (specs/021), as a one- or two-word
// state for a pill and, separately, when it last checked (specs/026 DS-21):
// "Connected" + "Checked 3 s ago". Ages come from the device's uptime clock.

export interface ClientView {
  state: string;
  checked?: string;
  tone: 'ok' | 'warn' | 'muted';
}

export const describeClient = (client: AlpacaClientState): ClientView => {
  const checked = client.lastCheckedAgeMs === null ? undefined : t('alpacaClients.checkedAgo', { ago: formatAgo(client.lastCheckedAgeMs) });
  if (client.silent) return { state: t('alpacaClients.goneQuiet'), checked, tone: 'warn' };
  if (client.connected) return { state: t('alpacaClients.connected'), checked, tone: 'ok' };
  if (client.watching) return { state: t('alpacaClients.checking'), checked, tone: 'ok' };
  if (client.lastCheckedAgeMs !== null) return { state: t('alpacaClients.disconnected'), checked, tone: 'muted' };
  return { state: t('alpacaClients.waiting'), tone: 'muted' };
};

export const CLIENT_PILL = { ok: 'pill-green', warn: 'pill-red', muted: 'pill-dim' } as const;
