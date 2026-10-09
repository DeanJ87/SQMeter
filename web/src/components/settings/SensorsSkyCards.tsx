import { FunctionalComponent } from 'preact';
import type { SettingsTabProps } from './context';
import { defaultSkyAveraging, defaultSkyCalibration } from './defaults';
import type { SensorAvailability } from './hardware';
import { useDarkCalibration } from '../../hooks/useSensorActions';
import { ActionButton, DepToggle, Field, Group, NumberInput, ResultNote, SelectInput, SettingsCard, StatusBadge } from './controls';
import { t } from '../../i18n';
import { formatDateTime, formatNumber } from '../../i18n/format';

const CLOCK_VALID = 1704067200; // calibration times below this are uptime, not dates

const detected = (sensor: SensorAvailability) => {
  if (sensor.detected === null) return undefined;
  return sensor.detected ? <StatusBadge tone="ok" label="OK" /> : <StatusBadge tone="bad" label={t('settings.sensors.notDetected')} />;
};

// Which sensors answered at boot, and the I2C bus.
export const SkySensorsCard: FunctionalComponent<SettingsTabProps> = ({ config, update, error, hw }) => (
  <SettingsCard id="sky-sensors" title={t('settings.sensors.skySensors')} hint={t('settings.sensors.detectedAtBootRestartAfter')}>
    <div>
      {[
        [t('settings.sensors.tsl2591Light'), hw.skyLight],
        [t('settings.sensors.mlx90614Ir'), hw.irSky],
        [t('settings.sensors.bme280Environment'), hw.environment],
      ].map(([label, sensor]) => (
        <div class="reading-row" key={label as string}>
          <span class="reading-label">{label as string}</span>
          {detected(sensor as SensorAvailability) ?? <span class="note">{t('common.checking')}</span>}
        </div>
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
    </div>
  </SettingsCard>
);

// Labelled: shown in this browser's time zone, not the device's (spec 005 FR-007).
const calibratedAtText = (darkCalibratedAt: number) =>
  darkCalibratedAt >= CLOCK_VALID ? formatDateTime(new Date(darkCalibratedAt * 1000)) + t('settings.alerts.thisBrowserSTime') : null;

const darkOffsetText = (darkVisibleOffset: number, calibratedAt: string | null) =>
  darkVisibleOffset > 0
    ? t('settings.sensors.darkOffsetCounts', { value: formatNumber(darkVisibleOffset, 2) }) + (calibratedAt ? ` · ${calibratedAt}` : '')
    : t('settings.sensors.notCalibrated');

type LightDiagnostics = NonNullable<NonNullable<NonNullable<SettingsTabProps['status']>['diagnostics']>['light']>;

const LightSamples: FunctionalComponent<{ light: LightDiagnostics }> = ({ light }) => (
  <div class="reading-row">
    <span class="reading-label">{t('settings.sensors.averagingWindow')}</span>
    <span class="reading-value">
      {light.windowSamples
        ? t('settings.sensors.samplesOfWindow', {
            count: Math.min(light.sampleCount, light.windowSamples),
            total: light.windowSamples,
          })
        : t('settings.sensors.sampleCount', { count: light.sampleCount })}
      {light.nightMode === false ? t('settings.sensors.seeingLight') : ''}
    </span>
  </div>
);

// SQM averaging, the SQM offset and the TSL2591 dark calibration.
export const SkyQualityCard: FunctionalComponent<SettingsTabProps> = ({
  config,
  update,
  updateMany,
  applyStored,
  error,
  hw,
  status,
  deps,
  fix,
}) => {
  const { calibrating, calibrationResult, calibrateDark } = useDarkCalibration(applyStored);
  const averaging = { ...defaultSkyAveraging, ...config.skyAveraging };
  const calibration = { ...defaultSkyCalibration, ...config.skyCalibration };
  const light = status?.diagnostics?.light;
  const calibratedAt = calibratedAtText(calibration.darkCalibratedAt);
  return (
    <SettingsCard id="sky" title={t('settings.sensors.skyQuality')} hint={t('settings.sensors.howTheLightSensorS')}>
      <div class="form-grid">
        <Field
          label={t('settings.sensors.averagingWindow')}
          error={error('skyAveraging.windowSeconds')}
          hint={t('settings.sensors.sqmIsTheAverageOver')}
        >
          <NumberInput
            dataField="skyAveraging.windowSeconds"
            integer
            min={10}
            max={300}
            unit="s"
            value={averaging.windowSeconds}
            onChange={(v) => update(['skyAveraging', 'windowSeconds'], v)}
          />
        </Field>
      </div>
      <DepToggle
        entry={deps.get('skyCalibration.enabled')}
        onFix={fix}
        label={t('settings.sensors.applySqmOffset')}
        hint={t('settings.sensors.addedToEverySqmReading')}
        checked={calibration.enabled}
        onChange={(v) => update(['skyCalibration', 'enabled'], v)}
      />
      {calibration.enabled && (
        <div class="form-grid indent">
          <Field label={t('settings.sensors.sqmOffset')} error={error('skyCalibration.sqmOffset')} hint={t('settings.sensors.5To5')}>
            <NumberInput
              dataField="skyCalibration.sqmOffset"
              min={-5}
              max={5}
              step={0.01}
              unit="mag/arcsec²"
              value={calibration.sqmOffset}
              onChange={(v) => update(['skyCalibration', 'sqmOffset'], v)}
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
            disabled={hw.skyLight.detected === false}
          >
            {t('settings.sensors.calibrateDark')}
          </ActionButton>
          {calibration.darkVisibleOffset > 0 && (
            <ActionButton
              onClick={() =>
                updateMany([
                  [['skyCalibration', 'darkVisibleOffset'], 0],
                  [['skyCalibration', 'darkSampleCount'], 0],
                  [['skyCalibration', 'darkCalibratedAt'], 0],
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
