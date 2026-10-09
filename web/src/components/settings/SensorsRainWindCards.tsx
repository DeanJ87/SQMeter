import { FunctionalComponent } from 'preact';
import type { SettingsTabProps } from './context';
import { defaultRainConfig, defaultWindConfig } from './defaults';
import type { SensorAvailability } from './hardware';
import { useRainTest } from '../../hooks/useSensorActions';
import {
  ActionButton,
  DepToggle,
  Field,
  Group,
  NumberInput,
  Requires,
  ResultNote,
  SelectInput,
  SettingsCard,
  StatusBadge,
  Toggle,
} from './controls';
import { t } from '../../i18n';

const ANEMOMETER_PRESETS = [
  { value: '2.4', label: t('settings.sensors.misolArgentSparkfun') },
  { value: '3.621', label: t('settings.sensors.davis6410') },
];

const detectionBadge = (
  sensor: SensorAvailability,
  labels = { ok: t('settings.sensors.detected'), bad: t('settings.sensors.notDetected') },
) => {
  if (!sensor.enabled) return undefined;
  if (sensor.detected === null) return undefined;
  return <StatusBadge tone={sensor.detected ? 'ok' : 'bad'} label={sensor.detected ? labels.ok : labels.bad} />;
};

// Pins, baud rate, timing, resolution and units.
const RainFields: FunctionalComponent<SettingsTabProps> = ({ config, update, error }) => {
  const rain = config.rain ?? defaultRainConfig;
  return (
    <div class="form-grid">
      <Field label={t('settings.sensors.rxPin')} error={error('rain.rxPin')} hint={t('settings.sensors.fromTheRg15S')}>
        <NumberInput dataField="rain.rxPin" integer min={0} max={39} value={rain.rxPin} onChange={(v) => update(['rain', 'rxPin'], v)} />
      </Field>
      <Field label={t('settings.sensors.txPin')} error={error('rain.txPin')} hint={t('settings.sensors.toTheRg15S')}>
        <NumberInput dataField="rain.txPin" integer min={0} max={39} value={rain.txPin} onChange={(v) => update(['rain', 'txPin'], v)} />
      </Field>
      <Field label={t('settings.sensors.baudRate')} error={error('rain.baudRate')}>
        <SelectInput
          dataField="rain.baudRate"
          value={String(rain.baudRate)}
          options={['2400', '4800', '9600', '19200'].map((b) => ({ value: b, label: b }))}
          onChange={(v) => update(['rain', 'baudRate'], Number(v))}
        />
      </Field>
      <Field label={t('settings.sensors.pollEvery')} error={error('rain.pollIntervalMs')}>
        <NumberInput
          dataField="rain.pollIntervalMs"
          integer
          min={1}
          max={3600}
          unit="s"
          value={Math.round((rain.pollIntervalMs ?? 5000) / 1000)}
          onChange={(v) => update(['rain', 'pollIntervalMs'], Math.max(1, v || 5) * 1000)}
        />
      </Field>
      <Field
        label={t('settings.sensors.rainClearDelay')}
        error={error('rain.rainClearDelayMs')}
        hint={t('settings.sensors.stillCountsAsRainingThis')}
      >
        <NumberInput
          dataField="rain.rainClearDelayMs"
          integer
          min={1}
          max={1440}
          unit="min"
          value={Math.round((rain.rainClearDelayMs ?? 900000) / 60000)}
          onChange={(v) => update(['rain', 'rainClearDelayMs'], Math.max(1, v || 15) * 60000)}
        />
      </Field>
      <Field label={t('settings.sensors.resolution')}>
        <SelectInput
          value={rain.resolution ?? 'switch'}
          options={[
            { value: 'high', label: t('settings.sensors.high001Mm') },
            { value: 'low', label: t('settings.sensors.low02Mm') },
            { value: 'switch', label: t('settings.sensors.dipSwitch') },
          ]}
          onChange={(v) => update(['rain', 'resolution'], v)}
        />
      </Field>
      <Field label={t('settings.sensors.units')}>
        <SelectInput
          value={rain.units ?? 'metric'}
          options={[
            { value: 'metric', label: 'mm' },
            { value: 'imperial', label: 'inches' },
            { value: 'switch', label: t('settings.sensors.dipSwitch') },
          ]}
          onChange={(v) => update(['rain', 'units'], v)}
        />
      </Field>
    </div>
  );
};

