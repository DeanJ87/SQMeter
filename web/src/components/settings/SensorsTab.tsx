import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import type { SettingsTabProps } from './context';
import { defaultRainConfig, defaultSkyAveraging, defaultSkyCalibration, defaultWindConfig } from './defaults';
import type { SensorAvailability } from './hardware';
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

const CLOCK_VALID = 1704067200; // calibration times below this are uptime, not dates

const SensorsTab: FunctionalComponent<SettingsTabProps> = ({
  config,
  update,
  updateMany,
  applyStored,
  error,
  hw,
  status,
  dirty,
  deps,
  fix,
}) => {
  const [calibrating, setCalibrating] = useState(false);
  const [calibrationResult, setCalibrationResult] = useState<{ type: 'success' | 'error'; text: string } | null>(null);
  const averaging = { ...defaultSkyAveraging, ...config.skyAveraging };
  const calibration = { ...defaultSkyCalibration, ...config.skyCalibration };
  const light = status?.diagnostics?.light;

  const calibrateDark = async () => {
    setCalibrating(true);
    setCalibrationResult(null);
    try {
      const response = await fetch('/api/sensors/tsl2591/calibrate-dark', { method: 'POST' });
      const result = await response.json();
      if (response.ok) {
        // The device saved it; keep the form in step so a later Save doesn't undo it.
        applyStored([
          [['skyCalibration', 'darkVisibleOffset'], result.darkVisibleOffset],
          [['skyCalibration', 'darkSampleCount'], result.sampleCount],
          [['skyCalibration', 'darkCalibratedAt'], result.darkCalibratedAt],
        ]);
        setCalibrationResult({ type: 'success', text: t('settings.sensors.darkOffsetSaved') });
      } else {
        setCalibrationResult({ type: 'error', text: result.error || t('settings.sensors.calibrationFailed') });
      }
    } catch {
      setCalibrationResult({ type: 'error', text: t('settings.sensors.couldNotReachTheDevice') });
    } finally {
      setCalibrating(false);
    }
  };

  const calibratedAt = calibration.darkCalibratedAt >= CLOCK_VALID ? new Date(calibration.darkCalibratedAt * 1000).toLocaleString() : null;
  const [testingRain, setTestingRain] = useState(false);
  const [rainResult, setRainResult] = useState<{ type: 'success' | 'error'; text: string } | null>(null);
  const rain = config.rain ?? defaultRainConfig;
  const wind = { ...defaultWindConfig, ...config.wind };

  const testRain = async () => {
    setTestingRain(true);
    setRainResult(null);
    try {
      const response = await fetch('/api/sensors/rg15/test', { method: 'POST' });
      const result = await response.json();
      setRainResult(
        response.ok
          ? {
              type: 'success',
              text: result.rawResponse
                ? t('settings.sensors.repliedWith', { response: result.rawResponse })
                : t('settings.sensors.replied'),
            }
          : { type: 'error', text: result.error || result.hint || t('settings.sensors.noReply') },
      );
    } catch {
      setRainResult({ type: 'error', text: t('settings.sensors.couldNotReachTheDevice') });
    } finally {
      setTestingRain(false);
    }
  };

  const setDailyReset = (value: string) => {
    const [hour, minute] = value.split(':').map((part) => parseInt(part, 10));
    updateMany([
      [['rain', 'dailyResetHour'], Number.isFinite(hour) ? hour : 0],
      [['rain', 'dailyResetMinute'], Number.isFinite(minute) ? minute : 0],
    ]);
  };

  const windPreset = ANEMOMETER_PRESETS.find((p) => Math.abs(parseFloat(p.value) - wind.kmhPerHz) < 0.0005)?.value ?? 'custom';
  const detected = (sensor: SensorAvailability) =>
    sensor.detected === null ? undefined : sensor.detected ? (
      <StatusBadge tone="ok" label="OK" />
    ) : (
      <StatusBadge tone="bad" label={t('settings.sensors.notDetected')} />
    );

  return (
    <>
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
              onChange={(v) => update(['sensor', 'i2cFrequency'], parseInt(v, 10))}
            />
          </Field>
        </div>
      </SettingsCard>

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
            <span class="reading-value">
              {calibration.darkVisibleOffset > 0
                ? `${calibration.darkVisibleOffset.toFixed(2)} counts${calibratedAt ? ` · ${calibratedAt}` : ''}`
                : t('settings.sensors.notCalibrated')}
            </span>
          </div>
          {light && (
            <div class="reading-row">
              <span class="reading-label">{t('settings.sensors.averagingWindow')}</span>
              <span class="reading-value">
                {light.windowSamples
                  ? `${Math.min(light.sampleCount, light.windowSamples)} of ${light.windowSamples} samples`
                  : `${light.sampleCount} samples`}
                {light.nightMode === false ? t('settings.sensors.seeingLight') : ''}
              </span>
            </div>
          )}
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

      <SettingsCard
        title={t('settings.sensors.cloudDetection')}
        hint={t('settings.sensors.howTheIrSkyMinus')}
        badge={hw.irSky.detected === false ? <StatusBadge tone="bad" label={t('settings.sensors.mlx90614NotDetected')} /> : undefined}
      >
        <div class="form-grid">
          <Field label={t('settings.sensors.clearBelow')} hint={t('settings.sensors.default130')}>
            <NumberInput
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
            error={error('cloudDetection.clearSkyThreshold')}
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
          <Field label={t('settings.sensors.humidityCorrection')} hint={t('settings.sensors.aagCloudwatcherK1Default0')}>
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
            <div class="form-grid">
              <Field label={t('settings.sensors.rxPin')} error={error('rain.rxPin')} hint={t('settings.sensors.fromTheRg15S')}>
                <NumberInput
                  dataField="rain.rxPin"
                  integer
                  min={0}
                  max={39}
                  value={rain.rxPin}
                  onChange={(v) => update(['rain', 'rxPin'], v)}
                />
              </Field>
              <Field label={t('settings.sensors.txPin')} error={error('rain.txPin')} hint={t('settings.sensors.toTheRg15S')}>
                <NumberInput
                  dataField="rain.txPin"
                  integer
                  min={0}
                  max={39}
                  value={rain.txPin}
                  onChange={(v) => update(['rain', 'txPin'], v)}
                />
              </Field>
              <Field label={t('settings.sensors.baudRate')} error={error('rain.baudRate')}>
                <SelectInput
                  dataField="rain.baudRate"
                  value={String(rain.baudRate)}
                  options={['2400', '4800', '9600', '19200'].map((b) => ({ value: b, label: b }))}
                  onChange={(v) => update(['rain', 'baudRate'], parseInt(v, 10))}
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
                  onChange={(v) => v !== 'custom' && update(['wind', 'kmhPerHz'], parseFloat(v))}
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
    </>
  );
};

export default SensorsTab;
