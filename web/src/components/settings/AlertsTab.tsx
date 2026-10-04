import { ComponentChildren, FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import type { AlertChannelName, AlertEventKey, AlertRecord } from '../../types';
import { mergeAlertsConfig } from './defaults';
import { darkness, formatClock, formatDuration, sunPosition } from '../../lib/astro';
import type { SettingsTabProps } from './context';
import { InfoTip, Note } from '../ui';
import { ActionButton, Field, Group, NumberInput, Requires, ResultNote, SelectInput, SettingsCard, StatusBadge, TextInput, Toggle } from './controls';

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
  'pushover', 'bike', 'bugle', 'cashregister', 'classical', 'cosmic', 'falling', 'gamelan', 'incoming', 'intermission', 'magic',
  'mechanical', 'pianobar', 'siren', 'spacealarm', 'tugboat', 'alien', 'climb', 'persistent', 'echo', 'updown', 'vibrate', 'none',
];
const LONG_SOUNDS = new Set(['alien', 'climb', 'persistent', 'echo', 'updown']);
const soundLabel = (sound: string) =>
  sound === 'none' ? 'Silent' : sound === 'vibrate' ? 'Vibrate only' : `${sound[0].toUpperCase()}${sound.slice(1)}${LONG_SOUNDS.has(sound) ? ' (long)' : ''}`;
const soundOptions = (current: string, defaultLabel: string) => [
  { value: '', label: defaultLabel },
  ...(current && !PUSHOVER_SOUNDS.includes(current) ? [current] : []).concat(PUSHOVER_SOUNDS).map((sound) => ({ value: sound, label: soundLabel(sound) })),
];

const CHANNEL_LABEL: Record<AlertChannelName, string> = { pushover: 'Pushover', ntfy: 'ntfy', webhook: 'Webhook', mqtt: 'MQTT' };

const describeDarkness = (latitude: number, longitude: number, darkAltitude: number) => {
  const now = new Date();
  const sun = sunPosition(now, latitude, longitude).altitude;
  const { darkNow, start, end } = darkness(latitude, longitude, darkAltitude, now);
  const sunNow = `Sun at ${sun.toFixed(1)}° now`;
  if (darkNow) return `${sunNow} - dark until ${formatClock(end)}.`;
  if (!start) return `${sunNow} - it doesn't get that dark in the next day and a half.`;
  return `${sunNow} - dark in ${formatDuration(start.valueOf() - now.valueOf())}, ${formatClock(start)} to ${formatClock(end)}.`;
};

