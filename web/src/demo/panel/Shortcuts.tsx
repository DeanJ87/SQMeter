import { FunctionalComponent } from 'preact';
import { route } from 'preact-router';
import { useState } from 'preact/hooks';
import { Button, Note } from '../../components/ui';
import { CLOUD_RAMP_MS, demoDevice } from '../device';
import { DARK_SKY_SQM, SHORTCUTS, type ShortcutId, type ShortcutResult } from '../shortcuts';
import { DraftNumber } from './NumberField';
import { formatNumber } from '../../i18n/format';

// Shortcuts worked out from the device's current settings (spec 019 US3).
// Everything they set shows in the readings afterwards (FR-011).

const RAMPS: { value: string; label: string }[] = [
  { value: 'auto', label: 'Cloud 40 s, others at once' },
  { value: '0', label: 'At once' },
  { value: '40000', label: 'Over 40 s' },
  { value: '120000', label: 'Over 2 min' },
  { value: '600000', label: 'Over 10 min' },
];

const Shortcuts: FunctionalComponent = () => {
  const [ramp, setRamp] = useState('auto');
  const [result, setResult] = useState<{ label: string; result: ShortcutResult } | null>(null);
  const [sqm, setSqm] = useState(DARK_SKY_SQM);

  const apply = (id: ShortcutId, label: string, cloud?: boolean) => {
    const rampMs = ramp === 'auto' ? (cloud ? CLOUD_RAMP_MS : 0) : Number(ramp);
    setResult({ label, result: demoDevice.applyShortcut(id, rampMs, { sqm }) });
  };

  return (
    <section class="demo-section" aria-label="Shortcuts">
      <h3>Shortcuts</h3>
      <div class="demo-buttons">
        {SHORTCUTS.map((s) => (
          <Button key={s.id} small onClick={() => apply(s.id, s.label, s.cloud)}>
            {s.label}
          </Button>
        ))}
      </div>
      <div class="demo-grid">
        <DraftNumber id="demo-dark-sky-sqm" label="Dark sky SQM" unit="mag/arcsec²" valueText={formatNumber(sqm, 2)} onCommit={setSqm} />
      </div>
      <div class="field">
        <label class="field-label" for="demo-ramp">
          Change
        </label>
        <select id="demo-ramp" class="input" value={ramp} onChange={(e) => setRamp((e.target as HTMLSelectElement).value)}>
          {RAMPS.map((r) => (
            <option key={r.value} value={r.value}>
              {r.label}
            </option>
          ))}
        </select>
      </div>
      {result && <Outcome label={result.label} result={result.result} />}
    </section>
  );
};

const Outcome: FunctionalComponent<{ label: string; result: ShortcutResult }> = ({ label, result }) => {
  if (result.ok)
    return (
      <Note tone="ok">
        {label}: {result.used}
        {result.clamped ? ` ${result.clamped}` : ''}
      </Note>
    );
  const link = result.link;
  return (
    <Note tone="warn" action={link && { label: link.label, onClick: () => route(link.route) }}>
      {label}: {result.reason}
    </Note>
  );
};

export default Shortcuts;
