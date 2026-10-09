import { FunctionalComponent } from 'preact';
import { summariseSeries } from '../../lib/a11y';
import { formatNumber } from '../../i18n/format';
import { svgNumber } from '../../lib/svg';

export const StatusDot: FunctionalComponent<{ ok: boolean }> = ({ ok }) => (
  <span class={`status-dot ${ok ? 'is-ok' : 'is-bad'}`} aria-hidden="true" />
);

type Point = { x: number; y: number };

// One smoothed segment of the trend line, as SVG path commands.
const curveTo = (path: string, point: Point, index: number, points: Point[]) => {
  if (index === 0) return `M ${svgNumber(point.x)} ${svgNumber(point.y)}`;

  const previous = points[index - 1];
  const beforePrevious = points[index - 2] ?? previous;
  const next = points[index + 1] ?? point;
  const smoothing = 0.18;
  const controlStart = {
    x: previous.x + (point.x - beforePrevious.x) * smoothing,
    y: previous.y + (point.y - beforePrevious.y) * smoothing,
  };
  const controlEnd = {
    x: point.x - (next.x - previous.x) * smoothing,
    y: point.y - (next.y - previous.y) * smoothing,
  };

  return [
    path,
    'C',
    svgNumber(controlStart.x),
    svgNumber(controlStart.y),
    svgNumber(controlEnd.x),
    svgNumber(controlEnd.y),
    svgNumber(point.x),
    svgNumber(point.y),
  ].join(' ');
};

// `label` names the series; screen readers get its trend in words (spec 022).
export const MiniSpark: FunctionalComponent<{ values: number[]; tone: string; label: string }> = ({ values, tone, label }) => {
  const summary = summariseSeries(label, values, (value) => formatNumber(value, 2));
  if (values.length < 2) return <div class="sparkline" role="img" aria-label={summary} />;
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1;
  const points = values.map((value, index) => ({
    x: (index / (values.length - 1)) * 100,
    y: 34 - ((value - min) / range) * 30,
  }));
  const linePath = points.reduce(curveTo, '');
  const fillPath = `${linePath} L 100 36 L 0 36 Z`;

  return (
    <svg class={`sparkline ${tone}`} viewBox="0 0 100 36" preserveAspectRatio="none" role="img" aria-label={summary}>
      <path d={fillPath} class="spark-fill" />
      <path d={linePath} class="spark-line" />
    </svg>
  );
};
