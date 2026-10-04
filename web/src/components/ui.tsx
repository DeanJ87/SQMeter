import { ComponentChildren, FunctionalComponent } from 'preact';

export const COLORS = {
  cyan: '#55c7f2',
  violet: '#a78bfa',
  green: '#5ee0a0',
  amber: '#f4c15d',
  red: '#f26b62',
  muted: '#566675',
};

type IconTone = keyof typeof COLORS;

export const Icon: FunctionalComponent<{ name: string; tone?: IconTone; size?: number }> = ({
  name,
  tone = 'cyan',
  size = 15,
}) => {
  const common = {
    stroke: COLORS[tone],
    strokeWidth: 1.7,
    fill: 'none',
    strokeLinecap: 'round' as const,
    strokeLinejoin: 'round' as const,
  };
  const paths: Record<string, ComponentChildren> = {
    star: <path {...common} d="m12 3 2.6 5.5 6 .8-4.4 4.2 1.1 6-5.3-2.9-5.3 2.9 1.1-6-4.4-4.2 6-.8L12 3Z" />,
    cloud: <path {...common} d="M18 18H8.2a4.2 4.2 0 1 1 1.1-8.2 6 6 0 0 1 11 3.3A2.8 2.8 0 0 1 18 18Z" />,
    therm: <><path {...common} d="M10 14.5V5a2 2 0 1 1 4 0v9.5a4 4 0 1 1-4 0Z" /><path {...common} d="M12 7v8" /></>,
    gps: <><circle {...common} cx="12" cy="12" r="3.2" /><path {...common} d="M12 3v3M12 18v3M3 12h3M18 12h3" /></>,
    eye: <><path {...common} d="M3 12s3.3-6 9-6 9 6 9 6-3.3 6-9 6-9-6-9-6Z" /><circle {...common} cx="12" cy="12" r="2.4" /></>,
    wifi: <><path {...common} d="M5 10a10 10 0 0 1 14 0M8 13a6 6 0 0 1 8 0M11 16a2 2 0 0 1 2 0" /><path {...common} d="M12 19h.01" /></>,
    rain: <><path {...common} d="M18 15H8.4a3.6 3.6 0 0 1 .9-7 5.2 5.2 0 0 1 9.5 2.9A2.4 2.4 0 0 1 18 15Z" /><path {...common} d="M8 19v1M12 18v2M16 19v1" /></>,
    cpu: <><rect {...common} x="5" y="5" width="14" height="14" rx="2" /><path {...common} d="M9 1v4M15 1v4M9 19v4M15 19v4M1 9h4M1 15h4M19 9h4M19 15h4" /></>,
    upload: <><path {...common} d="M12 16V4M7 9l5-5 5 5" /><path {...common} d="M5 18v2h14v-2" /></>,
    moon: <path {...common} d="M20 14.5A8 8 0 1 1 9.5 4a6.5 6.5 0 0 0 10.5 10.5Z" />,
  };

  return (
    <svg class="ui-icon" width={size} height={size} viewBox="0 0 24 24" aria-hidden="true">
      {paths[name] ?? null}
    </svg>
  );
};

export const Pill: FunctionalComponent<{ tone?: string }> = ({ tone = 'pill-dim', children }) => (
  <span class={`pill ${tone}`}>{children}</span>
);

export const Card: FunctionalComponent<{
  title: string;
  icon?: string;
  tone?: IconTone;
  actions?: ComponentChildren;
  hint?: ComponentChildren;
  id?: string;
}> = ({ title, icon, tone = 'cyan', actions, hint, id, children }) => (
  <section class="sq-card" id={id}>
    <div class="card-topline">
      <div class="card-title">
        {icon && <Icon name={icon} tone={tone} />}
        <h2>{title}</h2>
        {hint && <InfoTip text={hint} />}
      </div>
      {actions && <div class="card-actions">{actions}</div>}
    </div>
    {children}
  </section>
);

export const MetricTile: FunctionalComponent<{ label: string; value: string; unit?: string; tone?: string; align?: 'left' | 'center' }> = ({
  label,
  value,
  unit,
  tone = '',
  align = 'center',
}) => (
  <div class={`metric-tile ${align === 'left' ? 'left' : ''}`}>
    <div class="metric-label">{label}</div>
    <div class={`metric-value ${tone}`}>{value}</div>
    {unit && <div class="metric-unit">{unit}</div>}
  </div>
);

export const ReadingRow: FunctionalComponent<{ label: string; value: string; labelClass?: string; valueClass?: string }> = ({
  label,
  value,
  labelClass = '',
  valueClass = '',
}) => (
  <div class="reading-row">
    <span class={`reading-label ${labelClass}`}>{label}</span>
    <strong class={`reading-value ${valueClass}`}>{value}</strong>
  </div>
);

export const SensorReadingRow: FunctionalComponent<{ label: string; value: string; unit: string }> = ({ label, value, unit }) => (
  <div class="reading-row sensor-reading">
    <span class="reading-label">{label}</span>
    <strong class="reading-value">{value} <em>{unit}</em></strong>
  </div>
);

export const ProgressMeter: FunctionalComponent<{ value: number }> = ({ value }) => (
  <div class="progress-meter">
    <div style={{ width: `${Math.max(0, Math.min(100, value))}%` }} />
  </div>
);

type ButtonVariant = 'default' | 'primary' | 'danger' | 'ghost' | 'link';

// The only button style in the app. `default` is the tinted outline used for
// most actions, `primary` the one main action on a page (Save, Update),
// `danger` for destructive ones, `ghost` for quiet secondary actions and
// `link` for navigation.
export const Button: FunctionalComponent<{
  onClick?: () => void;
  variant?: ButtonVariant;
  small?: boolean;
  block?: boolean;
  disabled?: boolean;
  busy?: boolean;
  busyLabel?: string;
  title?: string;
  type?: 'button' | 'submit';
  ariaLabel?: string;
}> = ({ onClick, variant = 'default', small, block, disabled, busy, busyLabel, title, type = 'button', ariaLabel, children }) => (
  <button
    type={type}
    title={title}
    aria-label={ariaLabel}
    class={`btn btn-${variant}${small ? ' btn-sm' : ''}${block ? ' btn-block' : ''}`}
    disabled={disabled || busy}
    onClick={onClick}
  >
    {busy ? busyLabel ?? 'Working...' : children}
  </button>
);

// "?" that reveals an explanation on hover, focus or tap - for detail that
// would otherwise clutter the page as footnotes.
export const InfoTip: FunctionalComponent<{ text: ComponentChildren }> = ({ text }) => (
  <span class="info-tip" tabIndex={0} aria-label="More information">
    ?
    <span class="info-tip-body" role="tooltip">
      {text}
    </span>
  </span>
);

// One-line status note under a control: no box, just tinted text.
export const Note: FunctionalComponent<{
  tone?: 'muted' | 'warn' | 'bad' | 'ok';
  action?: { label: string; onClick: () => void };
}> = ({ tone = 'muted', action, children }) => (
  <p class={`note note-${tone}`}>
    {children}
    {action && (
      <button type="button" class="note-action" onClick={action.onClick}>
        {action.label}
      </button>
    )}
  </p>
);
