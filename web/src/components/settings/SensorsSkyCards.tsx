import { FunctionalComponent } from 'preact';
import type { SettingsTabProps } from './context';
import type { SkyRole } from '../../types';
import { defaultSkyAveraging, defaultSkyCalibration, I2C2_PIN_UNSET } from './defaults';
import type { SensorAvailability } from './hardware';
import { useDarkCalibration } from '../../hooks/useSensorActions';
import { ActionButton, DepToggle, Field, Group, NumberInput, ResultNote, SelectInput, SettingsCard, StatusBadge, Toggle } from './controls';
import { Note } from '../ui';
import { t } from '../../i18n';
import { SENSOR_MODEL, sensorName, skySensorName } from '../../lib/sensorNames';
import { otherSky, skyChoice } from '../../dashboard/skySensors';
import { formatDateTime, formatNumber } from '../../i18n/format';

const CLOCK_VALID = 1704067200; // calibration times below this are uptime, not dates

const detected = (sensor: SensorAvailability) => {
  if (sensor.detected === null) return undefined;
  return sensor.detected ? <StatusBadge tone="ok" label="OK" /> : <StatusBadge tone="bad" label={t('settings.sensors.notDetected')} />;
};

const SensorRow: FunctionalComponent<{ name: string; model: string }> = ({ name, model, children }) => (
  <div class="reading-row">
    <span class="reading-label">
      {name} <span class="system-subtle mono">{model}</span>
    </span>
    {children ?? <span class="note">{t('common.checking')}</span>}
  </div>
);

// A sky sensor's state: off, or what /api/status says of it (spec 030). The
// SQM falls back to the light sensor's state on firmware without skySensors.
const skyBadge = (props: SettingsTabProps, role: SkyRole) => {
  if (!skyChoice(props.config).enabled[role]) return <span class="note note-muted">{t('status.off')}</span>;
  const entry = props.status?.skySensors?.[role];
  if (entry) return detected({ enabled: true, detected: entry.detected, savedEnabled: true });
  return role === 'sqm' ? detected(props.hw.skyLight) : undefined;
};

// The I2C-2 pins: empty = not wired (255).
const I2c2Field: FunctionalComponent<SettingsTabProps & { pin: 'i2c2SDA' | 'i2c2SCL'; label: string }> = ({
  config,
  update,
  error,
  pin,
  label,
}) => {
  const value = config.sensor[pin] ?? I2C2_PIN_UNSET;
  return (
    <Field
      label={label}
      error={pin === 'i2c2SDA' ? error('sensor.i2c2SDA') : undefined}
      hint={pin === 'i2c2SDA' ? t('settings.sensors.i2c2Hint') : undefined}
    >
      <NumberInput
        dataField={`sensor.${pin}`}
        integer
        min={0}
        max={39}
        value={value === I2C2_PIN_UNSET ? Number.NaN : value}
        onChange={(v) => update(['sensor', pin], Number.isFinite(v) ? v : I2C2_PIN_UNSET)}
      />
    </Field>
  );
};

// SQM and SQM-L: a row and an on switch each (spec 030 FR-013). The SQM-L
// needs the second I2C bus (D-39).
const SkyRoleRows: FunctionalComponent<SettingsTabProps> = (props) => {
  const { config, update, deps, fix } = props;
  const choice = skyChoice(config);
  return (
    <>
      {(['sqm', 'sqml'] as const).map((role) => {
        const name = skySensorName(role);
        const toggle = {
          label: t('settings.sensors.useSkySensor', { name }),
          checked: choice.enabled[role],
          dataField: role === 'sqm' ? 'sky.sqmEnabled' : 'sky.sqmlEnabled',
          onChange: (v: boolean) => update(['sky', role === 'sqm' ? 'sqmEnabled' : 'sqmlEnabled'], v),
        };
        return (
          <div key={role}>
            <SensorRow name={name} model={SENSOR_MODEL.light}>
              {skyBadge(props, role)}
            </SensorRow>
            <div class="indent">
              {role === 'sqml' ? <DepToggle entry={deps.get('sky.sqmlEnabled')} onFix={fix} {...toggle} /> : <Toggle {...toggle} />}
            </div>
          </div>
        );
      })}
    </>
  );
};

