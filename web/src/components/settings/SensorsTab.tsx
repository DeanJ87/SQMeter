import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import type { SettingsTabProps } from './context';
import { defaultRainConfig, defaultWindConfig } from './defaults';
import type { SensorAvailability } from './hardware';
import {
  ActionButton,
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

const ANEMOMETER_PRESETS = [
  { value: '2.4', label: 'Misol / Argent / SparkFun (2.4 km/h per Hz)' },
  { value: '3.621', label: 'Davis 6410 (3.621 km/h per Hz)' },
];

const detectionBadge = (sensor: SensorAvailability, labels = { ok: 'Detected', bad: 'Not detected' }) => {
  if (!sensor.enabled) return <StatusBadge tone="off" label="Off" />;
  if (sensor.detected === null) return undefined;
  return <StatusBadge tone={sensor.detected ? 'ok' : 'bad'} label={sensor.detected ? labels.ok : labels.bad} />;
};

// Sensor switched on in the form but the device hasn't been saved/restarted with it yet.
const pendingNote = (sensor: SensorAvailability) =>
  sensor.enabled && sensor.savedEnabled === false ? <Requires>Save to start using it.</Requires> : null;

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
        ? { type: 'success', text: result.raw_response ? `RG-15 replied: ${result.raw_response}` : 'RG-15 replied' }
        : { type: 'error', text: result.error || result.hint || 'No valid reply from the RG-15' });
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
  const pin = (n: number) => `GPIO ${n}`;

  return (
    <>
      <SettingsCard
        title="Sky & environment sensors"
        description="The built-in I2C sensors. They're detected at boot - after fixing wiring, restart the device."
      >
        <div class="grid grid-cols-1 sm:grid-cols-3 gap-3">
          {[
            { name: 'TSL2591 light (SQM)', sensor: hw.skyLight },
            { name: 'MLX90614 IR (clouds)', sensor: hw.irSky },
            { name: 'BME280 (temp / humidity / pressure)', sensor: hw.environment },
          ].map(({ name, sensor }) => (
            <div key={name} class="flex flex-col gap-1.5 p-3 rounded-lg bg-gray-900/50 border border-gray-700">
              <span class="text-sm text-gray-200">{name}</span>
              {detectionBadge(sensor) ?? <StatusBadge tone="off" label="Checking..." />}
            </div>
          ))}
        </div>
        <div class="grid grid-cols-1 md:grid-cols-4 gap-4">
          <Field label="Read every (ms)" error={error('sensorInterval')} hint="100 ms - 1 h">
            <NumberInput
              dataField="sensorInterval"
              integer
              min={100}
              max={3600000}
              step={100}
              value={config.sensor.readIntervalMs}
              onChange={(v) => update(['sensor', 'readIntervalMs'], Math.max(100, v || 5000))}
            />
          </Field>
          <Field label="I2C SDA pin" error={error('i2cSDA') ?? error('i2cPins')}>
            <NumberInput dataField="i2cSDA" integer value={config.sensor.i2cSDA} onChange={(v) => update(['sensor', 'i2cSDA'], v || 21)} />
          </Field>
          <Field label="I2C SCL pin" error={error('i2cSCL')}>
            <NumberInput dataField="i2cSCL" integer value={config.sensor.i2cSCL} onChange={(v) => update(['sensor', 'i2cSCL'], v || 22)} />
          </Field>
          <Field label="I2C speed" error={error('i2cFrequency')}>
            <SelectInput
              dataField="i2cFrequency"
              value={String(config.sensor.i2cFrequency)}
              options={[
                { value: '10000', label: '10 kHz (long cables)' },
                { value: '50000', label: '50 kHz' },
                { value: '100000', label: '100 kHz (standard)' },
                { value: '400000', label: '400 kHz (fast)' },
              ]}
              onChange={(v) => update(['sensor', 'i2cFrequency'], parseInt(v, 10))}
            />
          </Field>
        </div>
      </SettingsCard>

      <SettingsCard
        title="Cloud detection"
        description="How the IR sky-minus-ambient temperature is turned into cloud cover. More negative = colder sky = clearer."
        badge={detectionBadge(hw.irSky, { ok: 'MLX90614 detected', bad: 'MLX90614 not detected' })}
      >
        {hw.irSky.detected === false && (
          <Requires tone="warn">Cloud cover can't be measured without the MLX90614 - these values have no effect until it's detected.</Requires>
        )}
        <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
          <Field label="Clear below (°C)" hint="Default -13.0">
            <NumberInput min={-30} max={0} step={0.1} value={config.cloudDetection.clearSkyThreshold} onChange={(v) => update(['cloudDetection', 'clearSkyThreshold'], v)} />
          </Field>
          <Field label="Overcast above (°C)" hint="Default -3.0" error={error('cloudDetection.clearSkyThreshold')}>
            <NumberInput min={-20} max={10} step={0.1} value={config.cloudDetection.cloudyThreshold} onChange={(v) => update(['cloudDetection', 'cloudyThreshold'], v)} />
          </Field>
          <Field
            label="Humidity correction (k1)"
            hint={hw.environment.detected === false ? 'Needs the BME280 - a fixed 53% humidity is assumed without it.' : 'AAG CloudWatcher formula, default 0.75'}
          >
            <NumberInput min={0} max={2} step={0.01} value={config.cloudDetection.humidityCorrection} onChange={(v) => update(['cloudDetection', 'humidityCorrection'], v)} />
          </Field>
        </div>
      </SettingsCard>

      <SettingsCard
        id="rain"
        title="Rain sensor (Hydreon RG-15)"
        description="Detects rain for the SafetyMonitor, alerts and ObservingConditions rain rate."
        badge={detectionBadge(hw.rain, { ok: 'Responding', bad: 'Not responding' })}
      >
        <Toggle label="Use an RG-15 rain sensor" checked={rain.enabled} onChange={(v) => update(['rain', 'enabled'], v)} />
        {pendingNote(hw.rain)}
        {rain.enabled && hw.rain.detected === false && (
          <Requires tone="warn">
            The RG-15 isn't answering. Check Serial OUT → {pin(rain.rxPin)}, Serial IN → {pin(rain.txPin)}, a common ground and
            the baud rate, then use the test below.
          </Requires>
        )}
        {rain.enabled && (
          <>
            <Group title="Connection">
              <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
                <Field label="RX pin (from RG-15 OUT)" error={error('rain.rxPin')}>
                  <NumberInput dataField="rain.rxPin" integer min={0} max={39} value={rain.rxPin} onChange={(v) => update(['rain', 'rxPin'], v)} />
                </Field>
                <Field label="TX pin (to RG-15 IN)" error={error('rain.txPin')}>
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
              </div>
              <div class="flex flex-wrap items-center gap-3">
                <ActionButton
                  onClick={testRain}
                  busy={testingRain}
                  busyLabel="Testing..."
                  disabled={hw.rain.savedEnabled === false || dirty}
                  title={dirty ? 'Save first - the test uses the saved settings' : undefined}
                >
                  Test communication
                </ActionButton>
                {dirty && !rainResult && <span class="text-xs text-gray-500">Save first - the test uses the saved settings.</span>}
                <ResultNote result={rainResult} />
              </div>
            </Group>

            <Group title="Measurement">
              <div class="grid grid-cols-1 md:grid-cols-4 gap-4">
                <Field label="Poll every (seconds)" error={error('rain.pollIntervalMs')}>
                  <NumberInput
                    dataField="rain.pollIntervalMs"
                    integer
                    min={1}
                    max={3600}
                    value={Math.round((rain.pollIntervalMs ?? 5000) / 1000)}
                    onChange={(v) => update(['rain', 'pollIntervalMs'], Math.max(1, v || 5) * 1000)}
                  />
                </Field>
                <Field
                  label="Rain clear delay (minutes)"
                  error={error('rain.rainClearDelayMs')}
                  hint="Still counts as raining this long after the last drop."
                >
                  <NumberInput
                    dataField="rain.rainClearDelayMs"
                    integer
                    min={1}
                    max={1440}
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
                      { value: 'switch', label: 'Set by DIP switch' },
                    ]}
                    onChange={(v) => update(['rain', 'resolution'], v)}
                  />
                </Field>
                <Field label="Units">
                  <SelectInput
                    value={rain.units ?? 'metric'}
                    options={[
                      { value: 'metric', label: 'Metric (mm)' },
                      { value: 'imperial', label: 'Imperial (in)' },
                      { value: 'switch', label: 'Set by DIP switch' },
                    ]}
                    onChange={(v) => update(['rain', 'units'], v)}
                  />
                </Field>
              </div>
              <Toggle
                label="Reset the daily rain total each day"
                checked={rain.dailyResetEnabled ?? false}
                onChange={(v) => update(['rain', 'dailyResetEnabled'], v)}
              />
              {rain.dailyResetEnabled && (
                <Field label="Reset at" class="ml-7 max-w-xs" error={error('rain.dailyResetHour') ?? error('rain.dailyResetMinute')}>
                  <input
                    data-field="rain.dailyResetHour"
                    type="time"
                    class="w-full px-3 py-2 bg-gray-900/60 border border-gray-600 rounded-lg text-white text-sm"
                    value={`${String(rain.dailyResetHour ?? 0).padStart(2, '0')}:${String(rain.dailyResetMinute ?? 0).padStart(2, '0')}`}
                    onChange={(e) => setDailyReset((e.target as HTMLInputElement).value)}
                  />
                </Field>
              )}
              <Toggle
                label="Log raw RG-15 serial traffic"
                checked={rain.debugUart}
                onChange={(v) => update(['rain', 'debugUart'], v)}
                hint="For troubleshooting only - noisy."
              />
            </Group>
          </>
        )}
      </SettingsCard>

      <SettingsCard
        id="wind"
        title="Wind (anemometer)"
        description="Reed-switch cup anemometer and optional wind vane: wind speed, gust and direction for ObservingConditions and the wind safety limits."
        badge={detectionBadge(hw.wind, { ok: 'Running', bad: 'Not reporting' })}
      >
        <Toggle label="Use an anemometer" checked={wind.enabled} onChange={(v) => update(['wind', 'enabled'], v)} />
        {pendingNote(hw.wind)}
        {wind.enabled && (
          <>
            <Group title="Anemometer">
              <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
                <Field label="Pin" error={error('wind.speedPin')} hint="Switch to GND; internal pull-up.">
                  <NumberInput dataField="wind.speedPin" integer value={wind.speedPin} onChange={(v) => update(['wind', 'speedPin'], v)} />
                </Field>
                <Field label="Type">
                  <SelectInput
                    value={windPreset}
                    options={[...ANEMOMETER_PRESETS, { value: 'custom', label: 'Other / custom' }]}
                    onChange={(v) => v !== 'custom' && update(['wind', 'kmhPerHz'], parseFloat(v))}
                  />
                </Field>
                <Field label="km/h per Hz" error={error('wind.kmhPerHz')} hint="Speed for one closure per second.">
                  <NumberInput ariaLabel="km/h per Hz" step={0.001} min={0.001} max={20} value={wind.kmhPerHz} onChange={(v) => update(['wind', 'kmhPerHz'], v)} />
                </Field>
              </div>
            </Group>

            <Group
              title="Wind vane"
              aside={wind.directionEnabled && hw.windVane.detected === false ? <StatusBadge tone="bad" label="Vane fault" /> : undefined}
            >
              <Toggle label="Use a wind vane" checked={wind.directionEnabled} onChange={(v) => update(['wind', 'directionEnabled'], v)} />
              {wind.directionEnabled && (
                <div class="grid grid-cols-1 md:grid-cols-3 gap-4 ml-7">
                  <Field label="Pin (GPIO 32-39)" error={error('wind.directionPin')} hint="ADC1 only - ADC2 can't be read with WiFi on.">
                    <NumberInput dataField="wind.directionPin" integer min={32} max={39} value={wind.directionPin} onChange={(v) => update(['wind', 'directionPin'], v)} />
                  </Field>
                  <Field label="Pull-up to 3.3 V (Ω)" error={error('wind.vanePullupOhms')} hint="Default 10 kΩ">
                    <NumberInput step={100} value={wind.vanePullupOhms} onChange={(v) => update(['wind', 'vanePullupOhms'], v)} />
                  </Field>
                  <Field label="North offset (°)" error={error('wind.directionOffsetDeg')} hint="If the vane isn't mounted pointing north.">
                    <NumberInput step={1} value={wind.directionOffsetDeg} onChange={(v) => update(['wind', 'directionOffsetDeg'], v)} />
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
