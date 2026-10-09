import { ComponentChildren, FunctionalComponent } from 'preact';
import { useRef, useState } from 'preact/hooks';
import type { AlertChannelName, AlertEventKey, AlertRecord, AlertSendMode } from '../../types';
import { mergeAlertsConfig } from './defaults';
import { showToast } from '../toast';
import { deviceTime } from '../../lib/deviceTime';
import { describeDarkness } from './darkness';
import type { SettingsTabProps } from './context';
import { useAlertSchedule } from '../../hooks/useAlertSchedule';
import { COMMON_VARS, EVENT_VARS } from './alertVariables';
import {
  PAUSE_HINT,
  SEND_MODE_OPTIONS,
  describeSchedule,
  describeSilentClients,
  minutesToSeconds,
  secondsToMinutes,
} from './alertSchedule';
import { InfoTip, Note } from '../ui';
import type { DepEntry } from '../../lib/settingsDeps';
import {
  ActionButton,
  DepNote,
  DepToggle,
  Field,
  Group,
  NumberInput,
  Requires,
  ResultNote,
  SelectInput,
  SettingsCard,
  StatusBadge,
  TextInput,
  Toggle,
} from './controls';

const LEVEL_OPTIONS = [
  { value: '0', label: 'Off' },
  { value: '1', label: 'Quiet' },
  { value: '2', label: 'Normal' },
  { value: '3', label: 'Urgent' },
  { value: '4', label: 'Wake me' },
];

const LEVEL_HINT =
  'Quiet: no sound. Urgent: breaks through quiet hours. Wake me: repeats until acknowledged (Pushover emergency, ntfy max) and rings paired phones over Bluetooth.';

// Pushover's built-in sounds; a custom one already saved stays selectable.
const PUSHOVER_SOUNDS = [
  'pushover',
  'bike',
  'bugle',
  'cashregister',
  'classical',
  'cosmic',
  'falling',
  'gamelan',
  'incoming',
  'intermission',
  'magic',
  'mechanical',
  'pianobar',
  'siren',
  'spacealarm',
  'tugboat',
  'alien',
  'climb',
  'persistent',
  'echo',
  'updown',
  'vibrate',
  'none',
];
const LONG_SOUNDS = new Set(['alien', 'climb', 'persistent', 'echo', 'updown']);
const soundLabel = (sound: string) =>
  sound === 'none'
    ? 'Silent'
    : sound === 'vibrate'
      ? 'Vibrate only'
      : `${sound[0].toUpperCase()}${sound.slice(1)}${LONG_SOUNDS.has(sound) ? ' (long)' : ''}`;
const soundOptions = (current: string, defaultLabel: string) => [
  { value: '', label: defaultLabel },
  ...(current && !PUSHOVER_SOUNDS.includes(current) ? [current] : [])
    .concat(PUSHOVER_SOUNDS)
    .map((sound) => ({ value: sound, label: soundLabel(sound) })),
];

const CHANNEL_LABEL: Record<AlertChannelName, string> = { pushover: 'Pushover', ntfy: 'ntfy', webhook: 'Webhook', mqtt: 'MQTT' };

// The firmware's built-in wording (lib/AlertLogic), written as templates;
// shown as the placeholder until you write your own.
const DEFAULT_TEXT: Record<AlertEventKey, { title: string; message: string }> = {
  unsafe: { title: 'Observatory UNSAFE', message: '{reasons}' },
  safe: { title: 'Observatory safe', message: 'All enabled safety rules pass.' },
  rain_started: { title: 'Rain detected', message: 'The rain sensor reports rain ({rain_rate} mm/h).' },
  rain_stopped: { title: 'Rain cleared', message: 'No rain for the configured rain clear delay.' },
  sensor_fault: { title: '{sensor} sensor fault', message: '{sensor} is offline or reporting errors.' },
  sensor_recovered: { title: '{sensor} sensor recovered', message: '{sensor} is reporting normally again.' },
  dew_risk: { title: 'Dew risk', message: 'Temperature {temp} °C is within {dew_margin} °C of the dew point ({dewpoint} °C).' },
  clear_sky: { title: 'Dark and clear', message: 'Cloud cover is down to {cloud}% (clear below {clear_below}%).' },
  clouded_over: { title: 'Clouded over', message: 'Cloud cover is up to {cloud}% (cloudy above {cloudy_above}%).' },
  client_lost: {
    title: 'Imaging app stopped checking',
    message: 'No request to the {device} for {silent_for} - last checked {last_checked}.',
  },
  client_back: { title: 'Imaging app is back', message: 'The {device} is being checked again.' },
  client_disconnected: { title: 'Imaging app disconnected', message: 'The {device} was disconnected.' },
};