// Whose reading counts, and whether the dashboard shows the other (D-40); a
// note when the chosen sensor is off (FR-010).
const SkyChoiceFields: FunctionalComponent<SettingsTabProps> = ({ config, update, deps, fix }) => {
  const choice = skyChoice(config);
  const authoritative = deps.get('sky.authoritative');
  const other = otherSky(choice.authoritative);
  return (
    <>
      <div class="form-grid">
        <Field label={t('settings.sensors.authoritativeReading')} hint={t('settings.sensors.authoritativeReadingHint')}>
          <SelectInput
            dataField="sky.authoritative"
            value={choice.authoritative}
            disabled={authoritative.state === 'inactive' && choice.enabled[choice.authoritative]}
            options={(['sqm', 'sqml'] as const).map((role) => ({ value: role, label: skySensorName(role) }))}
            onChange={(v) => update(['sky', 'authoritative'], v)}
          />
        </Field>
      </div>
      <DepToggle
        entry={deps.get('sky.showOther')}
        onFix={fix}
        dataField="sky.showOther"
        label={t('settings.sensors.showOtherReading')}
        hint={t('settings.sensors.showOtherReadingHint')}
        checked={choice.showOther}
        onChange={(v) => update(['sky', 'showOther'], v)}
      />
      {!choice.enabled[choice.authoritative] && (
        <Note
          tone="warn"
          action={
            choice.enabled[other]
              ? {
                  label: t('settings.sensors.useSkySensor', { name: skySensorName(other) }),
                  onClick: () => update(['sky', 'authoritative'], other),
                }
              : undefined
          }
        >
          {t('settings.sensors.skySensorOffNoReading', { name: skySensorName(choice.authoritative) })}
        </Note>
      )}
    </>
  );
};

const OTHER_SENSORS = [
  ['infrared', 'irSky'],
  ['environment', 'environment'],
] as const;

// Which sensors answered at boot, and the I2C buses.
export const SkySensorsCard: FunctionalComponent<SettingsTabProps> = (props) => {
  const { config, update, error, hw } = props;
  return (
    <SettingsCard id="sky-sensors" title={t('settings.sensors.skySensors')} hint={t('settings.sensors.detectedAtBootRestartAfter')}>
      <div>
        <SkyRoleRows {...props} />
        {OTHER_SENSORS.map(([sensor, key]) => (
          <SensorRow key={sensor} name={sensorName(sensor)} model={SENSOR_MODEL[sensor]}>
            {detected(hw[key])}
          </SensorRow>
        ))}
      </div>
      <div class="form-grid">
        <Field label={t('settings.sensors.readEvery')} error={error('sensorInterval')}>
          <NumberInput
            dataField="sensorInterval"
            integer
            min={100}
            max={3600000}
            step={100}
            unit="ms"
            value={config.sensor.readIntervalMs}
            onChange={(v) => update(['sensor', 'readIntervalMs'], Math.max(100, v || 5000))}
          />
        </Field>
        <Field label={t('settings.sensors.i2cSda')} error={error('i2cSDA') ?? error('i2cPins')}>
          <NumberInput dataField="i2cSDA" integer value={config.sensor.i2cSDA} onChange={(v) => update(['sensor', 'i2cSDA'], v || 21)} />
        </Field>
        <Field label={t('settings.sensors.i2cScl')} error={error('i2cSCL')}>
          <NumberInput dataField="i2cSCL" integer value={config.sensor.i2cSCL} onChange={(v) => update(['sensor', 'i2cSCL'], v || 22)} />
        </Field>
        <Field label={t('settings.sensors.i2cSpeed')} error={error('i2cFrequency')} hint={t('settings.sensors.lowerItForLongCables')}>
          <SelectInput
            dataField="i2cFrequency"
            value={String(config.sensor.i2cFrequency)}
            options={[
              { value: '10000', label: t('settings.sensors.10Khz') },
              { value: '50000', label: t('settings.sensors.50Khz') },
              { value: '100000', label: t('settings.sensors.100Khz') },
              { value: '400000', label: t('settings.sensors.400Khz') },
            ]}
            onChange={(v) => update(['sensor', 'i2cFrequency'], Number(v))}
          />
        </Field>
        <I2c2Field {...props} pin="i2c2SDA" label={t('settings.sensors.i2c2Sda')} />
        <I2c2Field {...props} pin="i2c2SCL" label={t('settings.sensors.i2c2Scl')} />
      </div>
      {error('sky.sqmlEnabled') && <Note tone="bad">{error('sky.sqmlEnabled')}</Note>}
      <SkyChoiceFields {...props} />
    </SettingsCard>
  );
};

