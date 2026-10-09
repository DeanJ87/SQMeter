import type { SystemStatus } from '../../types';
import { t } from '../../i18n';
import { darkness, formatClock, formatDuration } from '../../lib/astro';

// The darkness note under "Only send safety alerts when it's dark"
// (spec 005 FR-005). Dark-or-not is the device's own decision
// (`sky.isNight`, against the SAVED limit); only the start and end times are
// predicted here, in this browser's time zone.

type Sky = SystemStatus['sky'];

export interface DarknessInput {
  sky: Sky | undefined;
  location: { latitude: number; longitude: number } | null;
  formLimitDeg: number; // the limit in the form, possibly unsaved
  deviceNow?: Date;
}

const browserZone = () => {
  const zone = Intl.DateTimeFormat().resolvedOptions().timeZone;
  return zone ? t('settings.alerts.thisBrowserSTimeZone', { zone }) : t('settings.alerts.thisBrowserSTime');
};

export const describeDarkness = ({ sky, location, formLimitDeg, deviceNow }: DarknessInput): string | null => {
  if (!sky) return null;
  if (!sky.nightKnown || sky.sunAltitudeDeg === undefined || sky.isNight === undefined) return t('settings.alerts.darknessUnknown');
  const sun = sky.sunAltitudeDeg;
  const sunNow = t('settings.alerts.sunAtFixedNowValue', { fixed: sun.toFixed(1), value: t('settings.alerts.deviceSuffix') });
  // The device compares against its saved limit; a different answer from
  // the form's limit means that limit isn't saved yet.
  const unsaved = sun < formLimitDeg !== sky.isNight ? t('settings.alerts.saveToApplyLimit') : '';
  if (!location) return t(sky.isNight ? 'settings.alerts.darkNow' : 'settings.alerts.notDark', { sunNow }) + unsaved;

  const now = deviceNow ?? new Date();
  const predicted = darkness(location.latitude, location.longitude, formLimitDeg, now);
  if (sky.isNight) {
    const note = predicted.end
      ? t('settings.alerts.darkUntil', { sunNow, clock: formatClock(predicted.end), inZone: browserZone() })
      : t('settings.alerts.darkNow', { sunNow });
    return note + unsaved;
  }
  const start = predicted.darkNow ? null : predicted.start;
  if (!start) return t('settings.alerts.notDarkYet', { sunNow }) + unsaved;
  const note = t('settings.alerts.sunnowDarkInDurationClock', {
    sunNow,
    duration: formatDuration(start.valueOf() - now.valueOf()),
    clock: formatClock(start),
    clock2: formatClock(predicted.end),
    inZone: browserZone(),
  });
  return note + unsaved;
};
