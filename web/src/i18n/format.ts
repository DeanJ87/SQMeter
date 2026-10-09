import { currentLanguage, t } from './index';

// Numbers, durations and clock times in the active language (FR-017): the
// decimal separator, digit grouping and 12/24-hour clock follow the language,
// units stay their own setting. Digits are always Latin (0-9), in every
// language: readings, units and coordinates sit in left-to-right runs (spec
// 023 research D14), and the same digits on the device, in MQTT and on the
// page avoid misreading a value.

/** The formatting locale: the active language with Latin digits. */
export const formatLocale = (language: string = currentLanguage()) => `${language}-u-nu-latn`;

/** A reading with a fixed number of decimals, e.g. 21.48 / 21,48; '--' when missing. Never grouped. */
export const formatNumber = (value: number | undefined | null, digits: number) =>
  typeof value === 'number' && Number.isFinite(value)
    ? value.toLocaleString(formatLocale(), { minimumFractionDigits: digits, maximumFractionDigits: digits, useGrouping: false })
    : '--';

/** A count or size, grouped the language's way (1,234 / 1.234 / 1 234); '--' when missing. */
export const formatCount = (value: number | undefined | null, digits = 0) =>
  typeof value === 'number' && Number.isFinite(value)
    ? value.toLocaleString(formatLocale(), { minimumFractionDigits: digits, maximumFractionDigits: digits })
    : '--';

/** A number to `digits` significant figures, e.g. 0.000294 lux. */
export const formatSignificant = (value: number, digits: number) =>
  value.toLocaleString(formatLocale(), { minimumSignificantDigits: digits, maximumSignificantDigits: digits, useGrouping: false });

/** A number as typed into an input: the language's decimal separator, no grouping, no rounding. */
export const formatInputNumber = (value: number) =>
  Number.isFinite(value) ? value.toLocaleString(formatLocale(), { maximumFractionDigits: 10, useGrouping: false }) : '';

/** Bytes as "512 B", "12.50 KB", "1.25 MB". */
export const formatBytes = (bytes: number | undefined | null) => {
  if (typeof bytes !== 'number' || !Number.isFinite(bytes)) return '--';
  if (bytes < 1024) return `${formatCount(bytes)} B`;
  if (bytes < 1048576) return `${formatNumber(bytes / 1024, 2)} KB`;
  return `${formatNumber(bytes / 1048576, 2)} MB`;
};

/** "51.507400 N, 0.127800 W" - "; " between the two where the language writes decimals with a comma. */
export const formatCoordinates = (latitude: number, longitude: number, digits: number, degrees = false) => {
  const separator = decimalSeparator() === ',' ? '; ' : ', ';
  const unit = degrees ? '°' : '';
  const lat = `${formatNumber(Math.abs(latitude), digits)}${unit} ${latitude >= 0 ? 'N' : 'S'}`;
  const lon = `${formatNumber(Math.abs(longitude), digits)}${unit} ${longitude >= 0 ? 'E' : 'W'}`;
  return `${lat}${separator}${lon}`;
};

/** "51.5074, -0.1278" (or "51,5074; -0,1278") for a coordinates input; up to 4 decimals. */
export const formatCoordinatesInput = (latitude: number, longitude: number) => {
  const number = (value: number) => value.toLocaleString(formatLocale(), { maximumFractionDigits: 4, useGrouping: false });
  return `${number(latitude)}${decimalSeparator() === ',' ? '; ' : ', '}${number(longitude)}`;
};

/** The language's decimal separator ("." or ","). */
export const decimalSeparator = (language: string = currentLanguage()) =>
  new Intl.NumberFormat(formatLocale(language)).formatToParts(1.5).find((part) => part.type === 'decimal')?.value ?? '.';

/** A clock time (hours and minutes) in the language's style. */
export const formatTime = (date: Date) => date.toLocaleTimeString(formatLocale(), { hour: '2-digit', minute: '2-digit' });

/** Date and time, e.g. for history entries. */
export const formatDateTime = (date: Date, options?: Intl.DateTimeFormatOptions) => date.toLocaleString(formatLocale(), options);

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
  if (value < 1000) return t('units.milliseconds', { n: formatCount(value) });
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
