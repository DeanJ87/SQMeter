import { ComponentChildren, FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { route } from 'preact-router';
import type { AlpacaConfig, SafetyStatus } from '../../types';
import SafetyCard from '../SafetyCard';
import type { SettingsTabProps } from './context';
import { defaultAlpacaConfig, defaultRainConfig } from './defaults';
import { Button } from '../ui';
import { Field, Group, NumberInput, SettingsCard, StatusBadge, Toggle } from './controls';

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
  const rule = ({ enabledKey, valueKey, label, unit, min, max, step, hint, blockedReason, fix }: {
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
  }) => {
    const on = alpaca[enabledKey];
    return (
      <div class="rule-row">
        <Toggle
          label={label}
          checked={on}
          onChange={(v) => set(enabledKey, v)}
          hint={hint}
          blockedReason={blockedReason ? (on ? `${blockedReason} Reports unsafe while on.` : blockedReason) : undefined}
          onFix={fix}
        />
        {valueKey && (
          <NumberInput
            dataField={`alpaca.${valueKey}`}
            value={alpaca[valueKey]}
            min={min}
            max={max}
            step={step}
            unit={unit}
            disabled={!on}
            error={error(`alpaca.${valueKey}`)}
            ariaLabel={label}
            onChange={(v) => set(valueKey, v)}
          />
        )}
      </div>
    );
  };

  const toSensors = (anchor: string) => () => goTo('sensors', anchor);
  const rainReason = !hw.rain.enabled ? 'Rain sensor is off.' : null;
  const windReason = !hw.wind.enabled ? 'No anemometer set up.' : null;
  const mlxReason = hw.irSky.detected === false ? 'MLX90614 not detected.' : null;
  const tslReason = hw.skyLight.detected === false ? 'TSL2591 not detected.' : null;
  const bmeReason = hw.environment.detected === false ? 'BME280 not detected.' : null;
  const clearDelay = Math.round((config.rain ?? defaultRainConfig).rainClearDelayMs / 60000);

  return (
    <>
      <SettingsCard
        id="alpaca"
        title="ASCOM Alpaca"
        hint="SafetyMonitor and ObservingConditions devices for N.I.N.A. and other Alpaca clients."
      >
        <Toggle label="Serve Alpaca devices" checked={alpaca.enabled} onChange={(v) => set('enabled', v)} />
        <div>
          <Button variant="link" onClick={() => route('/alpaca')}>
            Device URLs and live values →
          </Button>
        </div>
      </SettingsCard>

      <SafetyCard safety={safety} showRulesLink={false} />

      <SettingsCard id="safety" title="Safety rules" hint="Any enabled rule that fails makes the verdict unsafe. The verdict drives the Dashboard, alerts and IsSafe in N.I.N.A.">
        <Group title="General">
          <Toggle
            label="Force unsafe"
            checked={alpaca.manualOverrideUnsafe}
            onChange={(v) => set('manualOverrideUnsafe', v)}
            hint="Manual override, e.g. while working on the observatory."
          />
          <div class="form-grid">
            <Field label="Stale after" error={error('alpaca.staleAfterSeconds')} hint="Sensor data older than this is unsafe.">
              <NumberInput dataField="alpaca.staleAfterSeconds" integer min={1} max={3600} unit="s" value={alpaca.staleAfterSeconds} onChange={(v) => set('staleAfterSeconds', v)} />
            </Field>
            <Field label="Safe delay" error={error('alpaca.safeDelaySeconds')} hint="Must stay safe this long before reporting safe again. Unsafe is always immediate.">
              <NumberInput dataField="alpaca.safeDelaySeconds" integer min={0} max={3600} unit="s" value={alpaca.safeDelaySeconds} onChange={(v) => set('safeDelaySeconds', v)} />
            </Field>
          </div>
        </Group>

        <Group title="Rain" aside={hw.rain.enabled && hw.rain.detected === false ? <StatusBadge tone="bad" label="Not responding" /> : undefined}>
          {rule({ enabledKey: 'rainUnsafeEnabled', label: 'Unsafe while raining', hint: `Including ${clearDelay} min after the last drop. Checked even when other sensors are stale.`, blockedReason: rainReason, fix: toSensors('rain') })}
          {rule({ enabledKey: 'rainSensorRequired', label: 'Unsafe if the rain sensor fails', hint: 'No reply, stale readings or a lens fault.', blockedReason: rainReason, fix: toSensors('rain') })}
        </Group>

        <Group title="Wind">
          {rule({ enabledKey: 'windSpeedUnsafeEnabled', valueKey: 'windSpeedUnsafeMs', label: 'Max wind speed', unit: `m/s · ${Math.round(alpaca.windSpeedUnsafeMs * 3.6)} km/h`, min: 0.1, max: 60, step: 0.5, hint: '2-minute mean.', blockedReason: windReason, fix: toSensors('wind') })}
          {rule({ enabledKey: 'windGustUnsafeEnabled', valueKey: 'windGustUnsafeMs', label: 'Max gust', unit: `m/s · ${Math.round(alpaca.windGustUnsafeMs * 3.6)} km/h`, min: 0.1, max: 80, step: 0.5, hint: 'Highest 3-second mean in 10 minutes.', blockedReason: windReason, fix: toSensors('wind') })}
        </Group>

        <Group title="Sky">
          {rule({ enabledKey: 'cloudCoverEnabled', valueKey: 'cloudCoverUnsafePercent', label: 'Max cloud cover', unit: '%', min: 0, max: 100, step: 1, blockedReason: mlxReason })}
          {rule({ enabledKey: 'sqmMinEnabled', valueKey: 'sqmMinSafe', label: 'Min sky darkness', unit: 'mag/arcsec²', min: 0, max: 30, step: 0.1, hint: 'E.g. 18 to treat twilight and moonlight as unsafe.', blockedReason: tslReason })}
        </Group>

        <Group title="Environment">
          {rule({ enabledKey: 'humidityMaxEnabled', valueKey: 'humidityMaxSafe', label: 'Max humidity', unit: '%', min: 0, max: 100, step: 1, blockedReason: bmeReason })}
          {rule({ enabledKey: 'dewpointMarginEnabled', valueKey: 'dewpointMarginMinC', label: 'Min margin above dew point', unit: '°C', min: 0, max: 20, step: 0.1, hint: 'Dew forms on optics below this.', blockedReason: bmeReason })}
        </Group>
      </SettingsCard>
    </>
  );
};

export default SafetyTab;