// The RG-15 rain sensor.
export const RainCard: FunctionalComponent<SettingsTabProps> = (props) => {
  const { config, update, updateMany, error, hw, dirty, deps, fix } = props;
  const { testingRain, rainResult, testRain } = useRainTest();
  const rain = config.rain ?? defaultRainConfig;
  const setDailyReset = (value: string) => {
    const [hour, minute] = value.split(':').map(Number);
    updateMany([
      [['rain', 'dailyResetHour'], Number.isFinite(hour) ? hour : 0],
      [['rain', 'dailyResetMinute'], Number.isFinite(minute) ? minute : 0],
    ]);
  };
  return (
    <SettingsCard
      id="rain"
      title={t('settings.sensors.rainSensor')}
      hint={t('settings.sensors.hydreonRg15OnA')}
      badge={detectionBadge(hw.rain, { ok: t('settings.sensors.responding'), bad: t('settings.sensors.notResponding') })}
    >
      <Toggle label={t('settings.sensors.rg15RainSensor')} checked={rain.enabled} onChange={(v) => update(['rain', 'enabled'], v)} />
      {rain.enabled && hw.rain.detected === false && (
        <Requires tone="warn">{t('settings.sensors.noReplyCheckOutGpio', { rxPin: rain.rxPin, txPin: rain.txPin })}</Requires>
      )}
      {rain.enabled && (
        <>
          <RainFields {...props} />
          <DepToggle
            entry={deps.get('rain.dailyResetEnabled')}
            onFix={fix}
            label={t('settings.sensors.resetTheDailyTotal')}
            checked={rain.dailyResetEnabled ?? false}
            onChange={(v) => update(['rain', 'dailyResetEnabled'], v)}
          />
          {rain.dailyResetEnabled && (
            <div class="form-grid indent">
              <Field label={t('settings.sensors.at')} error={error('rain.dailyResetHour') ?? error('rain.dailyResetMinute')}>
                <input
                  data-field="rain.dailyResetHour"
                  type="time"
                  class="input"
                  value={`${String(rain.dailyResetHour ?? 0).padStart(2, '0')}:${String(rain.dailyResetMinute ?? 0).padStart(2, '0')}`}
                  onChange={(e) => setDailyReset((e.target as HTMLInputElement).value)}
                />
              </Field>
            </div>
          )}
          <Toggle
            label={t('settings.sensors.logSerialTraffic')}
            checked={rain.debugUart}
            onChange={(v) => update(['rain', 'debugUart'], v)}
            hint={t('settings.sensors.troubleshootingOnly')}
          />
          <div class="btn-row">
            <ActionButton
              onClick={testRain}
              busy={testingRain}
              busyLabel={t('common.testing')}
              disabled={hw.rain.savedEnabled === false || dirty}
              title={dirty ? t('settings.sensors.saveFirst') : undefined}
            >
              {t('settings.sensors.testCommunication')}
            </ActionButton>
            <ResultNote result={rainResult} />
          </div>
        </>
      )}
    </SettingsCard>
  );
};

// The cup anemometer and the wind vane.
export const WindCard: FunctionalComponent<SettingsTabProps> = ({ config, update, error, hw, deps, fix }) => {
  const wind = { ...defaultWindConfig, ...config.wind };
  const windPreset = ANEMOMETER_PRESETS.find((p) => Math.abs(Number(p.value) - wind.kmhPerHz) < 0.0005)?.value ?? 'custom';
  return (
    <SettingsCard
      id="wind"
      title={t('settings.sensors.wind')}
      hint={t('settings.sensors.reedSwitchCupAnemometerOptional')}
      badge={detectionBadge(hw.wind, { ok: t('settings.sensors.running'), bad: t('settings.sensors.notReporting') })}
    >
      <Toggle label={t('settings.sensors.anemometer')} checked={wind.enabled} onChange={(v) => update(['wind', 'enabled'], v)} />
      {wind.enabled && (
        <>
          <div class="form-grid">
            <Field label={t('settings.sensors.pin')} error={error('wind.speedPin')} hint={t('settings.sensors.switchToGndInternalPull')}>
              <NumberInput dataField="wind.speedPin" integer value={wind.speedPin} onChange={(v) => update(['wind', 'speedPin'], v)} />
            </Field>
            <Field label={t('settings.sensors.model')}>
              <SelectInput
                value={windPreset}
                options={[...ANEMOMETER_PRESETS, { value: 'custom', label: t('settings.sensors.other') }]}
                onChange={(v) => v !== 'custom' && update(['wind', 'kmhPerHz'], Number(v))}
              />
            </Field>
            <Field label={t('settings.sensors.speedPerPulse')} error={error('wind.kmhPerHz')} hint={t('settings.sensors.kmhPerClosure')}>
              <NumberInput
                ariaLabel={t('settings.sensors.kmHPerHz')}
                step={0.001}
                min={0.001}
                max={20}
                unit="km/h·Hz⁻¹"
                value={wind.kmhPerHz}
                onChange={(v) => update(['wind', 'kmhPerHz'], v)}
              />
            </Field>
          </div>
          <Group
            title={t('settings.sensors.windVane')}
            aside={
              wind.directionEnabled && hw.windVane.detected === false ? (
                <StatusBadge tone="bad" label={t('settings.sensors.vaneFault')} />
              ) : undefined
            }
          >
            <DepToggle
              entry={deps.get('wind.directionEnabled')}
              onFix={fix}
              label={t('settings.sensors.windVane')}
              checked={wind.directionEnabled}
              onChange={(v) => update(['wind', 'directionEnabled'], v)}
            />
            {wind.directionEnabled && (
              <div class="form-grid">
                <Field label={t('settings.sensors.pin')} error={error('wind.directionPin')} hint={t('settings.sensors.gpio3239OnlyAdc2')}>
                  <NumberInput
                    dataField="wind.directionPin"
                    integer
                    min={32}
                    max={39}
                    value={wind.directionPin}
                    onChange={(v) => update(['wind', 'directionPin'], v)}
                  />
                </Field>
                <Field
                  label={t('settings.sensors.pullUp')}
                  error={error('wind.vanePullupOhms')}
                  hint={t('settings.sensors.resistorFromTheVanePin')}
                >
                  <NumberInput step={100} unit="Ω" value={wind.vanePullupOhms} onChange={(v) => update(['wind', 'vanePullupOhms'], v)} />
                </Field>
                <Field
                  label={t('settings.sensors.northOffset')}
                  error={error('wind.directionOffsetDeg')}
                  hint={t('settings.sensors.ifTheVaneIsnT')}
                >
                  <NumberInput
                    step={1}
                    unit="°"
                    value={wind.directionOffsetDeg}
                    onChange={(v) => update(['wind', 'directionOffsetDeg'], v)}
                  />
                </Field>
              </div>
            )}
          </Group>
        </>
      )}
    </SettingsCard>
  );
};
