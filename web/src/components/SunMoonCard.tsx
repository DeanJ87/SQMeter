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
  SKY_PHASE_LABEL,
  skyPhase,
  sunPosition,
  SUN_HORIZON,
} from '../lib/astro';
import { Card, MetricTile, Pill, ReadingRow } from './ui';

const PHASE_TONE = { day: 'pill-amber', civil: 'pill-amber', nautical: 'pill-cyan', astronomical: 'pill-cyan', night: 'pill-green' } as const;

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

// Upcoming events soonest first; ones that don't happen in the next 36 h last.
const inOrder = (events: [string, Date | null][]) =>
  [...events].sort(([, a], [, b]) => (a?.valueOf() ?? Infinity) - (b?.valueOf() ?? Infinity));

const SunMoonCard: FunctionalComponent<{ latitude: number; longitude: number }> = ({ latitude, longitude }) => {
  const [now, setNow] = useState(() => new Date());
  useEffect(() => {
    const timer = setInterval(() => setNow(new Date()), 60000);
    return () => clearInterval(timer);
  }, []);

  const sun = sunPosition(now, latitude, longitude);
  const moon = moonPosition(now, latitude, longitude);
  const illumination = moonIllumination(now);
  const phase = skyPhase(sun.altitude);
  const sunAt = (date: Date) => sunPosition(date, latitude, longitude).altitude;
  const moonAt = (date: Date) => moonPosition(date, latitude, longitude).altitude;
  const sunset = nextCrossing(sunAt, SUN_HORIZON, now, false);
  const sunrise = nextCrossing(sunAt, SUN_HORIZON, now, true);
  const moonrise = nextCrossing(moonAt, MOON_HORIZON, now, true);
  const moonset = nextCrossing(moonAt, MOON_HORIZON, now, false);
  const dark = darkness(latitude, longitude, -18, now);

  const darkLabel = dark.darkNow
    ? `now, until ${formatClock(dark.end)}`
    : dark.start
      ? `${formatClock(dark.start)} - ${formatClock(dark.end)}${dark.end ? ` (${formatDuration(dark.end.valueOf() - dark.start.valueOf())})` : ''}`
      : 'none tonight';

  return (
    <Card title="Sun & Moon" icon="moon" tone="violet" actions={<Pill tone={PHASE_TONE[phase]}>{SKY_PHASE_LABEL[phase]}</Pill>}>
      <div class="sun-moon">
        <MoonDisc phase={illumination.phase} southern={latitude < 0} />
        <div class="metric-grid">
          <MetricTile label="Sun" value={sun.altitude.toFixed(1)} unit="°" tone={sun.altitude < -18 ? 'tone-green' : sun.altitude < 0 ? 'tone-cyan' : 'tone-amber'} />
          <MetricTile label="Moon" value={(illumination.fraction * 100).toFixed(0)} unit={`% · ${moonPhaseName(illumination.phase)}`} tone="tone-violet" />
          <MetricTile label="Moon altitude" value={moon.altitude.toFixed(0)} unit={moon.altitude > 0 ? '° up' : '° down'} />
        </div>
      </div>
      <ReadingRow label="Astronomical dark" value={darkLabel} />
      {inOrder([
        ['Sunset', sunset],
        ['Sunrise', sunrise],
        ['Moonrise', moonrise],
        ['Moonset', moonset],
      ]).map(([label, time]) => (
        <ReadingRow key={label} label={label} value={formatClock(time)} />
      ))}
    </Card>
  );
};

export default SunMoonCard;
