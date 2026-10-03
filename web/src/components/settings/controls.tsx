import { ComponentChildren, FunctionalComponent } from 'preact';
import type { SettingsTabId } from './tabs';

// Shared building blocks for the Settings tabs, so every section looks and
// behaves the same: one card style, one toggle style, one way to explain why
// something can't be used.

export const inputClass = (error?: string) =>
  `w-full px-3 py-2 bg-gray-900/60 border rounded-lg text-white text-sm focus:outline-none disabled:opacity-40 disabled:cursor-not-allowed ${
    error ? 'border-red-500' : 'border-gray-600 focus:border-blue-500'
  }`;

type Tone = 'ok' | 'warn' | 'bad' | 'off';

export const StatusBadge: FunctionalComponent<{ tone: Tone; label: string }> = ({ tone, label }) => (
  <span class={`sq-badge sq-badge-${tone}`}>
    <i class="sq-badge-dot" aria-hidden="true" />
    {label}
  </span>
);

export const SettingsCard: FunctionalComponent<{
  id?: string;
  title: string;
  description?: ComponentChildren;
  badge?: ComponentChildren;
}> = ({ id, title, description, badge, children }) => (
  <section id={id} class="bg-gray-800 rounded-xl p-5 md:p-6 border border-gray-700 scroll-mt-24">
    <div class="flex flex-wrap items-start justify-between gap-2 mb-4">
      <div>
        <h2 class="text-lg font-semibold text-white">{title}</h2>
        {description && <p class="mt-1 text-sm text-gray-400">{description}</p>}
      </div>
      {badge}
    </div>
    <div class="space-y-4">{children}</div>
  </section>
);

// A titled group inside a card, separated by a rule.
export const Group: FunctionalComponent<{ title: string; aside?: ComponentChildren }> = ({ title, aside, children }) => (
  <div class="pt-4 border-t border-gray-700 first:border-t-0 first:pt-0 space-y-3">
    <div class="flex flex-wrap items-center justify-between gap-2">
      <h3 class="text-sm font-semibold uppercase tracking-wide text-gray-400">{title}</h3>
      {aside}
    </div>
    {children}
  </div>
);

// Explains why something is unavailable, optionally with a jump to the tab
// that fixes it.
export const Requires: FunctionalComponent<{
  tone?: 'info' | 'warn';
  onFix?: () => void;
  fixLabel?: string;
}> = ({ tone = 'info', onFix, fixLabel, children }) => (
  <div
    class={`flex flex-wrap items-center gap-x-3 gap-y-1 px-3 py-2 rounded-lg text-xs border ${
      tone === 'warn' ? 'bg-amber-900/30 border-amber-800 text-amber-200' : 'bg-gray-900/50 border-gray-700 text-gray-400'
    }`}
  >
    <span>{children}</span>
    {onFix && (
      <button type="button" class="text-cyan-300 hover:underline" onClick={onFix}>
        {fixLabel ?? 'Set up'} →
      </button>
    )}
  </div>
);

export const Toggle: FunctionalComponent<{
  label: string;
  checked: boolean;
  onChange: (checked: boolean) => void;
  hint?: ComponentChildren;
  // When set and the toggle is off, it can't be switched on; the reason is shown.
  // A toggle that is already on can always be switched off.
  blockedReason?: ComponentChildren;
  disabled?: boolean;
  dataField?: string;
}> = ({ label, checked, onChange, hint, blockedReason, disabled, dataField }) => {
  const blocked = Boolean(blockedReason) && !checked;
  return (
    <div class={blocked || disabled ? 'opacity-60' : ''}>
      <label class={`flex items-start gap-3 ${blocked || disabled ? 'cursor-not-allowed' : 'cursor-pointer'}`}>
        <input
          type="checkbox"
          data-field={dataField}
          class="mt-0.5 w-4 h-4 accent-blue-500"
          checked={checked}
          disabled={blocked || disabled}
          onChange={(e) => onChange((e.target as HTMLInputElement).checked)}
        />
        <span class="text-sm text-white">{label}</span>
      </label>
      {hint && <p class="mt-1 ml-7 text-xs text-gray-500">{hint}</p>}
      {blockedReason && <p class={`mt-1 ml-7 text-xs ${checked ? 'text-amber-300' : 'text-gray-400'}`}>{blockedReason}</p>}
    </div>
  );
};

export const Field: FunctionalComponent<{ label: string; hint?: ComponentChildren; error?: string; class?: string }> = ({
  label,
  hint,
  error,
  children,
  class: className,
}) => (
  <div class={className}>
    <label class="block text-sm font-medium text-gray-300 mb-1.5">{label}</label>
    {children}
    {error && <p class="mt-1 text-xs text-red-400">{error}</p>}
    {hint && !error && <p class="mt-1 text-xs text-gray-500">{hint}</p>}
  </div>
);

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
}> = ({ value, onChange, min, max, step, disabled, error, dataField, integer, ariaLabel }) => (
  <input
    type="number"
    data-field={dataField}
    aria-label={ariaLabel}
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

export const TextInput: FunctionalComponent<{
  value: string;
  onInput: (value: string) => void;
  type?: 'text' | 'password' | 'url';
  placeholder?: string;
  disabled?: boolean;
  error?: string;
  dataField?: string;
  autoComplete?: string;
}> = ({ value, onInput, type = 'text', placeholder, disabled, error, dataField, autoComplete }) => (
  <input
    type={type}
    data-field={dataField}
    name={dataField}
    class={inputClass(error)}
    value={value}
    placeholder={placeholder}
    disabled={disabled}
    autocomplete={autoComplete ?? (type === 'password' ? 'off' : undefined)}
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
}> = ({ value, onChange, options, disabled, error, dataField }) => (
  <select
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
}> = ({ onClick, disabled, busy, busyLabel, title, children }) => (
  <button
    type="button"
    title={title}
    onClick={onClick}
    disabled={disabled || busy}
    class="px-3 py-1.5 text-sm bg-gray-700 hover:bg-gray-600 disabled:opacity-50 disabled:cursor-not-allowed text-white rounded-lg"
  >
    {busy ? busyLabel ?? 'Working...' : children}
  </button>
);

export const ResultNote: FunctionalComponent<{ result: { type: 'success' | 'error'; text: string } | null }> = ({ result }) =>
  result ? (
    <p class={`text-xs ${result.type === 'success' ? 'text-green-300' : 'text-red-300'}`}>{result.text}</p>
  ) : null;

export interface TabLink {
  goTo: (tab: SettingsTabId, anchor?: string) => void;
}
