import { FunctionalComponent } from 'preact';
import type { AlpacaConfig } from '../../types';
import type { DepEntry, EffectiveView } from '../../lib/settingsDeps';
import { DepToggle, Group, NumberInput, Toggle } from './controls';
import { t } from '../../i18n';

// Settings -> Safety -> Battery (spec 032, contracts/ui.md): unsafe below a
// charge, and what happens when that rule trips. Without a gauge the rule
// reports unsafe (D-47, fail-safe), so it is shown whenever it is on.

interface Props {
  alpaca: AlpacaConfig;
  set: (key: keyof AlpacaConfig, value: unknown) => void;
  error: (key: string) => string | undefined;
  deps: EffectiveView;
  fix: (entry: DepEntry) => void;
}

const SafetyBatteryGroup: FunctionalComponent<Props> = ({ alpaca, set, error, deps, fix }) => {
  const on = Boolean(alpaca.batteryEnabled);
  const label = t('settings.safety.unsafeBelow');
  return (
    <Group title={t('settings.safety.battery')}>
      <div class="rule-row">
        <DepToggle
          entry={deps.get('alpaca.batteryEnabled')}
          onFix={fix}
          prefix={t('settings.safety.reportsUnsafe')}
          label={label}
          checked={on}
          onChange={(v) => set('batteryEnabled', v)}
          hint={t('settings.safety.batteryRuleHint')}
        />
        <NumberInput
          dataField="alpaca.batteryMinPercent"
          integer
          value={alpaca.batteryMinPercent ?? 10}
          min={2}
          max={50}
          unit="%"
          disabled={!on}
          error={error('alpaca.batteryMinPercent')}
          ariaLabel={label}
          onChange={(v) => set('batteryMinPercent', v)}
        />
      </div>
      <div class="indent toggle-stack">
        <Toggle
          label={t('settings.safety.sendAnAlert')}
          checked={alpaca.batteryAlert ?? true}
          disabled={!on}
          onChange={(v) => set('batteryAlert', v)}
          hint={t('settings.safety.sendAnAlertHint')}
        />
        <DepToggle
          entry={deps.get('alpaca.batteryPhoneAlarm')}
          onFix={fix}
          label={t('settings.safety.ringPairedPhones')}
          checked={alpaca.batteryPhoneAlarm ?? true}
          disabled={!on}
          onChange={(v) => set('batteryPhoneAlarm', v)}
        />
      </div>
    </Group>
  );
};

export default SafetyBatteryGroup;
