import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import type { SettingsTabProps } from './context';
import { defaultRainConfig, defaultWindConfig } from './defaults';
import type { SensorAvailability } from './hardware';
import { ActionButton, Field, Group, NumberInput, Requires, ResultNote, SelectInput, SettingsCard, StatusBadge, Toggle } from './controls';

const ANEMOMETER_PRESETS = [
  { value: '2.4', label: 'Misol / Argent / SparkFun' },
  { value: '3.621', label: 'Davis 6410' },
];

const detectionBadge = (sensor: SensorAvailability, labels = { ok: 'Detected', bad: 'Not detected' }) => {
  if (!sensor.enabled) return undefined;
  if (sensor.detected === null) return undefined;
  return <StatusBadge tone={sensor.detected ? 'ok' : 'bad'} label={sensor.detected ? labels.ok : labels.bad} />;
};

const SensorsTab: FunctionalComponent<SettingsTabProps> = ({ config, update, updateMany, error, hw, dirty }) => {
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
      setRainResult(response.ok && result.ok
        ? { type: 'success', text: result.raw_response ? `Replied: ${result.raw_response}` : 'Replied' }
        : { type: 'error', text: result.error || result.hint || 'No reply' });
    } catch {
      setRainResult({ type: 'error', text: 'Could not reach the device' });
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
    sensor.detected === null ? undefined : sensor.detected ? <StatusBadge tone="ok" label="OK" /> : <StatusBadge tone="bad" label="Not detected" />;

  return (
    <>
      <SettingsCard title="Sky sensors" hint="Detected at boot. Restart after fixing wiring.">
        <div>
          {[
            ['TSL2591 light', hw.skyLight],
            ['MLX90614 IR', hw.irSky],
            ['BME280 environment', hw.environment],
          ].map(([label, sensor]) => (
            <div class="reading-row" key={label as string}>
              <span class="reading-label">{label as string}</span>
              {detected(sensor as SensorAvailability) ?? <span class="note">Checking...</span>}
            </div>
          ))}
        </div>
        <div class="form-grid">
          <Field label="Read every" error={error('sensorInterval')}>
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
          <Field label="I2C SDA" error={error('i2cSDA') ?? error('i2cPins')}>
            <NumberInput dataField="i2cSDA" integer value={config.sensor.i2cSDA} onChange={(v) => update(['sensor', 'i2cSDA'], v || 21)} />
          </Field>
          <Field label="I2C SCL" error={error('i2cSCL')}>
            <NumberInput dataField="i2cSCL" integer value={config.sensor.i2cSCL} onChange={(v) => update(['sensor', 'i2cSCL'], v || 22)} />
          </Field>
          <Field label="I2C speed" error={error('i2cFrequency')} hint="Lower it for long cables.">
            <SelectInput
              dataField="i2cFrequency"
              value={String(config.sensor.i2cFrequency)}
              options={[
                { value: '10000', label: '10 kHz' },
                { value: '50000', label: '50 kHz' },
                { value: '100000', label: '100 kHz' },
                { value: '400000', label: '400 kHz' },
              ]}
              onChange={(v) => update(['sensor', 'i2cFrequency'], parseInt(v, 10))}
            />
          </Field>
        </div>
      </SettingsCard>

      <SettingsCard
        title="Cloud detection"
        hint="How the IR sky-minus-ambient temperature maps to cloud cover. Colder sky = clearer."
        badge={hw.irSky.detected === false ? <StatusBadge tone="bad" label="MLX90614 not detected" /> : undefined}
      >
        <div class="form-grid">
          <Field label="Clear below" hint="Default -13.0">
            <NumberInput min={-30} max={0} step={0.1} unit="°C" value={config.cloudDetection.clearSkyThreshold} onChange={(v) => update(['cloudDetection', 'clearSkyThreshold'], v)} />
          </Field>
          <Field label="Overcast above" hint="Default -3.0" error={error('cloudDetection.clearSkyThreshold')}>
            <NumberInput min={-20} max={10} step={0.1} unit="°C" value={config.cloudDetection.cloudyThreshold} onChange={(v) => update(['cloudDetection', 'cloudyThreshold'], v)} />
          </Field>
          <Field label="Humidity correction" hint="AAG CloudWatcher k1, default 0.75. Without the BME280 a fixed 53% humidity is assumed.">
            <NumberInput min={0} max={2} step={0.01} value={config.cloudDetection.humidityCorrection} onChange={(v) => update(['cloudDetection', 'humidityCorrection'], v)} />
          </Field>
        </div>
      </SettingsCard>

      <SettingsCard id="rain" title="Rain sensor" hint="Hydreon RG-15, on a serial port." badge={detectionBadge(hw.rain, { ok: 'Responding', bad: 'Not responding' })}>
        <Toggle label="RG-15 rain sensor" checked={rain.enabled} onChange={(v) => update(['rain', 'enabled'], v)} />
        {rain.enabled && hw.rain.detected === false && (
          <Requires tone="warn">
            No reply. Check OUT → GPIO {rain.rxPin}, IN → GPIO {rain.txPin}, ground and baud rate.
          </Requires>
        )}
        {rain.enabled && (
          <>
            <div class="form-grid">
              <Field label="RX pin" error={error('rain.rxPin')} hint="From the RG-15's serial OUT.">
                <NumberInput dataField="rain.rxPin" integer min={0} max={39} value={rain.rxPin} onChange={(v) => update(['rain', 'rxPin'], v)} />
              </Field>
              <Field label="TX pin" error={error('rain.txPin')} hint="To the RG-15's serial IN.">
                <NumberInput dataField="rain.txPin" integer min={0} max={39} value={rain.txPin} onChange={(v) => update(['rain', 'txPin'], v)} />
              </Field>
              <Field label="Baud rate" error={error('rain.baudRate')}>
                <SelectInput
                  dataField="rain.baudRate"
                  value={String(rain.baudRate)}
                  options={['2400', '4800', '9600', '19200'].map((b) => ({ value: b, label: b }))}
                  onChange={(v) => update(['rain', 'baudRate'], parseInt(v, 10))}
                />
              </Field>
              <Field label="Poll every" error={error('rain.pollIntervalMs')}>
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
              <Field label="Rain clear delay" error={error('rain.rainClearDelayMs')} hint="Still counts as raining this long after the last drop.">
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
              <Field label="Resolution">
                <SelectInput
                  value={rain.resolution ?? 'switch'}
                  options={[
                    { value: 'high', label: 'High (0.01 mm)' },
                    { value: 'low', label: 'Low (0.2 mm)' },
                    { value: 'switch', label: 'DIP switch' },
                  ]}
                  onChange={(v) => update(['rain', 'resolution'], v)}
                />
              </Field>
              <Field label="Units">
                <SelectInput
                  value={rain.units ?? 'metric'}
                  options={[
                    { value: 'metric', label: 'mm' },
                    { value: 'imperial', label: 'inches' },
                    { value: 'switch', label: 'DIP switch' },
                  ]}
                  onChange={(v) => update(['rain', 'units'], v)}
                />
              </Field>
            </div>
            <Toggle label="Reset the daily total" checked={rain.dailyResetEnabled ?? false} onChange={(v) => update(['rain', 'dailyResetEnabled'], v)} />
            {rain.dailyResetEnabled && (
              <div class="form-grid indent">
                <Field label="At" error={error('rain.dailyResetHour') ?? error('rain.dailyResetMinute')}>
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
            <Toggle label="Log serial traffic" checked={rain.debugUart} onChange={(v) => update(['rain', 'debugUart'], v)} hint="Troubleshooting only." />
            <div class="btn-row">
              <ActionButton
                onClick={testRain}
                busy={testingRain}
                busyLabel="Testing..."
                disabled={hw.rain.savedEnabled === false || dirty}
                title={dirty ? 'Save first' : undefined}
              >
                Test communication
              </ActionButton>
              <ResultNote result={rainResult} />
            </div>
          </>
        )}
      </SettingsCard>

      <SettingsCard id="wind" title="Wind" hint="Reed-switch cup anemometer, optional wind vane." badge={detectionBadge(hw.wind, { ok: 'Running', bad: 'Not reporting' })}>
        <Toggle label="Anemometer" checked={wind.enabled} onChange={(v) => update(['wind', 'enabled'], v)} />
        {wind.enabled && (
          <>
            <div class="form-grid">
              <Field label="Pin" error={error('wind.speedPin')} hint="Switch to GND; internal pull-up.">
                <NumberInput dataField="wind.speedPin" integer value={wind.speedPin} onChange={(v) => update(['wind', 'speedPin'], v)} />
              </Field>
              <Field label="Model">
                <SelectInput
                  value={windPreset}
                  options={[...ANEMOMETER_PRESETS, { value: 'custom', label: 'Other' }]}
                  onChange={(v) => v !== 'custom' && update(['wind', 'kmhPerHz'], parseFloat(v))}
                />
              </Field>
              <Field label="Speed per pulse" error={error('wind.kmhPerHz')} hint="km/h for one closure per second.">
                <NumberInput ariaLabel="km/h per Hz" step={0.001} min={0.001} max={20} unit="km/h·Hz⁻¹" value={wind.kmhPerHz} onChange={(v) => update(['wind', 'kmhPerHz'], v)} />
              </Field>
            </div>
            <Group title="Wind vane" aside={wind.directionEnabled && hw.windVane.detected === false ? <StatusBadge tone="bad" label="Vane fault" /> : undefined}>
              <Toggle label="Wind vane" checked={wind.directionEnabled} onChange={(v) => update(['wind', 'directionEnabled'], v)} />
              {wind.directionEnabled && (
                <div class="form-grid">
                  <Field label="Pin" error={error('wind.directionPin')} hint="GPIO 32-39 only: ADC2 can't be read while WiFi is on.">
                    <NumberInput dataField="wind.directionPin" integer min={32} max={39} value={wind.directionPin} onChange={(v) => update(['wind', 'directionPin'], v)} />
                  </Field>
                  <Field label="Pull-up" error={error('wind.vanePullupOhms')} hint="Resistor from the vane pin to 3.3 V.">
                    <NumberInput step={100} unit="Ω" value={wind.vanePullupOhms} onChange={(v) => update(['wind', 'vanePullupOhms'], v)} />
                  </Field>
                  <Field label="North offset" error={error('wind.directionOffsetDeg')} hint="If the vane isn't mounted pointing north.">
                    <NumberInput step={1} unit="°" value={wind.directionOffsetDeg} onChange={(v) => update(['wind', 'directionOffsetDeg'], v)} />
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
