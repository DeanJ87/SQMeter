import { FunctionalComponent } from 'preact';
import { route } from 'preact-router';
import { useState } from 'preact/hooks';
import { Button, Note } from '../../components/ui';
import { demoDevice } from '../device';
import { formatOffset, fromLocal, toLocalParts, zoneName, type LocalParts } from '../posixTz';
import { LOCATION_PRESETS, presetAt, TIME_PRESETS } from '../presets';
import { Check } from './NumberField';
import { formatCoordinates, formatDateTime } from '../../i18n/format';

// The device's date, time and place (spec 019 US4): shown in the device's
// own time zone, set exactly or from presets.

type Message = { tone: 'ok' | 'warn'; text: string } | null;
type Report = (message: Message) => void;

const pad = (n: number) => String(n).padStart(2, '0');

const formatPlace = (latitude: number, longitude: number) => formatCoordinates(latitude, longitude, 2, true);

// Local wall time, formatted without the browser's own zone getting involved.
const formatLocal = (local: LocalParts) =>
  formatDateTime(new Date(Date.UTC(local.year, local.month - 1, local.day, local.hours, local.minutes)), {
    timeZone: 'UTC',
    dateStyle: 'medium',
    timeStyle: 'short',
  });

const ClockAndPlace: FunctionalComponent<{ tz: string; ms: number; local: LocalParts }> = ({ tz, ms, local }) => {
  const place = demoDevice.place;
  const preset = presetAt(place);
  return (
    <p class="demo-clock">
      Device time <span class="mono">{formatLocal(local)}</span> {zoneName(tz, ms)} ({formatOffset(local.offset)})
      <br />
      Location <span class="mono">{formatPlace(place.latitude, place.longitude)}</span>
      {preset ? ` · ${preset.label}` : ''}
    </p>
  );
};

const ExactTime: FunctionalComponent<{ tz: string; local: LocalParts; report: Report }> = ({ tz, local, report }) => {
  const setExact = (value: string) => {
    const match = value.match(/^(\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2})$/);
    if (!match) {
      report({ tone: 'warn', text: "That isn't a date and time the device can use." });
      return;
    }
    const [year, month, day, hours, minutes] = match.slice(1).map(Number);
    demoDevice.setClock(fromLocal(tz, { year, month, day, hours, minutes }));
    report(null);
  };
  return (
    <div class="field">
      <label class="field-label" for="demo-clock-input">
        Device date and time (device's time zone)
      </label>
      <input
        id="demo-clock-input"
        class="input"
        type="datetime-local"
        value={`${local.year}-${pad(local.month)}-${pad(local.day)}T${pad(local.hours)}:${pad(local.minutes)}`}
        onChange={(e) => setExact((e.target as HTMLInputElement).value)}
      />
    </div>
  );
};

const Presets: FunctionalComponent<{ report: Report }> = ({ report }) => {
  const current = presetAt(demoDevice.place);
  return (
    <>
      <div class="demo-buttons" aria-label="Time presets">
        {TIME_PRESETS.map((p) => (
          <Button
            key={p.id}
            small
            onClick={() => {
              const result = demoDevice.applyTimePreset(p.id);
              report(result.ok ? null : { tone: 'warn', text: `${p.label}: ${result.reason}` });
            }}
          >
            {p.label}
          </Button>
        ))}
      </div>
      <div class="demo-buttons" aria-label="Places">
        {LOCATION_PRESETS.map((p) => (
          <Button
            key={p.id}
            small
            variant={current?.id === p.id ? 'primary' : 'default'}
            onClick={() => {
              const reply = demoDevice.applyLocationPreset(p.id);
              report(reply.status === 200 ? null : { tone: 'warn', text: JSON.parse(reply.body).error ?? 'Not saved' });
            }}
          >
            {p.label}
          </Button>
        ))}
      </div>
    </>
  );
};

const TimePlace: FunctionalComponent = () => {
  const [message, setMessage] = useState<Message>(null);
  const tz = demoDevice.timezone;
  const ms = demoDevice.now.getTime();
  const local = toLocalParts(tz, ms);

  return (
    <section class="demo-section" aria-label="Time and place">
      <h3>Time and place</h3>
      <ClockAndPlace tz={tz} ms={ms} local={local} />
      <ExactTime tz={tz} local={local} report={setMessage} />
      <Presets report={setMessage} />
      {message && <Note tone={message.tone}>{message.text}</Note>}
      <Note action={{ label: 'Time & Location', onClick: () => route('/settings?tab=time') }}>
        Exact coordinates and the time zone are in Settings.
      </Note>
      <Check
        label="Run the device clock 10× faster (sun, delays, cooldowns)"
        checked={demoDevice.timeMultiplier === 10}
        onChange={(on) => demoDevice.setTimeMultiplier(on ? 10 : 1)}
      />
    </section>
  );
};

export default TimePlace;
