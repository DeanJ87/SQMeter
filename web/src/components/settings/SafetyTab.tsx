import { ComponentChildren, FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { route } from 'preact-router';
import type { AlpacaConfig, SafetyStatus } from '../../types';
import SafetyCard from '../SafetyCard';
import type { SettingsTabProps } from './context';
import { defaultAlpacaConfig, defaultRainConfig } from './defaults';
import { Button } from '../ui';
import { DepToggle, Field, Group, NumberInput, SettingsCard, StatusBadge, Toggle } from './controls';
import { t } from '../../i18n';

type NumericKey = {
  [K in keyof AlpacaConfig]: AlpacaConfig[K] extends number ? K : never;
}[keyof AlpacaConfig];
type BoolKey = {
  [K in keyof AlpacaConfig]: AlpacaConfig[K] extends boolean ? K : never;
}[keyof AlpacaConfig];

const SafetyTab: FunctionalComponent<SettingsTabProps> = ({ config, update, error, hw, deps, fix }) => {
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
  // A rule whose sensor is off or missing either isn't in effect or reports
  // unsafe, as the device decides (D-15..D-19).
  const rule = ({
    enabledKey,
    valueKey,
    label,
    unit,
    min,
    max,
    step,
    hint,
  }: {
    enabledKey: BoolKey;
    valueKey?: NumericKey;
    label: string;
    unit?: string;
    min?: number;
    max?: number;
    step?: number;
    hint?: ComponentChildren;
  }) => {
    const on = alpaca[enabledKey];
    const entry = deps.get(`alpaca.${enabledKey}`);
    return (
      <div class="rule-row">
        <DepToggle
          entry={entry}
          onFix={fix}
          prefix={entry.unmet === 'fail-safe' ? t('settings.safety.reportsUnsafe') : t('settings.safety.notInEffect')}
          label={label}
          checked={on}
          onChange={(v) => set(enabledKey, v)}
          hint={hint}
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

  const clearDelay = Math.round((config.rain ?? defaultRainConfig).rainClearDelayMs / 60000);

  return (
    <>
      <SettingsCard id="alpaca" title="ASCOM Alpaca" hint={t('settings.safety.safetymonitorAndObservingconditionsDevic')}>
        <Toggle label={t('settings.safety.serveAlpacaDevices')} checked={alpaca.enabled} onChange={(v) => set('enabled', v)} />
        <div>
          <Button variant="link" onClick={() => route('/alpaca')}>
            {t('settings.safety.deviceUrlsAndLiveValues')}
          </Button>
        </div>
      </SettingsCard>

      <SafetyCard safety={safety} showRulesLink={false} />

      <SettingsCard id="safety" title={t('settings.safety.safetyRules')} hint={t('settings.safety.anyEnabledRuleThatFails')}>
        <Group title={t('settings.safety.general')}>
          <Toggle
            label={t('settings.safety.forceUnsafe')}
            checked={alpaca.manualOverrideUnsafe}
            onChange={(v) => set('manualOverrideUnsafe', v)}
            hint={t('settings.safety.manualOverrideEGWhile')}
          />
          <div class="form-grid">
            <Field
              label={t('settings.safety.staleAfter')}
              error={error('alpaca.staleAfterSeconds')}
              hint={t('settings.safety.sensorDataOlderThanThis')}
            >
              <NumberInput
                dataField="alpaca.staleAfterSeconds"
                integer
                min={1}
                max={3600}
                unit="s"
                value={alpaca.staleAfterSeconds}
                onChange={(v) => set('staleAfterSeconds', v)}
              />
            </Field>
            <Field
              label={t('settings.safety.safeDelay')}
              error={error('alpaca.safeDelaySeconds')}
              hint={t('settings.safety.mustStaySafeThisLong')}
            >
              <NumberInput
                dataField="alpaca.safeDelaySeconds"
                integer
                min={0}
                max={3600}
                unit="s"
                value={alpaca.safeDelaySeconds}
                onChange={(v) => set('safeDelaySeconds', v)}
              />
            </Field>
          </div>
        </Group>

        <Group
          title={t('settings.safety.rain')}
          aside={
            hw.rain.enabled && hw.rain.detected === false ? (
              <StatusBadge tone="bad" label={t('settings.safety.notResponding')} />
            ) : undefined
          }
        >
          {rule({
            enabledKey: 'rainUnsafeEnabled',
            label: t('settings.safety.unsafeWhileRaining'),
            hint: t('settings.safety.includingCleardelayMinAfterThe', { clearDelay }),
          })}
          {rule({
            enabledKey: 'rainSensorRequired',
            label: t('settings.safety.unsafeIfTheRainSensor'),
            hint: t('settings.safety.noReplyStaleReadingsOr'),
          })}
        </Group>

        <Group title={t('settings.safety.wind')}>
          {rule({
            enabledKey: 'windSpeedUnsafeEnabled',
            valueKey: 'windSpeedUnsafeMs',
            label: t('settings.safety.maxWindSpeed'),
            unit: `m/s · ${Math.round(alpaca.windSpeedUnsafeMs * 3.6)} km/h`,
            min: 0.1,
            max: 60,
            step: 0.5,
            hint: t('settings.safety.twoMinuteMean'),
          })}
          {rule({
            enabledKey: 'windGustUnsafeEnabled',
            valueKey: 'windGustUnsafeMs',
            label: t('settings.safety.maxGust'),
            unit: `m/s · ${Math.round(alpaca.windGustUnsafeMs * 3.6)} km/h`,
            min: 0.1,
            max: 80,
            step: 0.5,
            hint: t('settings.safety.highest3SecondMeanIn'),
          })}
        </Group>

        <Group title={t('settings.safety.sky')}>
          {rule({
            enabledKey: 'cloudCoverEnabled',
            valueKey: 'cloudCoverUnsafePercent',
            label: t('settings.safety.maxCloudCover'),
            unit: '%',
            min: 0,
            max: 100,
            step: 1,
          })}
          {rule({
            enabledKey: 'sqmMinEnabled',
            valueKey: 'sqmMinSafe',
            label: t('settings.safety.minSkyDarkness'),
            unit: 'mag/arcsec²',
            min: 0,
            max: 30,
            step: 0.1,
            hint: t('settings.safety.eG18ToTreat'),
          })}
        </Group>

        <Group title={t('settings.safety.environment')}>
          {rule({
            enabledKey: 'humidityMaxEnabled',
            valueKey: 'humidityMaxSafe',
            label: t('settings.safety.maxHumidity'),
            unit: '%',
            min: 0,
            max: 100,
            step: 1,
          })}
          {rule({
            enabledKey: 'dewpointMarginEnabled',
            valueKey: 'dewpointMarginMinC',
            label: t('settings.safety.minMarginAboveDewPoint'),
            unit: '°C',
            min: 0,
            max: 20,
            step: 0.1,
            hint: t('settings.safety.dewFormsOnOpticsBelow'),
          })}
        </Group>
      </SettingsCard>
    </>
  );
};

export default SafetyTab;
