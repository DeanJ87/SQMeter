import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import { INPUTS, type NumericInput } from '../conditions';

// Number fields for the Demo panel: the label carries the unit (FR-021); the
// value is committed on Enter or leaving the field, and the device's value
// shows again otherwise, since the panel re-renders every device tick.

const decimals = (step: number) => (step >= 1 ? 0 : Math.min(4, Math.ceil(-Math.log10(step))));

export const formatInput = (field: NumericInput, value: number) =>
  field === 'light.lux' ? value.toPrecision(3) : value.toFixed(decimals(INPUTS[field].step));

export const DraftNumber: FunctionalComponent<{
  id: string;
  label: string;
  unit: string;
  valueText: string;
  disabled?: boolean;
  hint?: string;
  onCommit: (value: number) => void;
}> = ({ id, label, unit, valueText, disabled, hint, onCommit }) => {
  const [draft, setDraft] = useState<string | null>(null);

  const commit = () => {
    if (draft === null) return;
    const parsed = Number.parseFloat(draft);
    setDraft(null);
    if (Number.isFinite(parsed)) onCommit(parsed);
  };

  return (
    <div class="field">
      <label class="field-label" for={id}>
        {label} ({unit})
      </label>
      <div class={`input-group${disabled ? ' is-disabled' : ''}`}>
        <input
          id={id}
          class="input"
          type="number"
          inputMode="decimal"
          step="any"
          disabled={disabled}
          value={draft ?? valueText}
          onInput={(e) => setDraft((e.target as HTMLInputElement).value)}
          onChange={commit}
          onBlur={commit}
          onKeyDown={(e) => e.key === 'Enter' && commit()}
        />
      </div>
      {hint && <span class="demo-hint">{hint}</span>}
    </div>
  );
};

/** One sensor reading; values outside the sensor's range are clamped and say so (FR-004). */
export const NumberField: FunctionalComponent<{
  field: NumericInput;
  value: number;
  disabled?: boolean;
  onCommit: (value: number) => boolean; // true when the value was clamped
}> = ({ field, value, disabled, onCommit }) => {
  const spec = INPUTS[field];
  const [clamped, setClamped] = useState(false);
  return (
    <DraftNumber
      id={`demo-input-${field.replace('.', '-')}`}
      label={spec.label}
      unit={spec.unit}
      valueText={formatInput(field, value)}
      disabled={disabled}
      hint={clamped ? `Limited to the sensor's range, ${spec.min} to ${spec.max} ${spec.unit}.` : undefined}
      onCommit={(v) => setClamped(onCommit(v))}
    />
  );
};

export const Check: FunctionalComponent<{ label: string; checked: boolean; disabled?: boolean; onChange: (on: boolean) => void }> = ({
  label,
  checked,
  disabled,
  onChange,
}) => (
  <label class="demo-check">
    <input type="checkbox" checked={checked} disabled={disabled} onChange={(e) => onChange((e.target as HTMLInputElement).checked)} />
    {label}
  </label>
);
