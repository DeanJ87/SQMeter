import { FunctionalComponent } from 'preact';
import { BATTERY_PIN_UNSET, type BatteryConfig } from '../../types';
import type { DepEntry } from '../../lib/settingsDeps';
import type { SettingsTabProps } from './context';
import { DepNote, Field, Group, NumberInput, SettingsCard, StatusBadge, Toggle } from './controls';
import { defaultBatteryConfig } from './defaults';
import { t } from '../../i18n';

// Settings -> Device -> Battery: the Li-ion pack and fuel gauge (spec 032,
// contracts/ui.md, mockups s1-s3). Without a gauge every row but the pins is
// muted with "No battery gauge detected" (D-46, said once under the monitoring
// switch); monitoring ships on.

type NumberKey = 'lowPercent' | 'criticalPercent' | 'shutdownPercent' | 'saveReadIntervalSeconds' | 'shutdownGraceSeconds';
type PinKey = 'alertPin' | 'stat1Pin' | 'stat2Pin' | 'pgPin';
interface NumberRow {
  label: string;
  unit: string;
  min: number;
  max: number;
  hint?: string;
  disabled?: boolean;
}

const badgeFor = (enabled: boolean, gauge: DepEntry) => {
  if (!enabled) return <StatusBadge tone="off" label={t('settings.display.off')} />;
  if (gauge.state === 'active') return <StatusBadge tone="ok" label={t('settings.display.detected')} />;
  if (gauge.state === 'inactive') return <StatusBadge tone="off" label={t('settings.display.notDetected')} />;
  return undefined;
};

const PINS: { key: PinKey; label: () => string }[] = [
  { key: 'alertPin', label: () => t('settings.battery.gaugeAlertPin') },
  { key: 'stat1Pin', label: () => t('settings.battery.stat1Pin') },
  { key: 'stat2Pin', label: () => t('settings.battery.stat2Pin') },
  { key: 'pgPin', label: () => t('settings.battery.pgPin') },
];

const PinFields: FunctionalComponent<{
  battery: BatteryConfig;
  set: (key: keyof BatteryConfig, value: unknown) => void;
  error: (key: string) => string | undefined;
}> = ({ battery, set, error }) => (
  <Group title={t('settings.battery.pins')}>
    <div class="form-grid">
      {PINS.map(({ key, label }) => (
        <Field key={key} label={label()} error={error(`battery.${key}`)}>
          <NumberInput
            dataField={`battery.${key}`}
            integer
            min={0}
            max={39}
            value={battery[key] === BATTERY_PIN_UNSET ? Number.NaN : battery[key]}
            error={error(`battery.${key}`)}
            onChange={(v) => set(key, Number.isFinite(v) ? v : BATTERY_PIN_UNSET)}
          />
        </Field>
      ))}
    </div>
  </Group>
);

const BatteryCard: FunctionalComponent<SettingsTabProps> = ({ config, update, error, deps, fix }) => {
  if (!config.battery) return null; // older firmware
  const battery = { ...defaultBatteryConfig, ...config.battery };
  const set = (key: keyof BatteryConfig, value: unknown) => update(['battery', key], value);
  const gauge = deps.get('battery.enabled');
  const missing = gauge.state === 'inactive';
  const locked = !battery.enabled || missing;
  const saveLocked = locked || !battery.powerSaving;
  const number = (key: NumberKey, { label, unit, min, max, hint, disabled = locked }: NumberRow) => (
    <Field label={label} hint={hint} error={error(`battery.${key}`)}>
      <NumberInput
        dataField={`battery.${key}`}
        integer
        min={min}
        max={max}
        unit={unit}
        value={battery[key]}
        disabled={disabled}
        error={error(`battery.${key}`)}
        onChange={(v) => set(key, v)}
      />
    </Field>
  );

  return (
    <SettingsCard id="battery" title={t('settings.battery.title')} badge={badgeFor(battery.enabled, gauge)}>
      <Toggle label={t('settings.battery.monitorBattery')} checked={battery.enabled} onChange={(v) => set('enabled', v)} />
      {missing && (
        <div class="indent">
          <DepNote entry={gauge} onFix={fix} />
        </div>
      )}
      <div class="form-grid">
        {number('lowPercent', {
          label: t('settings.battery.lowLevel'),
          unit: '%',
          min: 5,
          max: 50,
          hint: t('settings.battery.lowLevelHint'),
        })}
        {number('criticalPercent', {
          label: t('settings.battery.criticalLevel'),
          unit: '%',
          min: 2,
          max: 25,
          hint: t('settings.battery.criticalLevelHint'),
        })}
        {number('shutdownPercent', {
          label: t('settings.battery.shutdownLevel'),
          unit: '%',
          min: 2,
          max: 10,
          hint: t('settings.battery.shutdownLevelHint'),
        })}
        <Field
          label={t('settings.battery.packCapacity')}
          hint={t('settings.battery.packCapacityHint')}
          error={error('battery.capacityMah')}
        >
          <NumberInput
            dataField="battery.capacityMah"
            integer
            min={0}
            max={50000}
            unit="mAh"
            value={battery.capacityMah === 0 ? Number.NaN : battery.capacityMah}
            disabled={locked}
            error={error('battery.capacityMah')}
            onChange={(v) => set('capacityMah', Number.isFinite(v) ? v : 0)}
          />
        </Field>
      </div>
      <Toggle
        label={t('settings.battery.savePowerWhenLow')}
        checked={battery.powerSaving}
        disabled={locked}
        onChange={(v) => set('powerSaving', v)}
        hint={t('settings.battery.savePowerWhenLowHint')}
      />
      <div class="form-grid indent">
        {number('saveReadIntervalSeconds', {
          label: t('settings.battery.readSensorsEvery'),
          unit: 's',
          min: 10,
          max: 300,
          disabled: saveLocked,
        })}
        {number('shutdownGraceSeconds', {
          label: t('settings.battery.warnBeforeSleeping'),
          unit: 's',
          min: 10,
          max: 300,
          disabled: saveLocked,
        })}
      </div>
      <PinFields battery={battery} set={set} error={error} />
    </SettingsCard>
  );
};

export default BatteryCard;