const AlertsTab: FunctionalComponent<SettingsTabProps> = ({ config, update, error, hw, status, dirty, goTo }) => {
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
  const runTest = async (target: string, query: string, matches: (record: AlertRecord) => boolean, pick: (record: AlertRecord) => string | null) => {
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
    const summary = entries.map(([channel, r]) => `${CHANNEL_LABEL[channel]} ${r.status}${r.status === 'sent' ? '' : `: ${r.detail}`}`).join(' · ');
    return entries.every(([, r]) => r.status === 'sent') ? summary : `!${summary}`;
  };

  const sendTest = (channel: AlertChannelName) => runTest(channel, `channel=${channel}`, (r) => r.event === 'test', channelResult(channel));

  const sendEventTest = (key: AlertEventKey) => {
    const event = alerts.events[key];
    const query = `channel=all&event=${key}&level=${event.level}&sound=${encodeURIComponent(event.sound)}`;
    if (channelCount === 0) {
      // Only paired phones to ring; nothing to follow.
      setTestResult({ target: key, type: 'pending', text: 'Sending...' });
      fetch(`/api/alerts/test?${query}`, { method: 'POST' })
        .then(async (response) => {
          const body = await response.json().catch(() => ({}));
          setTestResult(response.ok ? { target: key, type: 'success', text: 'Ringing paired phones.' } : { target: key, type: 'error', text: body.error ?? 'Test failed' });
        })
        .catch(() => setTestResult({ target: key, type: 'error', text: 'Could not reach the device' }));
      return;
    }
    return runTest(key, query, (r) => r.event === key && r.title.startsWith('Test'), allChannelsResult);
  };

  const resultNote = (target: string) =>
    testResult?.target === target &&
    (testResult.type === 'pending' ? <Note>{testResult.text}</Note> : <ResultNote result={{ type: testResult.type, text: testResult.text }} />);

  // Render function (not a nested component) so re-renders don't remount it.
  const testButton = (channel: AlertChannelName) => (
    <div class="btn-row">
      <ActionButton onClick={() => sendTest(channel)} disabled={dirty || testResult?.type === 'pending'} title={dirty ? 'Save first' : undefined}>
        Send test
      </ActionButton>
      {resultNote(channel)}
    </div>
  );

  const rainReason = !hw.rain.enabled ? 'Rain sensor is off.' : null;
  const dewReason = hw.environment.detected === false ? 'BME280 not detected.' : null;
  const skyReason = hw.irSky.detected === false ? 'MLX90614 not detected.' : null;
  const pushoverOn = alerts.pushover.enabled;

  // Render function (not a nested component) so re-renders don't remount it.
  const eventRow = (
    key: AlertEventKey,
    label: string,
    opts: { hint?: string; blocked?: string | null; fix?: () => void; threshold?: ComponentChildren } = {}
  ) => {
    const event = alerts.events[key];
    const locked = Boolean(opts.blocked) && event.level === 0;
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
              <SelectInput value={event.sound} ariaLabel={`${label}: sound`} options={soundOptions(event.sound, 'Default')} disabled={off} onChange={(v) => set(['events', key, 'sound'], v)} />
            ) : (
              <span />
            ))}
          <ActionButton
            onClick={() => sendEventTest(key)}
            disabled={off || event.level === 0 || testResult?.type === 'pending'}
            title={event.level === 0 ? 'Off' : `Send a sample "${label}" alert at this level`}
          >
            Test
          </ActionButton>
        </div>
        {resultNote(key)}
        {opts.blocked && (
          <Requires tone={event.level > 0 ? 'warn' : 'info'} onFix={opts.fix}>
            {opts.blocked}
          </Requires>
        )}
      </>
    );
  };
  // Unknown until status loads; a location typed but not yet saved counts.
  const noLocation = status !== null && status.sky?.locationSource === 'none' && !config.location?.set;
  // A GPS fix wins over the location in Settings (saved or not).
  const location =
    status?.sky?.locationSource === 'gps' && status.sky.latitude !== undefined && status.sky.longitude !== undefined
      ? { latitude: status.sky.latitude, longitude: status.sky.longitude }
      : config.location?.set
        ? config.location
        : null;
  const darknessNote = location ? describeDarkness(location.latitude, location.longitude, alerts.nightSunAltitudeDeg) : null;
  const channelCount = [alerts.pushover.enabled, alerts.ntfy.enabled, alerts.webhook.enabled, alerts.mqtt.enabled].filter(Boolean).length;

  return (
    <>
      <SettingsCard
        id="alerts"
        title="Alerts"
        hint="Push notifications sent by the device itself."
        badge={
          off ? undefined : <StatusBadge
            tone={channelCount === 0 ? 'warn' : 'ok'}
            label={channelCount === 0 ? 'No channels' : `${channelCount} channel${channelCount === 1 ? '' : 's'}`}
          />
        }
      >
        <Toggle label="Send alerts" checked={alerts.enabled} onChange={(v) => set(['enabled'], v)} />
        {!off && channelCount === 0 && <Requires tone="warn">Turn on a channel below.</Requires>}
      </SettingsCard>

      <SettingsCard title="Notify me when" hint={LEVEL_HINT}>
        <fieldset class="card-body" disabled={off}>
          <div class={`event-table${pushoverOn ? ' with-sound' : ''}`}>
            <div class="event-row event-head" aria-hidden="true">
              <span />
              <span>Level</span>
              {pushoverOn && <span>Pushover sound</span>}
              <span />
            </div>
            {eventRow('unsafe', 'It turns unsafe', { hint: 'Lists the reasons.' })}
            {eventRow('safe', "It's safe again")}
            {eventRow('rain_started', 'Rain starts', { blocked: rainReason, fix: () => goTo('sensors', 'rain') })}
            {eventRow('rain_stopped', 'Rain stops', { blocked: rainReason, fix: () => goTo('sensors', 'rain') })}
            {eventRow('sensor_fault', 'A sensor fails', { hint: 'Includes the RG-15 lens fault.' })}
            {eventRow('sensor_recovered', 'A sensor recovers')}
            {eventRow('dew_risk', 'Dew risk within', {
              blocked: dewReason,
              threshold: (
                <NumberInput min={0} max={10} step={0.5} unit="°C" ariaLabel="Dew risk margin" value={alerts.dewRiskMarginC} disabled={off} onChange={(v) => set(['dewRiskMarginC'], v)} />
              ),
              hint: 'Temperature within this margin of the dew point.',
            })}
            {eventRow('clear_sky', 'Skies clear up below', {
              blocked: skyReason,
              threshold: (
                <NumberInput min={0} max={100} step={1} unit="%" ariaLabel="Clear below" value={alerts.clearSkyCloudPercent} disabled={off} onChange={(v) => set(['clearSkyCloudPercent'], v)} />
              ),
              hint: 'Cloud cover.',
            })}
            {eventRow('clouded_over', 'Skies cloud over above', {
              blocked: skyReason,
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
          </div>
          {err('cloudedOverCloudPercent') && <Note tone="bad">{err('cloudedOverCloudPercent')}</Note>}
          <div class="rule-row">
            <Toggle
              label="Sky alerts only when it's dark"
              checked={alerts.skyNightOnly}
              onChange={(v) => set(['skyNightOnly'], v)}
              disabled={off}
              hint="From the sun's position at your location. If it's already clear at nightfall, you get one 'Dark and clear' alert."
              blockedReason={noLocation ? 'Needs your location.' : null}
              onFix={() => goTo('time', 'location')}
            />
            <SelectInput
              value={String(alerts.nightSunAltitudeDeg)}
              disabled={off || !alerts.skyNightOnly}
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
            <Field label="Cooldown" error={err('cooldownSeconds')} hint="Minimum gap between alerts of the same kind. A change held back is sent when it ends.">
              <NumberInput integer min={0} max={86400} unit="s" value={alerts.cooldownSeconds} disabled={off} onChange={(v) => set(['cooldownSeconds'], v)} />
            </Field>
          </div>
        </fieldset>
      </SettingsCard>

      <SettingsCard title="Channels" hint="Tests use the saved settings and work while alerts are off. Sent alerts appear under the bell in the header.">
        <Group>
          <Toggle label="Pushover" checked={alerts.pushover.enabled} onChange={(v) => set(['pushover', 'enabled'], v)} />
          {alerts.pushover.enabled && (
            <>
              <div class="form-grid">
                <Field label="User key" error={err('pushover.userKey')} hint="Your user key, top of the Pushover dashboard - not the app token or your email.">
                  <TextInput dataField="alerts.pushover.userKey" type="password" value={alerts.pushover.userKey} onInput={(v) => set(['pushover', 'userKey'], v)} />
                </Field>
                <Field label="App token" error={err('pushover.appToken')} hint="Create an application at pushover.net.">
                  <TextInput type="password" value={alerts.pushover.appToken} onInput={(v) => set(['pushover', 'appToken'], v)} />
                </Field>
                <Field label="Default sound" hint="Used where an event's sound is Default.">
                  <SelectInput value={alerts.pushover.sound} options={soundOptions(alerts.pushover.sound, 'Pushover default')} onChange={(v) => set(['pushover', 'sound'], v)} />
                </Field>
              </div>
              {testButton('pushover')}
            </>
          )}
        </Group>

        <Group>
          <Toggle label="ntfy" checked={alerts.ntfy.enabled} onChange={(v) => set(['ntfy', 'enabled'], v)} />
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
          <Toggle label="Webhook" checked={alerts.webhook.enabled} onChange={(v) => set(['webhook', 'enabled'], v)} hint="POSTs JSON: device, event, title, message, level, timestamp." />
          {alerts.webhook.enabled && (
            <>
              <div class="form-grid">
                <Field label="URL" error={err('webhook.url')}>
                  <TextInput dataField="alerts.webhook.url" type="url" value={alerts.webhook.url} placeholder="http://homeassistant.local:8123/api/webhook/..." onInput={(v) => set(['webhook', 'url'], v)} />
                </Field>
                <Field label="Authorization header">
                  <TextInput type="password" value={alerts.webhook.authHeader} placeholder="Optional" onInput={(v) => set(['webhook', 'authHeader'], v)} />
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

        <Group aside={alerts.mqtt.enabled && hw.mqtt.enabled && hw.mqtt.connected === false ? <StatusBadge tone="bad" label="Broker not connected" /> : undefined}>
          <Toggle
            label="MQTT"
            checked={alerts.mqtt.enabled}
            onChange={(v) => set(['mqtt', 'enabled'], v)}
            blockedReason={!hw.mqtt.enabled ? 'MQTT is off.' : null}
            onFix={() => goTo('network', 'mqtt')}
            hint={`Publishes to ${config.mqtt.topic}/alerts and a retained ${config.mqtt.topic}/safety.`}
          />
          {alerts.mqtt.enabled && hw.mqtt.enabled && testButton('mqtt')}
        </Group>
      </SettingsCard>

    </>
  );
};

export default AlertsTab;
