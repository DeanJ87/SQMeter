import { FunctionalComponent } from 'preact';
import { INPUTS, type Ramp } from '../conditions';
import { demoDevice, type Pending } from '../device';
import { formatInput } from './NumberField';

// What the device makes of the readings (spec 019 US2): its own derived
// values, and what it's still waiting on, read from the device core. Nothing
// here is computed by the panel (FR-005).

export const formatRemaining = (seconds: number) => {
  const s = Math.max(0, Math.ceil(seconds));
  if (s < 60) return `${s} s`;
  const minutes = Math.floor(s / 60);
  return minutes < 60 ? `${minutes} min ${s % 60} s` : `${Math.floor(minutes / 60)} h ${minutes % 60} min`;
};

interface Readings {
  sky?: { status: string; sqm?: number; nelm?: number; bortle?: number };
  clouds?: { status: string; coverPercent?: number; description?: string };
  environment?: { status: string; dewpoint?: number };
  rain?: { status: string; raining?: boolean; rainingNow?: boolean; intensity?: number };
}

interface Safety {
  safe: boolean;
  reasons: string[];
  secondsUntilSafe?: number;
}

const Row: FunctionalComponent<{ label: string; value: string }> = ({ label, value }) => (
  <div class="demo-readout-row">
    <span>{label}</span>
    <span class="mono">{value}</span>
  </div>
);

const ok = (group?: { status: string }) => group?.status === 'ok';

const ALERT_NAMES: Record<string, string> = {
  safety: 'Safety alert',
  rain: 'Rain alert',
  lens: 'Lens alert',
  dew: 'Dew risk alert',
  sky: 'Sky alert',
};
const alertName = (condition: string) =>
  condition.startsWith('sensor:') ? `${condition.slice('sensor:'.length)} fault alert` : (ALERT_NAMES[condition] ?? 'Alert');

/** The device's waits, worded for people (FR-007). */
export function describeWaits(pending: Pending, safety: Safety): string[] {
  const waits: string[] = [];
  const sky = pending.skyAveraging;
  if (sky.nightMode && sky.settlingSeconds > 0)
    waits.push(`Sky brightness averages over ${sky.windowSeconds} s - settled in ${formatRemaining(sky.settlingSeconds)}`);
  if (pending.rainClear.latched && !pending.rainClear.rainingNow && (pending.rainClear.remainingSeconds ?? 0) > 0)
    waits.push(`Rain clear delay - ${formatRemaining(pending.rainClear.remainingSeconds ?? 0)} until rain is cleared`);
  if (!safety.safe && (safety.secondsUntilSafe ?? 0) > 0)
    waits.push(`Safe delay - safe in ${formatRemaining(safety.secondsUntilSafe ?? 0)}`);
  for (const alert of pending.alerts) {
    if (alert.kind === 'grace') waits.push(`No alerts for ${formatRemaining(alert.remainingSeconds)} after start-up`);
    else if (alert.kind === 'settle')
      waits.push(`${alertName(alert.condition)} - the change must hold ${formatRemaining(alert.remainingSeconds)} more`);
    else waits.push(`${alertName(alert.condition)} - cooldown, ${formatRemaining(alert.remainingSeconds)} until it can be sent`);
  }
  return waits;
}

const NO_READING = '--';

// The device's derived values, one line each (FR-006).
function readoutRows(readings: Readings): { label: string; value: string }[] {
  const { sky, clouds, environment, rain } = readings;
  const rows = [
    {
      label: 'Sky quality',
      value: ok(sky) ? `${sky?.sqm?.toFixed(2)} · NELM ${sky?.nelm?.toFixed(1)} · Bortle ${sky?.bortle}` : NO_READING,
    },
    { label: 'Clouds', value: ok(clouds) ? `${clouds?.coverPercent?.toFixed(0)}% · ${clouds?.description}` : NO_READING },
    { label: 'Dew point', value: ok(environment) ? `${environment?.dewpoint?.toFixed(1)} °C` : NO_READING },
  ];
  if (rain) rows.push({ label: 'Rain', value: rainText(rain) });
  return rows;
}

function rainText(rain: NonNullable<Readings['rain']>) {
  if (!ok(rain)) return NO_READING;
  if (rain.rainingNow) return `raining ${rain.intensity?.toFixed(1)} mm/h`;
  return rain.raining ? 'held after rain' : 'dry';
}

const rampText = (ramp: Ramp) => {
  const spec = INPUTS[ramp.field];
  return `${spec.label} moving to ${formatInput(ramp.field, ramp.to)} ${spec.unit} - ${formatRemaining(demoDevice.rampRemainingMs(ramp) / 1000)} left`;
};

const DeviceReadout: FunctionalComponent = () => {
  const readings = JSON.parse(demoDevice.readings()) as Readings;
  const safety = JSON.parse(demoDevice.safety()) as Safety;
  const pending = [...demoDevice.ramps.map(rampText), ...describeWaits(demoDevice.pending(), safety)];

  return (
    <section class="demo-section" aria-label="What the device reads">
      <h3>The device reads</h3>
      <div class="demo-readout">
        {readoutRows(readings).map((row) => (
          <Row key={row.label} label={row.label} value={row.value} />
        ))}
        <Row label="Verdict" value={safety.safe ? 'Safe' : 'Unsafe'} />
        <Row label="Alerts" value={demoDevice.isArmed() ? 'On' : 'Off'} />
      </div>
      {!safety.safe && safety.reasons.length > 0 && <p class="demo-hint">{safety.reasons.join('; ')}</p>}
      {pending.length > 0 && (
        <ul class="demo-waits" aria-label="Waiting on">
          {pending.map((text) => (
            <li key={text.replace(/\d+/g, '')}>{text}</li>
          ))}
        </ul>
      )}
    </section>
  );
};

export default DeviceReadout;
