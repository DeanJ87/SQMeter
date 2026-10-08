import { ComponentChildren, FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { route } from 'preact-router';
import type { AlpacaConfig, SafetyStatus } from '../../types';
import SafetyCard from '../SafetyCard';
import type { SettingsTabProps } from './context';
import { defaultAlpacaConfig, defaultRainConfig } from './defaults';
import { unavailableReason } from './hardware';
import { Field, Group, NumberInput, Requires, SettingsCard, StatusBadge, Toggle } from './controls';

type NumericKey = {
  [K in keyof AlpacaConfig]: AlpacaConfig[K] extends number ? K : never;
}[keyof AlpacaConfig];
type BoolKey = {
  [K in keyof AlpacaConfig]: AlpacaConfig[K] extends boolean ? K : never;
}[keyof AlpacaConfig];

const SafetyTab: FunctionalComponent<SettingsTabProps> = ({ config, update, error, hw, goTo }) => {
  const alpaca = { ...defaultAlpacaConfig, ...config.alpaca };
  const set = (key: keyof AlpacaConfig, value: unknown) => update(['alpaca', key], value);
  const [safety, setSafety] = useState<SafetyStatus | null>(null);

  useEffect(() => {
    const load = () =>
      fetch('/api/safety')
        .then((response) => (response.ok ? response.json() : null))
        .then(setSafety)
        .catch(() => setSafety(null));
    load();
    const timer = setInterval(load, 5000);
    return () => clearInterval(timer);
  }, []);

  // One threshold rule: a toggle, its limit, and why it can't be used.
  // A render function rather than a component defined in here, so the 5 s
  // safety refresh doesn't remount inputs (and drop focus) mid-edit.
  const rule = ({ enabledKey, valueKey, label, unit, min, max, step, hint, blockedReason, fix, fixLabel }: {
    enabledKey: BoolKey;
    valueKey?: NumericKey;
    label: string;
    unit?: string;
    min?: number;
    max?: number;
    step?: number;
    hint?: ComponentChildren;
    blockedReason?: string | null;
    fix?: () => void;
    fixLabel?: string;
  }) => {
    const on = alpaca[enabledKey];
    return (
      <div class="grid grid-cols-1 md:grid-cols-[1fr_12rem] gap-x-4 gap-y-2 items-start">
        <div>
          <Toggle
            label={label}
            checked={on}
            onChange={(v) => set(enabledKey, v)}
            hint={hint}
            blockedReason={
              blockedReason
                ? on
                  ? `${blockedReason} While this rule is on, the SafetyMonitor reports unsafe.`
                  : blockedReason
                : undefined
            }
          />
          {blockedReason && fix && (
            <button type="button" class="mt-1 ml-7 text-xs text-cyan-300 hover:underline" onClick={fix}>
              {fixLabel ?? 'Set up'} →
            </button>
          )}
        </div>
        {valueKey && (
          <div class="flex items-center gap-2 ml-7 md:ml-0">
            <NumberInput
              dataField={`alpaca.${valueKey}`}
              value={alpaca[valueKey]}
              min={min}
              max={max}
              step={step}
              disabled={!on}
              error={error(`alpaca.${valueKey}`)}
              ariaLabel={`${label} (${unit ?? ''})`}
              onChange={(v) => set(valueKey, v)}
            />
            {unit && <span class="text-xs text-gray-400 whitespace-nowrap">{unit}</span>}
          </div>
        )}
      </div>
    );
  };

  const toSensors = (anchor: string) => () => goTo('sensors', anchor);
  const rainReason = !hw.rain.enabled ? 'The rain sensor is turned off.' : null;
  const windReason = !hw.wind.enabled ? 'No anemometer is set up.' : null;
  const mlxReason = unavailableReason(hw.irSky, 'The MLX90614 IR sensor', 'wire');
  const tslReason = unavailableReason(hw.skyLight, 'The TSL2591 light sensor', 'wire');
  const bmeReason = unavailableReason(hw.environment, 'The BME280', 'wire');

  return (
    <>
      <SettingsCard
        id="alpaca"
        title="ASCOM Alpaca"
        description="Serves this device to N.I.N.A. and other Alpaca clients as a SafetyMonitor and ObservingConditions device."
        badge={<StatusBadge tone={alpaca.enabled ? 'ok' : 'off'} label={alpaca.enabled ? 'On' : 'Off'} />}
      >
        <Toggle
          label="Enable the Alpaca SafetyMonitor and ObservingConditions devices"
          checked={alpaca.enabled}
          onChange={(v) => set('enabled', v)}
          hint="Restart after turning on, so N.I.N.A.'s discovery (UDP port 32227) can find the device."
        />
        <button type="button" class="text-sm text-cyan-300 hover:underline" onClick={() => route('/alpaca')}>
          Device URLs, setup links and live values →
        </button>
      </SettingsCard>

      <SafetyCard safety={safety} />

      <SettingsCard
        id="safety"
        title="Safety rules"
        description={
          <>
            Any enabled rule that fails makes the verdict unsafe. The verdict drives the Dashboard, alerts and - when Alpaca is
            on - <code>IsSafe</code> in N.I.N.A. Rules for hardware you don't have are greyed out.
          </>
        }
      >
        <Group title="General">
          <Toggle
            label="Force unsafe (manual override)"
            checked={alpaca.manualOverrideUnsafe}
            onChange={(v) => set('manualOverrideUnsafe', v)}
            hint="E.g. while working on the observatory."
          />
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
            <Field label="Treat data as stale after (seconds)" error={error('alpaca.staleAfterSeconds')} hint="Stale data is unsafe. Default 30.">
              <NumberInput
                dataField="alpaca.staleAfterSeconds"
                integer
                min={1}
                max={3600}
                value={alpaca.staleAfterSeconds}
                onChange={(v) => set('staleAfterSeconds', v)}
              />
            </Field>
            <Field
              label="Safe delay (seconds)"
              error={error('alpaca.safeDelaySeconds')}
              hint="Must stay safe this long before reporting safe again. Unsafe is always immediate. 0 = off."
            >
              <NumberInput
                dataField="alpaca.safeDelaySeconds"
                integer
                min={0}
                max={3600}
                value={alpaca.safeDelaySeconds}
                onChange={(v) => set('safeDelaySeconds', v)}
              />
            </Field>
          </div>
        </Group>

        <Group
          title="Rain"
          aside={hw.rain.enabled && hw.rain.detected === false ? <StatusBadge tone="bad" label="RG-15 not responding" /> : undefined}
        >
          {rule({ enabledKey: "rainUnsafeEnabled", label: "Unsafe while raining", hint: `Including the rain clear delay (${Math.round(((config.rain ?? defaultRainConfig).rainClearDelayMs ?? 900000) / 60000)} min after the last drop). Checked even when other sensors are stale.`, blockedReason: rainReason, fix: toSensors('rain'), fixLabel: "Set up the rain sensor" })}
          {rule({ enabledKey: "rainSensorRequired", label: "Unsafe if the rain sensor stops responding", hint: "Fail-safe: also covers stale readings and the RG-15 lens-fault flag.", blockedReason: rainReason, fix: toSensors('rain'), fixLabel: "Set up the rain sensor" })}
        </Group>

        <Group title="Wind">
          {rule({ enabledKey: "windSpeedUnsafeEnabled", valueKey: "windSpeedUnsafeMs", label: "Maximum wind speed", unit: `m/s (${Math.round(alpaca.windSpeedUnsafeMs * 3.6)} km/h)`, min: 0.1, max: 60, step: 0.5, hint: "2-minute mean.", blockedReason: windReason, fix: toSensors('wind'), fixLabel: "Set up the anemometer" })}
          {rule({ enabledKey: "windGustUnsafeEnabled", valueKey: "windGustUnsafeMs", label: "Maximum gust", unit: `m/s (${Math.round(alpaca.windGustUnsafeMs * 3.6)} km/h)`, min: 0.1, max: 80, step: 0.5, hint: "Peak 3-second mean over 10 minutes.", blockedReason: windReason, fix: toSensors('wind'), fixLabel: "Set up the anemometer" })}
        </Group>

        <Group title="Sky">
          {rule({ enabledKey: "cloudCoverEnabled", valueKey: "cloudCoverUnsafePercent", label: "Maximum cloud cover", unit: "%", min: 0, max: 100, step: 1, blockedReason: mlxReason })}
          {rule({ enabledKey: "sqmMinEnabled", valueKey: "sqmMinSafe", label: "Minimum sky darkness (SQM)", unit: "mag/arcsec²", min: 0, max: 30, step: 0.1, hint: "E.g. 18 to treat twilight or moonlight as unsafe.", blockedReason: tslReason })}
        </Group>

        <Group title="Environment">
          {rule({ enabledKey: "humidityMaxEnabled", valueKey: "humidityMaxSafe", label: "Maximum humidity", unit: "%", min: 0, max: 100, step: 1, blockedReason: bmeReason })}
          {rule({ enabledKey: "dewpointMarginEnabled", valueKey: "dewpointMarginMinC", label: "Minimum temperature above dew point", unit: "°C", min: 0, max: 20, step: 0.1, hint: "Unsafe when dew is likely to form on optics.", blockedReason: bmeReason })}
        </Group>

        {!alpaca.enabled && (
          <Requires>Alpaca is off, so N.I.N.A. can't see this verdict - it still drives the Dashboard and alerts.</Requires>
        )}
      </SettingsCard>
    </>
  );
};

export default SafetyTab;
