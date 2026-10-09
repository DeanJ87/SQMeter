import type { AlpacaClientState } from '../types';
import { t } from '../i18n';
import { formatAgo } from '../i18n/format';

// The imaging app as the device sees it (specs/021), e.g. "Connected, last
// checked 3 s ago". Ages come from the device's uptime clock.

const ago = formatAgo;

export const describeClient = (state: AlpacaClientState): { text: string; tone: 'ok' | 'warn' | 'muted' } => {
  const checked = state.lastCheckedAgeMs === null ? '' : t('alpacaClients.lastCheckedAgo', { ago: ago(state.lastCheckedAgeMs) });
  if (state.silent) return { text: t('alpacaClients.goneQuietChecked', { checked }), tone: 'warn' };
  if (state.connected) return { text: t('alpacaClients.connectedChecked', { checked }), tone: 'ok' };
  if (state.watching) return { text: t('alpacaClients.checkedWithoutConnectingChecked', { checked }), tone: 'ok' };
  if (state.lastCheckedAgeMs !== null) return { text: t('alpacaClients.disconnectedChecked', { checked }), tone: 'muted' };
  return { text: t('alpacaClients.waitingForAnImagingApp'), tone: 'muted' };
};
