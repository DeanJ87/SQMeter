import { FunctionalComponent } from 'preact';
import { useLayoutEffect, useMemo, useRef, useState } from 'preact/hooks';
import { formatClock, moonIllumination, moonPosition, SkyPhase, skyPhase, skyPhaseLabel, sunPosition } from '../lib/astro';
import { t } from '../i18n';

// Noon-to-noon altitude chart for the coming (or current) night: twilight as
// background bands, the moon's altitude as a curve, a "now" line, and a
// crosshair with the exact values under the pointer.

const STEP_MS = 10 * 60000;
const HEIGHT = 170;
const PAD = { left: 26, right: 6, top: 8, bottom: 18 };

const BAND_CLASS: Record<SkyPhase, string> = {
  day: 'band-day',
  civil: 'band-civil',
  nautical: 'band-nautical',
  astronomical: 'band-astro',
  night: 'band-night',
};

// Local noon before `now` (or today's, after midday) to 24 h later.
export const nightWindow = (now: Date) => {
  const start = new Date(now);
  start.setHours(12, 0, 0, 0);
  if (now.getHours() < 12) start.setDate(start.getDate() - 1);
  const end = new Date(start);
  end.setDate(end.getDate() + 1);
  return { start, end };
};

const NightChart: FunctionalComponent<{ latitude: number; longitude: number; now: Date }> = ({ latitude, longitude, now }) => {
  const boxRef = useRef<HTMLDivElement>(null);
  const [width, setWidth] = useState(320);
  const [hoverTime, setHoverTime] = useState<number | null>(null);

  useLayoutEffect(() => {
    const box = boxRef.current;
    if (!box) return;
    const observer = new ResizeObserver(() => setWidth(box.clientWidth || 320));
    observer.observe(box);
    setWidth(box.clientWidth || 320);
    return () => observer.disconnect();
  }, []);

  const minute = Math.floor(now.valueOf() / 60000);
  const { start, end, samples } = useMemo(() => {
    const window = nightWindow(now);
    const points = [];
    for (let t = window.start.valueOf(); t <= window.end.valueOf(); t += STEP_MS) {
      const date = new Date(t);
      points.push({ t, sun: sunPosition(date, latitude, longitude).altitude, moon: moonPosition(date, latitude, longitude).altitude });
    }
    return { ...window, samples: points };
  }, [latitude, longitude, minute]);

  const plotWidth = Math.max(10, width - PAD.left - PAD.right);
  const plotHeight = HEIGHT - PAD.top - PAD.bottom;
  const span = end.valueOf() - start.valueOf();
  const x = (t: number) => PAD.left + ((t - start.valueOf()) / span) * plotWidth;
  const y = (altitude: number) => PAD.top + (1 - Math.max(0, Math.min(90, altitude)) / 90) * plotHeight;

  // Merge consecutive samples in the same twilight phase into one band.
  const bands: { from: number; to: number; phase: SkyPhase }[] = [];
  for (let i = 0; i < samples.length - 1; i++) {
    const phase = skyPhase((samples[i].sun + samples[i + 1].sun) / 2);
    const last = bands[bands.length - 1];
    if (last && last.phase === phase) last.to = samples[i + 1].t;
    else bands.push({ from: samples[i].t, to: samples[i + 1].t, phase });
  }

  // The moon's curve, only where it's above the horizon.
  const moonSegments: string[] = [];
  let segment: string[] = [];
  for (const sample of samples) {
    if (sample.moon > 0) segment.push(`${x(sample.t).toFixed(1)},${y(sample.moon).toFixed(1)}`);
    else if (segment.length) {
      moonSegments.push(segment.join(' '));
      segment = [];
    }
  }
  if (segment.length) moonSegments.push(segment.join(' '));
  const baseline = y(0);
  const lit = moonIllumination(now).fraction;

  const ticks: number[] = [];
  const tick = new Date(start);
  while (tick.valueOf() <= end.valueOf()) {
    if (tick.getHours() % 3 === 0) ticks.push(tick.valueOf());
    tick.setHours(tick.getHours() + 1);
  }

  const pick = (event: PointerEvent) => {
    const box = (event.currentTarget as SVGElement).getBoundingClientRect();
    const fraction = (event.clientX - box.left - PAD.left) / plotWidth;
    setHoverTime(fraction < 0 || fraction > 1 ? null : start.valueOf() + fraction * span);
  };

  const hover =
    hoverTime !== null
      ? (() => {
          const date = new Date(hoverTime);
          const sun = sunPosition(date, latitude, longitude).altitude;
          const moon = moonPosition(date, latitude, longitude).altitude;
          return { date, sun, moon, lit: moonIllumination(date).fraction };
        })()
      : null;
  const nowInside = now.valueOf() >= start.valueOf() && now.valueOf() <= end.valueOf();

  // What the chart shows, in words (spec 022 FR-011).
  const darkBands = bands.filter((band) => band.phase === 'night');
  const moonUp: { from: number; to: number }[] = [];
  samples.forEach((sample, i) => {
    if (sample.moon <= 0) return;
    const last = moonUp[moonUp.length - 1];
    if (last && i > 0 && samples[i - 1].moon > 0) last.to = sample.t;
    else moonUp.push({ from: sample.t, to: sample.t });
  });
  const clock = (t: number) => formatClock(new Date(t));
  const description = [
    t('nightChart.nightChartClockToClock2', { clock: clock(start.valueOf()), clock2: clock(end.valueOf()) }),
    darkBands.length
      ? t('nightChart.darkFromClockToClock2', { clock: clock(darkBands[0].from), clock2: clock(darkBands[darkBands.length - 1].to) })
      : t('nightChart.noFullDarkness'),
    moonUp.length
      ? t('nightChart.moonUpJoin', {
          join: moonUp.map((up) => t('nightChart.range', { from: clock(up.from), to: clock(up.to) })).join(t('nightChart.and')),
        })
      : t('nightChart.moonBelowTheHorizon'),
    t('nightChart.moonRoundLit', { round: Math.round(lit * 100) }),
  ].join(' ');

  return (
    <div class="night-chart" ref={boxRef}>
      <svg
        width={width}
        height={HEIGHT}
        role="img"
        aria-label={description}
        onPointerMove={pick}
        onPointerDown={pick}
        onPointerLeave={() => setHoverTime(null)}
      >
        {bands.map((band) => (
          <rect
            key={band.from}
            class={BAND_CLASS[band.phase]}
            x={x(band.from)}
            y={PAD.top}
            width={Math.max(0, x(band.to) - x(band.from))}
            height={plotHeight}
          />
        ))}
        {[30, 60].map((altitude) => (
          <line key={altitude} class="chart-grid" x1={PAD.left} x2={PAD.left + plotWidth} y1={y(altitude)} y2={y(altitude)} />
        ))}
        {[0, 30, 60, 90].map((altitude) => (
          <text key={altitude} class="chart-label" x={PAD.left - 4} y={y(altitude) + 3} text-anchor="end">
            {altitude}°
          </text>
        ))}
        {ticks.map((t) => (
          <text key={t} class="chart-label" x={x(t)} y={HEIGHT - 4} text-anchor="middle">
            {String(new Date(t).getHours()).padStart(2, '0')}
          </text>
        ))}
        {moonSegments.map((points) => {
          const coords = points.split(' ');
          const first = coords[0].split(',')[0];
          const last = coords[coords.length - 1].split(',')[0];
          return (
            <g key={points.slice(0, 16)}>
              <path class="moon-area" d={`M${first},${baseline} L${points.replace(/ /g, ' L')} L${last},${baseline} Z`} />
              <polyline class="moon-line" points={points} style={{ opacity: 0.45 + 0.55 * lit }} />
            </g>
          );
        })}
        {nowInside && (
          <g>
            <line class="now-line" x1={x(now.valueOf())} x2={x(now.valueOf())} y1={PAD.top} y2={PAD.top + plotHeight} />
            <text class="chart-label now-label" x={x(now.valueOf()) + 3} y={PAD.top + 9}>
              {t('nightChart.now')}
            </text>
          </g>
        )}
        {hover && (
          <g>
            <line class="hover-line" x1={x(hover.date.valueOf())} x2={x(hover.date.valueOf())} y1={PAD.top} y2={PAD.top + plotHeight} />
            {hover.moon > 0 && <circle class="hover-dot" cx={x(hover.date.valueOf())} cy={y(hover.moon)} r={3.5} />}
          </g>
        )}
      </svg>
      {hover && (
        <div class="chart-tip" style={{ left: `${Math.min(Math.max(x(hover.date.valueOf()), 70), width - 70)}px` }}>
          <strong>{formatClock(hover.date)}</strong>
          <span>{t('nightChart.sunFixedValue', { fixed: hover.sun.toFixed(1), value: skyPhaseLabel(skyPhase(hover.sun)) })}</span>
          <span>
            {hover.moon > 0
              ? t('nightChart.hoverMoonUp', { altitude: hover.moon.toFixed(0), lit: (hover.lit * 100).toFixed(0) })
              : t('nightChart.hoverMoonDown', { lit: (hover.lit * 100).toFixed(0) })}
          </span>
        </div>
      )}
      <div class="chart-legend">
        <span>
          <i class="swatch band-day" /> {t('nightChart.day')}
        </span>
        <span>
          <i class="swatch band-civil" /> {t('nightChart.twilight')}
        </span>
        <span>
          <i class="swatch band-night" /> {t('nightChart.dark')}
        </span>
        <span>
          <i class="swatch swatch-moon" /> {t('nightChart.moonAltitude')}
        </span>
      </div>
    </div>
  );
};

export default NightChart;