// Labelled: shown in this browser's time zone, not the device's (spec 005 FR-007).
const calibratedAtText = (darkCalibratedAt: number) =>
  darkCalibratedAt >= CLOCK_VALID ? formatDateTime(new Date(darkCalibratedAt * 1000)) + t('settings.alerts.thisBrowserSTime') : null;

const darkOffsetText = (darkVisibleOffset: number, calibratedAt: string | null) =>
  darkVisibleOffset > 0
    ? t('settings.sensors.darkOffsetCounts', { value: formatNumber(darkVisibleOffset, 2) }) + (calibratedAt ? ` · ${calibratedAt}` : '')
    : t('settings.sensors.notCalibrated');

// The averaging window's progress, from /api/status (per sensor since spec 030).
interface SampleProgress {
  sampleCount?: number;
  windowSamples?: number;
  nightMode?: boolean;
}

const LightSamples: FunctionalComponent<{ light: SampleProgress }> = ({ light }) => {
  const count = light.sampleCount ?? 0;
  return (
    <div class="reading-row">
      <span class="reading-label">{t('settings.sensors.averagingWindow')}</span>
      <span class="reading-value">
        {light.windowSamples
          ? t('settings.sensors.samplesOfWindow', { count: Math.min(count, light.windowSamples), total: light.windowSamples })
          : t('settings.sensors.sampleCount', { count })}
        {light.nightMode === false ? t('settings.sensors.seeingLight') : ''}
      </span>
    </div>
  );
};

// Each sky sensor's own settings sections and status (spec 030).
const SKY_SECTIONS = {
  sqm: { averaging: 'skyAveraging', calibration: 'skyCalibration', anchor: 'sky' },
  sqml: { averaging: 'skyAveragingL', calibration: 'skyCalibrationL', anchor: 'sky-sqml' },
} as const;

// One sensor's averaging, SQM offset and TSL2591 dark calibration. Titled
// with the sensor's name when the SQM-L is on, else "Sky quality" as before.
export const SkyQualityCard: FunctionalComponent<SettingsTabProps & { role?: SkyRole }> = ({
  config,
  update,
  updateMany,
  applyStored,
  error,
  hw,
  status,
  deps,
  fix,
  role = 'sqm',
}) => {
  const { calibrating, calibrationResult, calibrateDark } = useDarkCalibration(applyStored, role);
  const { averaging: averagingKey, calibration: calibrationKey, anchor } = SKY_SECTIONS[role];
  const averaging = { ...defaultSkyAveraging, ...config[averagingKey] };
  const calibration = { ...defaultSkyCalibration, ...config[calibrationKey] };
  const own = status?.skySensors?.[role];
  const light = own ?? (role === 'sqm' ? status?.diagnostics?.light : undefined);
  const found = own ? own.detected : role === 'sqm' ? hw.skyLight.detected : null;
  const calibratedAt = calibratedAtText(calibration.darkCalibratedAt);
  const title = skyChoice(config).enabled.sqml ? skySensorName(role) : t('settings.sensors.skyQuality');
  return (
    <SettingsCard id={anchor} title={title} hint={t('settings.sensors.howTheLightSensorS')}>
      <div class="form-grid">
        <Field
          label={t('settings.sensors.averagingWindow')}
          error={error(`${averagingKey}.windowSeconds`)}
          hint={t('settings.sensors.sqmIsTheAverageOver')}
        >
          <NumberInput
            dataField={`${averagingKey}.windowSeconds`}
            integer
            min={10}
            max={300}
            unit="s"
            value={averaging.windowSeconds}
            onChange={(v) => update([averagingKey, 'windowSeconds'], v)}
          />
        </Field>
      </div>
      <DepToggle
        entry={deps.get(`${calibrationKey}.enabled`)}
        onFix={fix}
        label={t('settings.sensors.applySqmOffset')}
        hint={t('settings.sensors.addedToEverySqmReading')}
        checked={calibration.enabled}
        onChange={(v) => update([calibrationKey, 'enabled'], v)}
      />
      {calibration.enabled && (
        <div class="form-grid indent">
          <Field label={t('settings.sensors.sqmOffset')} error={error(`${calibrationKey}.sqmOffset`)} hint={t('settings.sensors.5To5')}>
            <NumberInput
              dataField={`${calibrationKey}.sqmOffset`}
              min={-5}
              max={5}
              step={0.01}
              unit="mag/arcsec²"
              value={calibration.sqmOffset}
              onChange={(v) => update([calibrationKey, 'sqmOffset'], v)}
            />
          </Field>
        </div>
      )}
      <Group title={t('settings.sensors.darkCalibration')}>
        <div class="reading-row">
          <span class="reading-label">{t('settings.sensors.darkOffset')}</span>
          <span class="reading-value">{darkOffsetText(calibration.darkVisibleOffset, calibratedAt)}</span>
        </div>
        {light && <LightSamples light={light} />}
        <p class="note note-muted">{t('settings.sensors.coverTheSensorCompletelyCap')}</p>
        <div class="btn-row">
          <ActionButton
            onClick={() => void calibrateDark()}
            busy={calibrating}
            busyLabel={t('common.calibrating')}
            disabled={found === false}
          >
            {t('settings.sensors.calibrateDark')}
          </ActionButton>
          {calibration.darkVisibleOffset > 0 && (
            <ActionButton
              onClick={() =>
                updateMany([
                  [[calibrationKey, 'darkVisibleOffset'], 0],
                  [[calibrationKey, 'darkSampleCount'], 0],
                  [[calibrationKey, 'darkCalibratedAt'], 0],
                ])
              }
            >
              {t('settings.sensors.clear')}
            </ActionButton>
          )}
        </div>
        <ResultNote result={calibrationResult} />
      </Group>
    </SettingsCard>
  );
};