// "Dark and clear" only when sky alerts wait for darkness, as on the device.
const defaultText = (key: AlertEventKey, skyNightOnly: boolean) =>
  key === 'clear_sky' && !skyNightOnly ? { ...DEFAULT_TEXT.clear_sky, title: 'Skies clear' } : DEFAULT_TEXT[key];

const VAR_HELP: Record<string, string> = {
  event: 'Event name, e.g. unsafe or rain_started',
  reasons: 'Every failing rule with its value and limit, one per line',
  reasons_inline: 'The same, on one line',
  reason_count: 'How many rules are failing',
  sensor: 'Which sensor',
  dew_margin_min: 'Dew risk margin setting',
  device: 'Device name',
  time: 'Local time',
  date: 'Local date',
  level: 'Alert level',
  sqm: 'Sky quality, mag/arcsec²',
  sqm_min: 'SQM safety minimum',
  cloud: 'Cloud cover %',
  cloud_max: 'Cloud cover safety limit %',
  clear_below: '"Clear" threshold %',
  cloudy_above: '"Clouded over" threshold %',
  sky_temp: 'Sky temperature °C',
  temp: 'Temperature °C',
  humidity: 'Humidity %',
  humidity_max: 'Humidity safety limit %',
  dewpoint: 'Dew point °C',
  dew_margin: 'Temperature minus dew point °C',
  pressure: 'Pressure hPa',
  rain_rate: 'Rain rate mm/h',
  wind: 'Wind m/s',
  gust: 'Gust m/s',
  sun_alt: 'Sun altitude °',
  silent_for: 'The "silent for" time, e.g. 2 min',
  last_checked: 'When the imaging app last checked: a time, or "N min ago"',
  client_id: 'The Alpaca ClientID the imaging app sent, if any',
};
// For the imaging-app events {device} is the Alpaca device, not the device name.
const CLIENT_VAR_HELP: Record<string, string> = { device: '"safety monitor" or "weather device"' };
const CLIENT_EVENTS: AlertEventKey[] = ['client_lost', 'client_back', 'client_disconnected'];

// On this tab the "Alerts are off" link (D-04) is shown once, by the Send
// alerts switch, not on every row - and channels can be set up and tested
// while alerts are off.
const withoutAlertsOff = (entry: DepEntry): DepEntry => {
  const { blockedBy, ...rest } = entry;
  const next: DepEntry = blockedBy && blockedBy.reason !== 'alerts-off' ? { ...rest, blockedBy } : rest;
  return entry.reason === 'alerts-off'
    ? { ...next, state: 'active', reason: undefined, text: undefined, fix: undefined, id: entry.id }
    : next;
};

