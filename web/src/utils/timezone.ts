import { t } from '../i18n';
// Map POSIX timezone strings to friendly names
export const TIMEZONE_MAP: Record<string, string> = {
  UTC0: 'UTC',
  'PST8PDT,M3.2.0,M11.1.0': t('timezone.usPacificPstPdt'),
  'MST7MDT,M3.2.0,M11.1.0': t('timezone.usMountainMstMdt'),
  'CST6CDT,M3.2.0,M11.1.0': t('timezone.usCentralCstCdt'),
  'EST5EDT,M3.2.0,M11.1.0': t('timezone.usEasternEstEdt'),
  'GMT0BST,M3.5.0/1,M10.5.0': t('timezone.europeLondonGmtBst'),
  'CET-1CEST,M3.5.0,M10.5.0/3': t('timezone.europeParisCetCest'),
  'AEST-10AEDT,M10.1.0,M4.1.0/3': t('timezone.australiaSydneyAestAedt'),
  'JST-9': t('timezone.asiaTokyoJst'),
};

export function getTimezoneFriendlyName(posix: string): string {
  return TIMEZONE_MAP[posix] || posix;
}
