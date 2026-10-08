import { ComponentChildren, FunctionalComponent } from 'preact';
import { route } from 'preact-router';
import { Note } from '../../components/ui';
import { differential, type SensorId } from '../conditions';
import { demoDevice } from '../device';
import { Check, DraftNumber, NumberField } from './NumberField';

// What each simulated sensor reports, grouped by sensor (spec 019 US1).

const Group: FunctionalComponent<{ title: string; open?: boolean; children: ComponentChildren }> = ({ title, open, children }) => (
  <details class="demo-group" open={open}>
    <summary>{title}</summary>
    <div class="demo-group-body">{children}</div>
  </details>
);

const NotResponding: FunctionalComponent<{ sensor: SensorId; name: string; disabled?: boolean }> = ({ sensor, name, disabled }) => (
  <Check label={`${name} not responding`} checked={demoDevice.conditions.faults[sensor]} disabled={disabled} onChange={(on) => demoDevice.setFault(sensor, on)} />
);

// Controls for a sensor switched off in the device's settings (FR-016).
const SwitchedOff: FunctionalComponent<{ name: string; tab: string }> = ({ name, tab }) => (
  <Note action={{ label: 'Settings', onClick: () => route(`/settings?tab=${tab}`) }}>The {name} is switched off in the device's settings.</Note>
);

const SkyGroup: FunctionalComponent = () => {
  const c = demoDevice.conditions;
  return (
    <Group title="Sky and light (MLX90614, TSL2591)" open>
      <div class="demo-grid">
        <NumberField field="ir.sky" value={c.ir.sky} onCommit={(v) => demoDevice.setInput('ir.sky', v)} />
        <NumberField field="ir.ambient" value={c.ir.ambient} onCommit={(v) => demoDevice.setInput('ir.ambient', v)} />
        <DraftNumber
          id="demo-input-differential"
          label="Sky minus IR sensor"
          unit="°C"
          valueText={differential(c).toFixed(1)}
          onCommit={(v) => demoDevice.setDifferential(v)}
        />
        <NumberField field="light.lux" value={c.light.lux} onCommit={(v) => demoDevice.setInput('light.lux', v)} />
      </div>
      <Check label="Light follows the sun" checked={c.light.mode === 'sun'} onChange={(on) => (on ? demoDevice.followSun() : demoDevice.setInput('light.lux', c.light.lux))} />
      <NotResponding sensor="infrared" name="IR sensor" />
      <NotResponding sensor="light" name="Light sensor" />
    </Group>
  );
};

const AirGroup: FunctionalComponent = () => {
  const c = demoDevice.conditions;
  return (
    <Group title="Air (BME280)">
      <div class="demo-grid">
        <NumberField field="air.temperature" value={c.air.temperature} onCommit={(v) => demoDevice.setInput('air.temperature', v)} />
        <NumberField field="air.humidity" value={c.air.humidity} onCommit={(v) => demoDevice.setInput('air.humidity', v)} />
        <NumberField field="air.pressure" value={c.air.pressure} onCommit={(v) => demoDevice.setInput('air.pressure', v)} />
      </div>
      <NotResponding sensor="environment" name="BME280" />
    </Group>
  );
};

const RainGroup: FunctionalComponent = () => {
  const c = demoDevice.conditions;
  const off = !demoDevice.rainEnabled;
  return (
    <Group title="Rain (RG-15)">
      {off && <SwitchedOff name="rain sensor" tab="sensors" />}
      <div class="demo-grid">
        <NumberField field="rain.rate" value={c.rain.rate} disabled={off} onCommit={(v) => demoDevice.setInput('rain.rate', v)} />
      </div>
      <Check label="Lens fault" checked={c.rain.lensFault} disabled={off} onChange={(on) => demoDevice.setLensFault(on)} />
      <NotResponding sensor="rain" name="Rain sensor" disabled={off} />
    </Group>
  );
};

const WindGroup: FunctionalComponent = () => {
  const c = demoDevice.conditions;
  const off = !demoDevice.windEnabled;
  return (
    <Group title="Wind">
      {off && <SwitchedOff name="anemometer" tab="sensors" />}
      <div class="demo-grid">
        <NumberField field="wind.speed" value={c.wind.speed} disabled={off} onCommit={(v) => demoDevice.setInput('wind.speed', v)} />
        <NumberField field="wind.gust" value={c.wind.gust} disabled={off} onCommit={(v) => demoDevice.setInput('wind.gust', v)} />
        <NumberField field="wind.direction" value={c.wind.direction} disabled={off} onCommit={(v) => demoDevice.setInput('wind.direction', v)} />
      </div>
      <NotResponding sensor="wind" name="Anemometer" disabled={off} />
    </Group>
  );
};

const GpsGroup: FunctionalComponent = () => {
  const off = !demoDevice.gpsEnabled;
  return (
    <Group title="GPS">
      {off && <SwitchedOff name="GPS" tab="time" />}
      <Check label="GPS has a fix" checked={demoDevice.conditions.gps.fix} disabled={off} onChange={(on) => demoDevice.setGpsFix(on)} />
      <p class="demo-hint">The GPS reports the device's location (Time and place, below).</p>
    </Group>
  );
};

const SensorInputs: FunctionalComponent = () => (
  <section class="demo-section" aria-label="Sensor readings">
    <h3>Sensor readings</h3>
    <SkyGroup />
    <AirGroup />
    <RainGroup />
    <WindGroup />
    <GpsGroup />
    <Check label="Hold steady (no natural variation)" checked={demoDevice.conditions.steady} onChange={(on) => demoDevice.setSteady(on)} />
  </section>
);

export default SensorInputs;