const AlertsTab: FunctionalComponent<SettingsTabProps> = ({ config, update, updateMany, error, status, dirty, deps, fix }) => {
  const dep = (setting: string) => withoutAlertsOff(deps.get(setting));
  const alerts = mergeAlertsConfig(config.alerts);
  const [testResult, setTestResult] = useState<{ target: string; type: 'success' | 'error' | 'pending'; text: string } | null>(null);

  const set = (path: string[], value: unknown) => update(['alerts', ...path], value);
  const err = (key: string) => error(`alerts.${key}`);
  const off = !alerts.enabled;

  const fetchRecent = async (): Promise<AlertRecord[]> => {
    const response = await fetch('/api/alerts/recent');
    if (!response.ok) return [];
    const body = await response.json();
    return Array.isArray(body?.alerts) ? body.alerts : [];
  };

  // The device sends the test in the background; follow its delivery
  // status until every channel reports sent / failed / skipped.
  const runTest = async (
    target: string,
    query: string,
    matches: (record: AlertRecord) => boolean,
    pick: (record: AlertRecord) => string | null,
  ) => {
    setTestResult({ target, type: 'pending', text: 'Sending...' });
    try {
      const before = (await fetchRecent())[0]?.id ?? 0;
      const response = await fetch(`/api/alerts/test?${query}`, { method: 'POST' });
      if (!response.ok) {
        const body = await response.json().catch(() => ({}));
        setTestResult({ target, type: 'error', text: body.error ?? 'Test failed' });
        return;
      }
      const deadline = Date.now() + 30000;
      while (Date.now() < deadline) {
        await new Promise((resolve) => setTimeout(resolve, 1000));
        const record = (await fetchRecent()).find((r) => r.id > before && matches(r));
        const result = record ? pick(record) : null;
        if (result !== null) {
          const failed = result.startsWith('!');
          setTestResult({ target, type: failed ? 'error' : 'success', text: failed ? result.slice(1) : result });
          return;
        }
      }
      setTestResult({ target, type: 'error', text: 'No result from the device after 30 s.' });
    } catch {
      setTestResult({ target, type: 'error', text: 'Could not reach the device' });
    }
  };

  // null while pending; a leading "!" marks a failure.
  const channelResult = (channel: AlertChannelName) => (record: AlertRecord) => {
    const result = record.channels[channel];
    if (!result || result.status === 'pending') return null;
    if (result.status === 'sent') return 'Delivered.';
    return `!${result.status === 'skipped' ? 'Skipped' : 'Failed'}: ${result.detail}`;
  };

  const allChannelsResult = (record: AlertRecord) => {
    const entries = Object.entries(record.channels) as [AlertChannelName, { status: string; detail: string }][];
    if (entries.some(([, r]) => r.status === 'pending')) return null;
    const summary = entries
      .map(([channel, r]) => `${CHANNEL_LABEL[channel]} ${r.status}${r.status === 'sent' ? '' : `: ${r.detail}`}`)
      .join(' · ');
    return entries.every(([, r]) => r.status === 'sent') ? summary : `!${summary}`;
  };

  const sendTest = (channel: AlertChannelName) => runTest(channel, `channel=${channel}`, (r) => r.event === 'test', channelResult(channel));

  const sendEventTest = (key: AlertEventKey) => {
    const event = alerts.events[key];
    const query =
      `channel=all&event=${key}&level=${event.level}&sound=${encodeURIComponent(event.sound)}` +
      `&title=${encodeURIComponent(event.title ?? '')}&message=${encodeURIComponent(event.message ?? '')}`;
    if (channelCount === 0) {
      // Only paired phones to ring; nothing to follow.
      setTestResult({ target: key, type: 'pending', text: 'Sending...' });
      fetch(`/api/alerts/test?${query}`, { method: 'POST' })
        .then(async (response) => {
          const body = await response.json().catch(() => ({}));
          setTestResult(
            response.ok
              ? { target: key, type: 'success', text: 'Ringing paired phones.' }
              : { target: key, type: 'error', text: body.error ?? 'Test failed' },
          );
        })
        .catch(() => setTestResult({ target: key, type: 'error', text: 'Could not reach the device' }));
      return;
    }
    return runTest(key, query, (r) => r.event === key && r.title.startsWith('Test'), allChannelsResult);
  };

  const resultNote = (target: string) =>
    testResult?.target === target &&
    (testResult.type === 'pending' ? (
      <Note>{testResult.text}</Note>
    ) : (
      <ResultNote result={{ type: testResult.type, text: testResult.text }} />
    ));

  // Render function (not a nested component) so re-renders don't remount it.
  const testButton = (channel: AlertChannelName) => {
    const entry = dep(`alerts.${channel}.enabled`);
    if (entry.state === 'off' || entry.state === 'inactive') return null;
    return (
      <div class="btn-row">
        <ActionButton
          onClick={() => sendTest(channel)}
          disabled={dirty || testResult?.type === 'pending'}
          title={dirty ? 'Save first' : undefined}
        >
          Send test
        </ActionButton>
        {resultNote(channel)}
      </div>
    );
  };

  const pushoverOn = alerts.pushover.enabled;

  // Paused or sending is live device state, not a saved setting.
  const { schedule: shownSchedule, busy: scheduleBusy, pauseOrResume: sendPauseOrResume } = useAlertSchedule(status?.alerts);
  const pauseOrResume = async (resume: boolean) => {
    if (!(await sendPauseOrResume(resume))) showToast({ message: 'Could not reach the device', tone: 'bad' });
  };
  const sendMode: AlertSendMode = alerts.sendMode ?? (alerts.armWithAlpaca ? 'whileConnected' : 'any');
  const [editing, setEditing] = useState<AlertEventKey | null>(null);
  // Where a clicked {variable} goes: the last focused title/message field.
  const lastField = useRef<{ key: AlertEventKey; field: 'title' | 'message'; el: HTMLInputElement | HTMLTextAreaElement } | null>(null);

  const insertVar = (key: AlertEventKey, name: string) => {
    const target = lastField.current?.key === key ? lastField.current : null;
    const field = target?.field ?? 'message';
    const current = alerts.events[key][field] ?? '';
    const start = target?.el.selectionStart ?? current.length;
    const end = target?.el.selectionEnd ?? current.length;
    const token = `{${name}}`;
    set(['events', key, field], current.slice(0, start) + token + current.slice(end));
    if (target) {
      requestAnimationFrame(() => {
        target.el.focus();
        target.el.setSelectionRange(start + token.length, start + token.length);
      });
    }
  };

  const templateEditor = (key: AlertEventKey) => {
    const event = alerts.events[key];
    const track = (field: 'title' | 'message') => (e: Event) => {
      lastField.current = { key, field, el: e.currentTarget as HTMLInputElement | HTMLTextAreaElement };
    };
    return (
      <div class="template-editor" data-template={key}>
        <Field label="Title" error={(event.title ?? '').length > 80 ? 'Up to 80 characters' : undefined}>
          <input
            class="input"
            aria-label="Alert title"
            value={event.title ?? ''}
            placeholder={defaultText(key, alerts.skyNightOnly).title}
            maxLength={80}
            onFocus={track('title')}
            onKeyUp={track('title')}
            onClick={track('title')}
            onInput={(e) => set(['events', key, 'title'], (e.target as HTMLInputElement).value)}
          />
        </Field>
        <Field label="Message" error={(event.message ?? '').length > 240 ? 'Up to 240 characters' : undefined}>
          <textarea
            class="input"
            aria-label="Alert message"
            value={event.message ?? ''}
            placeholder={defaultText(key, alerts.skyNightOnly).message}
            maxLength={240}
            onFocus={track('message')}
            onKeyUp={track('message')}
            onClick={track('message')}
            onInput={(e) => set(['events', key, 'message'], (e.target as HTMLTextAreaElement).value)}
          />
        </Field>
        <div class="var-chips" aria-label="Insert a value">
          {[...(EVENT_VARS[key] ?? []), ...COMMON_VARS].map((name) => (
            <button
              key={name}
              type="button"
              class="var-chip"
              title={(CLIENT_EVENTS.includes(key) ? CLIENT_VAR_HELP[name] : undefined) ?? VAR_HELP[name]}
              onMouseDown={(e) => e.preventDefault()}
              onClick={() => insertVar(key, name)}
            >
              {`{${name}}`}
            </button>
          ))}
        </div>
        {(event.title || event.message) && (
          <div class="btn-row">
            <ActionButton
              onClick={() =>
                updateMany([
                  [['alerts', 'events', key, 'title'], ''],
                  [['alerts', 'events', key, 'message'], ''],
                ])
              }
            >
              Use the default wording
            </ActionButton>
          </div>
        )}
      </div>
    );
  };

  // Render function (not a nested component) so re-renders don't remount it.
  const eventRow = (key: AlertEventKey, label: string, opts: { hint?: string; threshold?: ComponentChildren } = {}) => {
    const event = alerts.events[key];
    const entry = dep(`alerts.events.${key}.level`);
    // Can't be raised from Off while what it needs is missing (FR-005).
    const locked = event.level === 0 && entry.blockedBy !== undefined;
    return (
      <>
        <div class="event-row" data-event={key}>
          <span class="event-label">
            <span>
              {label}
              {opts.hint && <InfoTip text={opts.hint} />}
            </span>
            {opts.threshold}
          </span>
          <SelectInput
            value={String(event.level)}
            ariaLabel={`${label}: level`}
            options={LEVEL_OPTIONS}
            disabled={off || locked}
            onChange={(v) => set(['events', key, 'level'], parseInt(v, 10))}
          />
          {pushoverOn &&
            (event.level >= 2 ? (
              <SelectInput
                value={event.sound}
                ariaLabel={`${label}: sound`}
                options={soundOptions(event.sound, 'Default')}
                disabled={off}
                onChange={(v) => set(['events', key, 'sound'], v)}
              />
            ) : (
              <span />
            ))}
          <ActionButton
            onClick={() => sendEventTest(key)}
            disabled={off || event.level === 0 || entry.state === 'inactive' || testResult?.type === 'pending'}
            title={event.level === 0 ? 'Off' : entry.state === 'inactive' ? entry.text : `Send a sample "${label}" alert at this level`}
          >
            Test
          </ActionButton>
          <ActionButton onClick={() => setEditing(editing === key ? null : key)} disabled={off} title="Write your own title and message">
            {event.title || event.message ? 'Text •' : 'Text'}
          </ActionButton>
        </div>
        {resultNote(key)}
        {editing === key && templateEditor(key)}
        <DepNote entry={entry} onFix={fix} />
      </>
    );
  };
  // A GPS fix wins over the location in Settings (saved or not).
  const location =
    status?.sky?.locationSource === 'gps' && status.sky.latitude !== undefined && status.sky.longitude !== undefined
      ? { latitude: status.sky.latitude, longitude: status.sky.longitude }
      : config.location?.set
        ? config.location
        : null;
  const darknessNote = describeDarkness({
    sky: status?.sky,
    location,
    formLimitDeg: alerts.nightSunAltitudeDeg,
    deviceNow: deviceTime(status),
  });
  // Counts only channels that can deliver (FR-007).
  const channelEntries = (['pushover', 'ntfy', 'webhook', 'mqtt'] as const).map((channel) => dep(`alerts.${channel}.enabled`));
  const channelsOn = channelEntries.filter((e) => e.state !== 'off').length;
  const channelCount = channelEntries.filter((e) => e.state === 'active' || e.state === 'unknown').length;
  const wakePhones = deps.get('alerts.wakePhones');

  return (
    <>
      <SettingsCard
        id="alerts"
        title="Alerts"
        hint="Push notifications sent by the device itself."
        badge={
          off ? undefined : (
            <StatusBadge
              tone={channelCount === 0 ? 'warn' : 'ok'}
              label={channelCount === 0 ? 'No channels' : `${channelCount} channel${channelCount === 1 ? '' : 's'}`}
            />
          )
        }
      >
        <Toggle label="Send alerts" checked={alerts.enabled} onChange={(v) => set(['enabled'], v)} />
        {!off && channelsOn === 0 && <Requires tone="warn">Turn on a channel below.</Requires>}
        {!off && channelsOn > 0 && channelCount === 0 && (
          <Requires tone="warn">Alerts reach nowhere: no channel can deliver right now.</Requires>
        )}
        {!off && (
          <Group title="When to send">
            <SelectInput
              value={sendMode}
              ariaLabel="When to send"
              options={SEND_MODE_OPTIONS.map(({ value, label }) => ({ value, label }))}
              onChange={(v) =>
                updateMany([
                  [['alerts', 'sendMode'], v],
                  [['alerts', 'armWithAlpaca'], v === 'whileConnected'],
                ])
              }
            />
            <Note>{SEND_MODE_OPTIONS.find((option) => option.value === sendMode)?.help}</Note>
            {dep('alerts.sendMode').state === 'inactive' && <DepNote entry={dep('alerts.sendMode')} onFix={fix} />}
            <div class="btn-row" data-schedule-status>
              {shownSchedule && (
                <Note tone={shownSchedule.armed ? 'ok' : 'warn'}>
                  {describeSchedule({ ...shownSchedule, mode: shownSchedule.mode ?? sendMode })}
                </Note>
              )}
              <ActionButton
                onClick={() => pauseOrResume(!(shownSchedule?.armed ?? true))}
                disabled={shownSchedule === null || scheduleBusy}
              >
                {shownSchedule?.armed === false ? 'Resume alerts' : 'Pause alerts'}
              </ActionButton>
              <InfoTip text={PAUSE_HINT} />
            </div>
            {status?.alpaca?.clients && describeSilentClients(status.alpaca.clients) && (
              <Note tone="warn">{describeSilentClients(status.alpaca.clients)}</Note>
            )}
          </Group>
        )}
      </SettingsCard>

      <SettingsCard title="Notify me when" hint={LEVEL_HINT}>
        <fieldset class="card-body" disabled={off}>
          <div class={`event-table${pushoverOn ? ' with-sound' : ''}`}>
            <div class="event-row event-table-head" aria-hidden="true">
              <span />
              <span>Level</span>
              {pushoverOn && <span>Pushover sound</span>}
              <span />
              <span />
            </div>
            {eventRow('unsafe', 'It turns unsafe', { hint: 'Lists the reasons.' })}
            {eventRow('safe', "It's safe again")}
            {eventRow('rain_started', 'Rain starts')}
            {eventRow('rain_stopped', 'Rain stops')}
            {eventRow('sensor_fault', 'A sensor fails', { hint: 'Includes the RG-15 lens fault.' })}
            {eventRow('sensor_recovered', 'A sensor recovers')}
            {eventRow('dew_risk', 'Dew risk within', {
              threshold: (
                <NumberInput
                  min={0}
                  max={10}
                  step={0.5}
                  unit="°C"
                  ariaLabel="Dew risk margin"
                  value={alerts.dewRiskMarginC}
                  disabled={off}
                  onChange={(v) => set(['dewRiskMarginC'], v)}
                />
              ),
              hint: 'Temperature within this margin of the dew point.',
            })}
            {eventRow('clear_sky', 'Skies clear up below', {
              threshold: (
                <NumberInput
                  min={0}
                  max={100}
                  step={1}
                  unit="%"
                  ariaLabel="Clear below"
                  value={alerts.clearSkyCloudPercent}
                  disabled={off}
                  onChange={(v) => set(['clearSkyCloudPercent'], v)}
                />
              ),
              hint: 'Cloud cover.',
            })}
            {eventRow('clouded_over', 'Skies cloud over above', {
              threshold: (
                <NumberInput
                  min={0}
                  max={100}
                  step={1}
                  unit="%"
                  ariaLabel="Clouded over above"
                  value={alerts.cloudedOverCloudPercent}
                  error={err('cloudedOverCloudPercent')}
                  disabled={off}
                  onChange={(v) => set(['cloudedOverCloudPercent'], v)}
                />
              ),
              hint: 'Cloud cover.',
            })}
            {eventRow('client_lost', 'The imaging app stops checking', {
              hint: 'No request reached the safety monitor or weather device for the time below. A crash, a sleeping PC or a network drop.',
            })}
            {eventRow('client_back', 'The imaging app is back', {
              hint: 'It started checking again after going quiet.',
            })}
            {eventRow('client_disconnected', 'The imaging app disconnects', {
              hint: 'It disconnected normally, for example at the end of a session.',
            })}
          </div>
          {err('cloudedOverCloudPercent') && <Note tone="bad">{err('cloudedOverCloudPercent')}</Note>}
          {wakePhones.state === 'inactive' && <DepNote entry={wakePhones} onFix={fix} prefix="Phones won't ring" />}
          <div class="form-grid">
            <Field
              label="Silent for - safety monitor"
              error={err('clientSilentSafetySeconds')}
              hint="How long without a request before you're told. Imaging apps usually check the safety monitor every few seconds."
            >
              <NumberInput
                min={0.5}
                max={60}
                step={0.5}
                unit="min"
                ariaLabel="Silent for - safety monitor"
                value={secondsToMinutes(alerts.clientSilentSafetySeconds ?? 120)}
                disabled={off}
                onChange={(v) => set(['clientSilentSafetySeconds'], minutesToSeconds(v))}
              />
            </Field>
            <Field
              label="Silent for - weather device"
              error={err('clientSilentWeatherSeconds')}
              hint="Imaging apps check weather less often; keep this longer than their weather interval."
            >
              <NumberInput
                min={0.5}
                max={60}
                step={0.5}
                unit="min"
                ariaLabel="Silent for - weather device"
                value={secondsToMinutes(alerts.clientSilentWeatherSeconds ?? 600)}
                disabled={off}
                onChange={(v) => set(['clientSilentWeatherSeconds'], minutesToSeconds(v))}
              />
            </Field>
          </div>
          <div class="rule-row">
            <div class="toggle-stack">
              <DepToggle
                entry={dep('alerts.skyNightOnly')}
                onFix={fix}
                label="Sky alerts only when it's dark"
                checked={alerts.skyNightOnly}
                onChange={(v) => set(['skyNightOnly'], v)}
                disabled={off}
                hint="From the sun's position at your location. If it's already clear at nightfall, you get one 'Dark and clear' alert."
              />
              <DepToggle
                entry={dep('alerts.safetyNightOnly')}
                onFix={fix}
                label="Safety alerts only when it's dark"
                checked={alerts.safetyNightOnly}
                onChange={(v) => set(['safetyNightOnly'], v)}
                disabled={off}
                hint="So dawn brightening the sky past the SQM limit doesn't wake you. At nightfall you hear about it if safe/unsafe changed since the last alert. Rain and sensor alerts still come at any time."
              />
            </div>
            <SelectInput
              value={String(alerts.nightSunAltitudeDeg)}
              ariaLabel="Dark means"
              disabled={off || (!alerts.skyNightOnly && !alerts.safetyNightOnly)}
              options={[
                { value: '-0.833', label: 'After sunset' },
                { value: '-12', label: 'Nautical dark (-12°)' },
                { value: '-18', label: 'Astronomical dark (-18°)' },
              ]}
              onChange={(v) => set(['nightSunAltitudeDeg'], parseFloat(v))}
            />
          </div>
          {darknessNote && <Note>{darknessNote}</Note>}
          <div class="form-grid">
            <Field
              label="Cooldown"
              error={err('cooldownSeconds')}
              hint="Minimum gap between alerts of the same kind. A change held back is sent when it ends."
            >
              <NumberInput
                integer
                min={0}
                max={86400}
                unit="s"
                value={alerts.cooldownSeconds}
                disabled={off}
                onChange={(v) => set(['cooldownSeconds'], v)}
              />
            </Field>
          </div>
        </fieldset>
      </SettingsCard>

      <SettingsCard
        title="Channels"
        hint="Tests use the saved settings and work while alerts are off. Sent alerts appear under the bell in the header."
      >
        <Group>
          <DepToggle
            entry={dep('alerts.pushover.enabled')}
            onFix={fix}
            label="Pushover"
            checked={alerts.pushover.enabled}
            onChange={(v) => set(['pushover', 'enabled'], v)}
          />
          {alerts.pushover.enabled && (
            <>
              <div class="form-grid">
                <Field
                  label="User key"
                  error={err('pushover.userKey')}
                  hint="Your user key, top of the Pushover dashboard - not the app token or your email."
                >
                  <TextInput
                    dataField="alerts.pushover.userKey"
                    type="password"
                    value={alerts.pushover.userKey}
                    onInput={(v) => set(['pushover', 'userKey'], v)}
                  />
                </Field>
                <Field label="App token" error={err('pushover.appToken')} hint="Create an application at pushover.net.">
                  <TextInput type="password" value={alerts.pushover.appToken} onInput={(v) => set(['pushover', 'appToken'], v)} />
                </Field>
                <Field label="Default sound" hint="Used where an event's sound is Default.">
                  <SelectInput
                    value={alerts.pushover.sound}
                    options={soundOptions(alerts.pushover.sound, 'Pushover default')}
                    onChange={(v) => set(['pushover', 'sound'], v)}
                  />
                </Field>
              </div>
              {testButton('pushover')}
            </>
          )}
        </Group>

        <Group>
          <DepToggle
            entry={dep('alerts.ntfy.enabled')}
            onFix={fix}
            label="ntfy"
            checked={alerts.ntfy.enabled}
            onChange={(v) => set(['ntfy', 'enabled'], v)}
          />
          {alerts.ntfy.enabled && (
            <>
              <div class="form-grid">
                <Field label="Server" error={err('ntfy.server')}>
                  <TextInput type="url" value={alerts.ntfy.server} onInput={(v) => set(['ntfy', 'server'], v)} />
                </Field>
                <Field label="Topic" error={err('ntfy.topic')} hint="ntfy.sh topics are public - use a long random name.">
                  <TextInput dataField="alerts.ntfy.topic" value={alerts.ntfy.topic} onInput={(v) => set(['ntfy', 'topic'], v)} />
                </Field>
                <Field label="Token">
                  <TextInput type="password" value={alerts.ntfy.token} placeholder="Optional" onInput={(v) => set(['ntfy', 'token'], v)} />
                </Field>
              </div>
              {testButton('ntfy')}
            </>
          )}
        </Group>

        <Group>
          <DepToggle
            entry={dep('alerts.webhook.enabled')}
            onFix={fix}
            label="Webhook"
            checked={alerts.webhook.enabled}
            onChange={(v) => set(['webhook', 'enabled'], v)}
            hint="POSTs JSON: device, event, title, message, level, timestamp."
          />
          {alerts.webhook.enabled && (
            <>
              <div class="form-grid">
                <Field label="URL" error={err('webhook.url')}>
                  <TextInput
                    dataField="alerts.webhook.url"
                    type="url"
                    value={alerts.webhook.url}
                    placeholder="http://homeassistant.local:8123/api/webhook/..."
                    onInput={(v) => set(['webhook', 'url'], v)}
                  />
                </Field>
                <Field label="Authorization header">
                  <TextInput
                    type="password"
                    value={alerts.webhook.authHeader}
                    placeholder="Optional"
                    onInput={(v) => set(['webhook', 'authHeader'], v)}
                  />
                </Field>
              </div>
              {alerts.webhook.url.startsWith('https://') && (
                <Toggle
                  label="Skip certificate checks"
                  checked={alerts.webhook.insecureTls}
                  onChange={(v) => set(['webhook', 'insecureTls'], v)}
                  hint="Only for a self-signed server on your own network."
                />
              )}
              {testButton('webhook')}
            </>
          )}
        </Group>

        <Group>
          <DepToggle
            entry={dep('alerts.mqtt.enabled')}
            onFix={fix}
            label="MQTT"
            checked={alerts.mqtt.enabled}
            onChange={(v) => set(['mqtt', 'enabled'], v)}
            hint={`Publishes each alert to ${config.mqtt.topic}/alerts. The safe flag is set under Network → MQTT → Publish.`}
          />
          {testButton('mqtt')}
        </Group>
      </SettingsCard>
    </>
  );
};

export default AlertsTab;
