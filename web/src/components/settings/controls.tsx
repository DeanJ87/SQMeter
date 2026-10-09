import { ComponentChildren, createContext, FunctionalComponent } from 'preact';
import { useContext, useId } from 'preact/hooks';
import { FIX_LABEL, blocksSwitchingOn, noteFor, type DepEntry } from '../../lib/settingsDeps';
import { Button, Card, InfoTip, Note, Pill } from '../ui';

// Settings building blocks, all on the shared component classes so Settings
// looks like the rest of the app. Explanations go in `hint` (a "?" tooltip),
// not in paragraphs.

type Tone = 'ok' | 'warn' | 'bad' | 'off';
const PILL: Record<Tone, string> = { ok: 'pill-green', warn: 'pill-amber', bad: 'pill-red', off: 'pill-dim' };

export const StatusBadge: FunctionalComponent<{ tone: Tone; label: string }> = ({ tone, label }) => <Pill tone={PILL[tone]}>{label}</Pill>;

export const SettingsCard: FunctionalComponent<{
  id?: string;
  title: string;
  hint?: ComponentChildren;
  badge?: ComponentChildren;
}> = ({ id, title, hint, badge, children }) => (
  <Card id={id} title={title} hint={hint} actions={badge}>
    <div class="card-body">{children}</div>
  </Card>
);

export const Group: FunctionalComponent<{ title?: string; aside?: ComponentChildren }> = ({ title, aside, children }) => (
  <div class="card-group">
    {(title || aside) && (
      <h3 class="card-group-title">
        {title}
        {aside}
      </h3>
    )}
    {children}
  </div>
);

// Why something is unavailable, optionally with a jump to where to fix it.
export const Requires: FunctionalComponent<{ tone?: 'info' | 'warn'; onFix?: () => void; fixLabel?: string }> = ({
  tone = 'info',
  onFix,
  fixLabel,
  children,
}) => (
  <Note tone={tone === 'warn' ? 'warn' : 'muted'} action={onFix ? { label: fixLabel ?? 'Set up', onClick: onFix } : undefined}>
    {children}
  </Note>
);

export const Toggle: FunctionalComponent<{
  label: string;
  checked: boolean;
  onChange: (checked: boolean) => void;
  hint?: ComponentChildren;
  // When set and the toggle is off, it can't be switched on; the reason is
  // shown. A toggle that's already on can always be switched off.
  blockedReason?: ComponentChildren;
  onFix?: () => void;
  fixLabel?: string;
  disabled?: boolean;
  dataField?: string;
}> = ({ label, checked, onChange, hint, blockedReason, onFix, fixLabel, disabled, dataField }) => {
  const locked = (Boolean(blockedReason) && !checked) || disabled;
  return (
    <div>
      <label class={`toggle${locked ? ' is-disabled' : ''}`}>
        <input
          type="checkbox"
          role="switch"
          data-field={dataField}
          aria-label={label}
          checked={checked}
          disabled={locked}
          onChange={(e) => onChange((e.target as HTMLInputElement).checked)}
        />
        <span>
          {label}
          {hint && <InfoTip text={hint} />}
        </span>
      </label>
      {blockedReason && (
        <div class="indent">
          <Requires tone={checked ? 'warn' : 'info'} onFix={onFix} fixLabel={fixLabel}>
            {blockedReason}
          </Requires>
        </div>
      )}
    </div>
  );
};

// Why a dependent setting isn't in effect, with its one-click fix
// (specs/020-settings-dependencies). Inactive: a warning ("Inactive - MQTT
// is off"), or a muted note for harmless defaults. Off but blocked: what has
// to change before it can be switched on.
export const DepNote: FunctionalComponent<{ entry: DepEntry; onFix: (entry: DepEntry) => void; prefix?: string }> = ({
  entry,
  onFix,
  prefix,
}) => {
  const note = noteFor(entry, prefix);
  if (!note) return null;
  return (
    <Requires tone={note.tone} onFix={() => onFix(note.target)} fixLabel={FIX_LABEL[note.reason]}>
      <span data-dep={note.id}>{note.text}</span>
    </Requires>
  );
};

// A toggle for a dependent setting: can't be switched on while what it needs
// is off (FR-005) - runtime conditions like WiFi never block - and can
// always be switched off; shows why it isn't in effect.
export const DepToggle: FunctionalComponent<
  Omit<Parameters<typeof Toggle>[0], 'blockedReason' | 'onFix' | 'fixLabel'> & {
    entry: DepEntry;
    onFix: (entry: DepEntry) => void;
    prefix?: string;
  }
> = ({ entry, onFix, prefix, ...toggle }) => {
  const blocked = entry.state === 'off' && entry.blockedBy && blocksSwitchingOn(entry.blockedBy.reason);
  return (
    <div>
      <Toggle {...toggle} disabled={toggle.disabled || (blocked && !toggle.checked)} />
      {(entry.state === 'inactive' || blocked) && (
        <div class="indent">
          <DepNote entry={entry} onFix={onFix} prefix={prefix} />
        </div>
      )}
    </div>
  );
};

