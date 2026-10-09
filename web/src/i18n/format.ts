import { currentLanguage, t } from './index';

// Numbers, durations and clock times in the active language (FR-017): the
// decimal separator, digit grouping and 12/24-hour clock follow the language,
// units stay their own setting.

/** A reading with a fixed number of decimals, e.g. 21.48 / 21,48; '--' when missing. */
export const formatNumber = (value: number | undefined | null, digits: number) =>
  typeof value === 'number' && Number.isFinite(value)
    ? value.toLocaleString(currentLanguage(), { minimumFractionDigits: digits, maximumFractionDigits: digits, useGrouping: false })
    : '--';

/** A clock time (hours and minutes) in the language's style. */
export const formatTime = (date: Date) => date.toLocaleTimeString(currentLanguage(), { hour: '2-digit', minute: '2-digit' });

/** Date and time, e.g. for history entries. */
export const formatDateTime = (date: Date, options?: Intl.DateTimeFormatOptions) => date.toLocaleString(currentLanguage(), options);

/** A span of minutes as "2h 5m" (or the language's equivalent). */
export const formatMinutes = (totalMinutes: number) => {
  const minutes = Math.max(0, Math.round(totalMinutes));
  const hours = Math.floor(minutes / 60);
  return hours > 0 ? t('units.hoursMinutes', { h: hours, m: minutes % 60 }) : t('units.minutes', { m: minutes });
};

/** An uptime in seconds as "1d 2h 3m" / "2h 3m". */
export const formatUptime = (seconds: number | undefined) => {
  if (typeof seconds !== 'number' || !Number.isFinite(seconds)) return '--';
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  return days > 0 ? t('units.daysHoursMinutes', { d: days, h: hours, m: minutes }) : t('units.hoursMinutes', { h: hours, m: minutes });
};

/** An age in milliseconds: "420 ms", "3.2 s", "4m 10s". */
export const formatAgeMs = (value: number | null | undefined) => {
  if (typeof value !== 'number' || !Number.isFinite(value)) return '--';
  if (value < 1000) return t('units.milliseconds', { n: value });
  if (value < 60000) return t('units.seconds', { n: formatNumber(value / 1000, 1) });
  return t('units.minutesSeconds', { m: Math.floor(value / 60000), s: Math.floor((value % 60000) / 1000) });
};

/** How long ago, from an age in milliseconds: "12 s ago", "4 min ago", "1 h 5 min ago". */
export const formatAgo = (ms: number) => {
  const seconds = Math.round(ms / 1000);
  if (seconds < 60) return t('units.secondsAgo', { n: seconds });
  const minutes = Math.round(seconds / 60);
  return minutes < 60
    ? t('units.minutesAgo', { n: minutes })
    : t('units.hoursMinutesAgo', { h: Math.floor(minutes / 60), m: minutes % 60 });
};
