import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import {
  darkness,
  formatClock,
  formatDuration,
  moonIllumination,
  moonPhaseName,
  moonPosition,
  MOON_HORIZON,
  nextCrossing,
  skyPhaseLabel,
  skyPhase,
  sunPosition,
} from '../lib/astro';
import NightChart from './NightChart';
import { Card, Pill } from './ui';
import { t } from '../i18n';
import { formatNumber } from '../i18n/format';

const PHASE_TONE = {
  day: 'pill-amber',
  civil: 'pill-amber',
  nautical: 'pill-cyan',
  astronomical: 'pill-cyan',
  night: 'pill-green',
} as const;

// Lit part of the moon. phase 0 new .. 0.5 full .. 1 new; mirrored south of the equator.
const MoonDisc: FunctionalComponent<{ phase: number; southern: boolean }> = ({ phase, southern }) => {
  const r = 10;
  const k = Math.cos(2 * Math.PI * phase);
  const waxing = phase < 0.5;
  const rx = Math.abs(k) * r;
  const limbSweep = waxing ? 1 : 0;
  const termSweep = waxing ? (k > 0 ? 0 : 1) : k > 0 ? 1 : 0;
  return (
    <svg class="moon-disc" viewBox="-12 -12 24 24" width="44" height="44" aria-hidden="true">
      <circle r={r} class="moon-dark" />
      <path
        class="moon-lit"
        transform={southern ? 'scale(-1,1)' : undefined}
        d={`M0,${-r} A${r},${r} 0 0 ${limbSweep} 0,${r} A${rx},${r} 0 0 ${termSweep} 0,${-r} Z`}
      />
    </svg>
  );
};

// `deviceNow` is the device's clock when it has one; otherwise this browser's.
const browserZone = Intl.DateTimeFormat().resolvedOptions().timeZone;

const SunMoonCard: FunctionalComponent<{ latitude: number; longitude: number; deviceNow?: Date }> = ({
  latitude,
  longitude,
  deviceNow,
}) => {
  const [browserNow, setBrowserNow] = useState(() => new Date());
  useEffect(() => {
    const timer = setInterval(() => setBrowserNow(new Date()), 60000);
    return () => clearInterval(timer);
  }, []);
  const now = deviceNow ?? browserNow;

  const sun = sunPosition(now, latitude, longitude);
  const illumination = moonIllumination(now);
  const phase = skyPhase(sun.altitude);
  const moonAt = (date: Date) => moonPosition(date, latitude, longitude).altitude;
  const moonrise = nextCrossing(moonAt, MOON_HORIZON, now, true);
  const moonset = nextCrossing(moonAt, MOON_HORIZON, now, false);
  const dark = darkness(latitude, longitude, -18, now);

  const darkLabel = dark.darkNow
    ? t('sunMoonCard.darkNowUntilClock', { clock: formatClock(dark.end) })
    : dark.start && dark.end
      ? t('sunMoonCard.darkFromTo', {
          from: formatClock(dark.start),
          to: formatClock(dark.end),
          duration: formatDuration(dark.end.valueOf() - dark.start.valueOf()),
        })
      : dark.start
        ? t('sunMoonCard.darkFrom', { from: formatClock(dark.start) })
        : t('sunMoonCard.noAstronomicalDarkTonight');
  const moonTimes = [
    moonrise && { time: moonrise, text: t('sunMoonCard.moonRises', { clock: formatClock(moonrise) }) },
    moonset && { time: moonset, text: t('sunMoonCard.moonSets', { clock: formatClock(moonset) }) },
  ]
    .filter((event): event is { time: Date; text: string } => Boolean(event))
    .sort((a, b) => a.time.valueOf() - b.time.valueOf())
    .map((event) => event.text)
    .join(t('common.listSeparator'));

  return (
    <Card
      title={t('sunMoonCard.sunMoon')}
      icon="moon"
      tone="violet"
      hint={browserZone ? t('sunMoonCard.timesInZone', { zone: browserZone }) : t('sunMoonCard.timesInBrowserZone')}
      actions={<Pill tone={PHASE_TONE[phase]}>{skyPhaseLabel(phase)}</Pill>}
    >
      <div class="sun-moon">
        <MoonDisc phase={illumination.phase} southern={latitude < 0} />
        <div class="sun-moon-summary">
          <strong>
            {t('sunMoonCard.moonphasenameFixedLit', {
              moonPhaseName: moonPhaseName(illumination.phase),
              fixed: formatNumber(illumination.fraction * 100, 0),
            })}
          </strong>
          <span>{darkLabel}</span>
          {moonTimes && <span>{t('sunMoonCard.moonMoontimes', { moonTimes })}</span>}
        </div>
      </div>
      <NightChart latitude={latitude} longitude={longitude} now={now} />
    </Card>
  );
};

export default SunMoonCard;