// A Field names the inputs inside it and ties its error to them (WCAG 1.3.1,
// 3.3.1): the inputs below read this context, so call sites don't wire ids.
interface FieldInfo {
  labelId: string;
  errorId?: string;
}
const FieldContext = createContext<FieldInfo | null>(null);

export const Field: FunctionalComponent<{ label: string; hint?: ComponentChildren; error?: string; class?: string }> = ({
  label,
  hint,
  error,
  children,
  class: className,
}) => {
  const id = useId();
  const info: FieldInfo = { labelId: `${id}-label`, errorId: error ? `${id}-error` : undefined };
  return (
    <div class={`field${className ? ` ${className}` : ''}`}>
      <label class="field-label">
        <span id={info.labelId}>{label}</span>
        {hint && <InfoTip text={hint} />}
      </label>
      <FieldContext.Provider value={info}>{children}</FieldContext.Provider>
      {error && (
        <Note tone="bad" id={info.errorId}>
          {error}
        </Note>
      )}
    </div>
  );
};

// ARIA for an input: its own label if given, else its Field's, plus the
// error description and invalid state.
const useFieldAria = (ariaLabel?: string, error?: string) => {
  const field = useContext(FieldContext);
  const errorId = error ? field?.errorId : undefined;
  return {
    'aria-label': ariaLabel,
    'aria-labelledby': ariaLabel ? undefined : field?.labelId,
    'aria-describedby': errorId,
    'aria-invalid': error ? true : undefined,
  };
};

const inputClass = (error?: string) => `input${error ? ' is-invalid' : ''}`;

export const NumberInput: FunctionalComponent<{
  value: number;
  onChange: (value: number) => void;
  min?: number;
  max?: number;
  step?: number | string;
  disabled?: boolean;
  error?: string;
  dataField?: string;
  integer?: boolean;
  ariaLabel?: string;
  unit?: string;
}> = ({ value, onChange, min, max, step, disabled, error, dataField, integer, ariaLabel, unit }) => {
  const aria = useFieldAria(ariaLabel, error);
  const input = (
    <input
      type="number"
      data-field={dataField}
      {...aria}
      class={inputClass(error)}
      value={Number.isFinite(value) ? value : ''}
      min={min}
      max={max}
      step={step}
      disabled={disabled}
      onChange={(e) => {
        const raw = (e.target as HTMLInputElement).value;
        onChange(integer ? parseInt(raw, 10) : parseFloat(raw));
      }}
    />
  );
  return unit ? (
    <div class={`input-group${error ? ' is-invalid' : ''}${disabled ? ' is-disabled' : ''}`}>
      {input}
      <span class="input-unit">{unit}</span>
    </div>
  ) : (
    input
  );
};

export const TextInput: FunctionalComponent<{
  value: string;
  onInput: (value: string) => void;
  type?: 'text' | 'password' | 'url';
  placeholder?: string;
  disabled?: boolean;
  error?: string;
  dataField?: string;
  ariaLabel?: string;
}> = ({ value, onInput, type = 'text', placeholder, disabled, error, dataField, ariaLabel }) => (
  <input
    {...useFieldAria(ariaLabel, error)}
    type={type}
    data-field={dataField}
    name={dataField}
    class={inputClass(error)}
    value={value}
    placeholder={placeholder}
    disabled={disabled}
    autocomplete={type === 'password' ? 'off' : undefined}
    onInput={(e) => onInput((e.target as HTMLInputElement).value)}
  />
);

export const SelectInput: FunctionalComponent<{
  value: string;
  onChange: (value: string) => void;
  options: { value: string; label: string; disabled?: boolean }[];
  disabled?: boolean;
  error?: string;
  dataField?: string;
  id?: string;
  ariaLabel?: string;
}> = ({ value, onChange, options, disabled, error, dataField, id, ariaLabel }) => (
  <select
    {...useFieldAria(ariaLabel, error)}
    id={id}
    data-field={dataField}
    class={inputClass(error)}
    value={value}
    disabled={disabled}
    onChange={(e) => onChange((e.target as HTMLSelectElement).value)}
  >
    {options.map((option) => (
      <option key={option.value} value={option.value} disabled={option.disabled}>
        {option.label}
      </option>
    ))}
  </select>
);

export const ActionButton: FunctionalComponent<{
  onClick: () => void;
  disabled?: boolean;
  busy?: boolean;
  busyLabel?: string;
  title?: string;
  variant?: 'default' | 'danger';
}> = ({ onClick, disabled, busy, busyLabel, title, variant, children }) => (
  <Button small onClick={onClick} disabled={disabled} busy={busy} busyLabel={busyLabel} title={title} variant={variant}>
    {children}
  </Button>
);

export const ResultNote: FunctionalComponent<{ result: { type: 'success' | 'error'; text: string } | null }> = ({ result }) =>
  result ? <Note tone={result.type === 'success' ? 'ok' : 'bad'}>{result.text}</Note> : null;