/** One Sky quality card per enabled sky sensor (spec 030 FR-013). */
export const SkyQualityCards: FunctionalComponent<SettingsTabProps> = (props) => {
  const choice = skyChoice(props.config);
  return (
    <>
      {choice.enabled.sqm && <SkyQualityCard {...props} role="sqm" />}
      {choice.enabled.sqml && <SkyQualityCard {...props} role="sqml" />}
    </>
  );
};

// The IR sky-minus-ambient thresholds.
export const CloudDetectionCard: FunctionalComponent<SettingsTabProps> = ({ config, update, error, hw }) => (
  <SettingsCard
    title={t('settings.sensors.cloudDetection')}
    hint={t('settings.sensors.howTheIrSkyMinus')}
    badge={hw.irSky.detected === false ? <StatusBadge tone="bad" label={t('settings.sensors.mlx90614NotDetected')} /> : undefined}
  >
    <div class="form-grid">
      <Field
        label={t('settings.sensors.clearBelow')}
        hint={t('settings.sensors.default130')}
        error={error('cloudDetection.clearSkyThreshold')}
      >
        <NumberInput
          dataField="cloudDetection.clearSkyThreshold"
          min={-30}
          max={0}
          step={0.1}
          unit="°C"
          value={config.cloudDetection.clearSkyThreshold}
          onChange={(v) => update(['cloudDetection', 'clearSkyThreshold'], v)}
        />
      </Field>
      <Field
        label={t('settings.sensors.overcastAbove')}
        hint={t('settings.sensors.default30')}
        error={error('cloudDetection.cloudyThreshold')}
      >
        <NumberInput
          min={-20}
          max={10}
          step={0.1}
          unit="°C"
          value={config.cloudDetection.cloudyThreshold}
          onChange={(v) => update(['cloudDetection', 'cloudyThreshold'], v)}
        />
      </Field>
      <Field
        label={t('settings.sensors.humidityCorrection')}
        hint={t('settings.sensors.aagCloudwatcherK1Default0')}
        error={error('cloudDetection.humidityCorrection')}
      >
        <NumberInput
          min={0}
          max={2}
          step={0.01}
          value={config.cloudDetection.humidityCorrection}
          onChange={(v) => update(['cloudDetection', 'humidityCorrection'], v)}
        />
      </Field>
    </div>
  </SettingsCard>
);
