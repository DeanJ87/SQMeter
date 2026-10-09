import { ComponentChildren, FunctionalComponent } from 'preact';
import { useRef, useState } from 'preact/hooks';
import type { AlertChannelName, AlertEventKey, AlertRecord, AlertSendMode } from '../../types';
import { mergeAlertsConfig } from './defaults';
import { showToast } from '../toast';
import { darkness, formatClock, formatDuration, sunPosition } from '../../lib/astro';
import { deviceTime } from '../../lib/deviceTime';
import type { SettingsTabProps } from './context';
import { useAlertSchedule } from '../../hooks/useAlertSchedule';
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
import { t } from '../../i18n';

const LEVEL_OPTIONS = [
  { value: '0', label: t('settings.alerts.off') },
  { value: '1', label: t('settings.alerts.quiet') },
  { value: '2', label: t('settings.alerts.normal') },
  { value: '3', label: t('settings.alerts.urgent') },
  { value: '4', label: t('settings.alerts.wakeMe') },
];

const LEVEL_HINT = t('settings.alerts.quietNoSoundUrgentBreaks');

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
    ? t('settings.alerts.silent')
    : sound === 'vibrate'
      ? t('settings.alerts.vibrateOnly')
      : `${sound[0].toUpperCase()}${sound.slice(1)}${LONG_SOUNDS.has(sound) ? ' (long)' : ''}`;
const soundOptions = (current: string, defaultLabel: string) => [
  { value: '', label: defaultLabel },
  ...(current && !PUSHOVER_SOUNDS.includes(current) ? [current] : [])
    .concat(PUSHOVER_SOUNDS)
    .map((sound) => ({ value: sound, label: soundLabel(sound) })),
];

const CHANNEL_LABEL: Record<AlertChannelName, string> = {
  pushover: 'Pushover',
  ntfy: 'ntfy',
  webhook: t('settings.alerts.webhook'),
  mqtt: 'MQTT',
};

// Dark-or-not comes from the device's own sun position (what the alerts
// use); the start/end times are a prediction made here, shown in this
// browser's time zone.
const describeDarkness = (latitude: number, longitude: number, darkAltitude: number, deviceSunAltitude?: number, deviceNow?: Date) => {
  const now = deviceNow ?? new Date();
  const sun = deviceSunAltitude ?? sunPosition(now, latitude, longitude).altitude;
  const darkNow = sun <= darkAltitude;
  const predicted = darkness(latitude, longitude, darkAltitude, now);
  const sunNow = t('settings.alerts.sunAtFixedNowValue', {
    fixed: sun.toFixed(1),
    value: deviceSunAltitude === undefined ? '' : ' (device)',
  });
  const zone = Intl.DateTimeFormat().resolvedOptions().timeZone;
  const inZone = zone ? t('settings.alerts.thisBrowserSTimeZone', { zone }) : t('settings.alerts.thisBrowserSTime');
  if (darkNow)
    return predicted.end
      ? t('settings.alerts.darkUntil', { sunNow, clock: formatClock(predicted.end), inZone })
      : t('settings.alerts.darkNow', { sunNow });
  const start = predicted.darkNow ? null : predicted.start;
  if (!start) return t('settings.alerts.notDarkYet', { sunNow });
  return t('settings.alerts.sunnowDarkInDurationClock', {
    sunNow,
    duration: formatDuration(start.valueOf() - now.valueOf()),
    clock: formatClock(start),
    clock2: formatClock(predicted.end),
    inZone,
  });
};

// The firmware's built-in wording (lib/AlertLogic), written as templates;
// shown as the placeholder until you write your own.
const DEFAULT_TEXT: Record<AlertEventKey, { title: string; message: string }> = {
  unsafe: { title: t('settings.alerts.observatoryUnsafe'), message: '{reasons}' },
  safe: { title: t('settings.alerts.observatorySafe'), message: t('settings.alerts.allEnabledSafetyRulesPass') },
  rain_started: { title: t('settings.alerts.rainDetected'), message: t('settings.alerts.theRainSensorReportsRain') },
  rain_stopped: { title: t('settings.alerts.rainCleared'), message: t('settings.alerts.noRainForTheConfigured') },
  sensor_fault: { title: t('settings.alerts.sensorFaultTitle'), message: t('settings.alerts.sensorFaultMessage') },
  sensor_recovered: { title: t('settings.alerts.sensorRecoveredTitle'), message: t('settings.alerts.sensorRecoveredMessage') },
  dew_risk: { title: t('settings.alerts.dewRisk'), message: t('settings.alerts.temperatureTempCIsWithin') },
  clear_sky: { title: t('settings.alerts.darkAndClear'), message: t('settings.alerts.cloudCoverIsDownTo') },
  clouded_over: { title: t('settings.alerts.cloudedOver'), message: t('settings.alerts.cloudCoverIsUpTo') },
  client_lost: {
    title: t('settings.alerts.imagingAppStoppedChecking'),
    message: t('settings.alerts.noRequestToTheDevice'),
  },
  client_back: { title: t('settings.alerts.imagingAppIsBack'), message: t('settings.alerts.theDeviceIsBeingChecked') },
  client_disconnected: { title: t('settings.alerts.imagingAppDisconnected'), message: t('settings.alerts.theDeviceWasDisconnected') },
};

// "Dark and clear" only when sky alerts wait for darkness, as on the device.
const defaultText = (key: AlertEventKey, skyNightOnly: boolean) =>
  key === 'clear_sky' && !skyNightOnly ? { ...DEFAULT_TEXT.clear_sky, title: t('settings.alerts.skiesClear') } : DEFAULT_TEXT[key];

const VAR_HELP: Record<string, string> = {
  event: t('settings.alerts.varEvent'),
  reasons: t('settings.alerts.everyFailingRuleWithIts'),
  reasons_inline: t('settings.alerts.theSameOnOneLine'),
  reason_count: t('settings.alerts.howManyRulesAreFailing'),
  sensor: t('settings.alerts.whichSensor'),
  dew_margin_min: t('settings.alerts.dewRiskMarginSetting'),
  device: t('settings.alerts.deviceName'),
  time: t('settings.alerts.localTime'),
  date: t('settings.alerts.localDate'),
  level: t('settings.alerts.varLevel'),
  sqm: t('settings.alerts.skyQualityMagArcsec'),
  sqm_min: t('settings.alerts.sqmSafetyMinimum'),
  cloud: t('settings.alerts.cloudCover'),
  cloud_max: t('settings.alerts.cloudCoverSafetyLimit'),
  clear_below: t('settings.alerts.clearThreshold'),
  cloudy_above: t('settings.alerts.cloudedOverThreshold'),
  sky_temp: t('settings.alerts.skyTemperatureC'),
  temp: t('settings.alerts.temperatureC'),
  humidity: t('settings.alerts.humidity'),
  humidity_max: t('settings.alerts.humiditySafetyLimit'),
  dewpoint: t('settings.alerts.dewPointC'),
  dew_margin: t('settings.alerts.temperatureMinusDewPointC'),
  pressure: t('settings.alerts.pressureHpa'),
  rain_rate: t('settings.alerts.rainRateMmH'),
  wind: t('settings.alerts.windMS'),
  gust: t('settings.alerts.gustMS'),
  sun_alt: t('settings.alerts.sunAltitude'),
  silent_for: t('settings.alerts.theSilentForTimeE'),
  last_checked: t('settings.alerts.whenTheImagingAppLast'),
  client_id: t('settings.alerts.theAlpacaClientidTheImaging'),
};
// For the imaging-app events {device} is the Alpaca device, not the device name.
const CLIENT_VAR_HELP: Record<string, string> = { device: t('settings.alerts.safetyMonitorOrWeatherDevice') };
const CLIENT_EVENTS: AlertEventKey[] = ['client_lost', 'client_back', 'client_disconnected'];
const COMMON_VARS = [
  'event',
  'device',
  'time',
  'date',
  'level',
  'sqm',
  'sqm_min',
  'cloud',
  'cloud_max',
  'clear_below',
  'cloudy_above',
  'sky_temp',
  'temp',
  'humidity',
  'humidity_max',
  'dewpoint',
  'dew_margin',
  'pressure',
  'rain_rate',
  'wind',
  'gust',
  'sun_alt',
];
const EVENT_VARS: Partial<Record<AlertEventKey, string[]>> = {
  unsafe: ['reasons', 'reasons_inline', 'reason_count'],
  sensor_fault: ['sensor'],
  sensor_recovered: ['sensor'],
  dew_risk: ['dew_margin_min'],
  client_lost: ['silent_for', 'last_checked', 'client_id'],
  client_back: ['last_checked', 'client_id'],
  client_disconnected: ['client_id'],
};

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
    setTestResult({ target, type: 'pending', text: t('common.sending') });
    try {
      const before = (await fetchRecent())[0]?.id ?? 0;
      const response = await fetch(`/api/alerts/test?${query}`, { method: 'POST' });
      if (!response.ok) {
        const body = await response.json().catch(() => ({}));
        setTestResult({ target, type: 'error', text: body.error ?? t('settings.alerts.testFailed') });
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
      setTestResult({ target, type: 'error', text: t('settings.alerts.noResultFromTheDevice') });
    } catch {
      setTestResult({ target, type: 'error', text: t('settings.alerts.couldNotReachTheDevice') });
    }
  };

  // null while pending; a leading "!" marks a failure.
  const channelResult = (channel: AlertChannelName) => (record: AlertRecord) => {
    const result = record.channels[channel];
    if (!result || result.status === 'pending') return null;
    if (result.status === 'sent') return t('settings.alerts.delivered');
    return `!${result.status === 'skipped' ? t('settings.alerts.skipped') : t('settings.alerts.failed')}: ${result.detail}`;
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
      setTestResult({ target: key, type: 'pending', text: t('common.sending') });
      fetch(`/api/alerts/test?${query}`, { method: 'POST' })
        .then(async (response) => {
          const body = await response.json().catch(() => ({}));
          setTestResult(
            response.ok
              ? { target: key, type: 'success', text: t('settings.alerts.ringingPairedPhones') }
              : { target: key, type: 'error', text: body.error ?? t('settings.alerts.testFailed') },
          );
        })
        .catch(() => setTestResult({ target: key, type: 'error', text: t('settings.alerts.couldNotReachTheDevice') }));
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
          title={dirty ? t('settings.alerts.saveFirst') : undefined}
        >
          {t('settings.alerts.sendTest')}
        </ActionButton>
        {resultNote(channel)}
      </div>
    );
  };

  const pushoverOn = alerts.pushover.enabled;

  // Paused or sending is live device state, not a saved setting.
  const { schedule: shownSchedule, busy: scheduleBusy, pauseOrResume: sendPauseOrResume } = useAlertSchedule(status?.alerts);
  const pauseOrResume = async (resume: boolean) => {
    if (!(await sendPauseOrResume(resume))) showToast({ message: t('settings.alerts.couldNotReachTheDevice'), tone: 'bad' });
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
        <Field
          label={t('settings.alerts.title')}
          error={(event.title ?? '').length > 80 ? t('settings.alerts.upTo80Characters') : undefined}
        >
          <input
            class="input"
            aria-label={t('settings.alerts.alertTitle')}
            value={event.title ?? ''}
            placeholder={defaultText(key, alerts.skyNightOnly).title}
            maxLength={80}
            onFocus={track('title')}
            onKeyUp={track('title')}
            onClick={track('title')}
            onInput={(e) => set(['events', key, 'title'], (e.target as HTMLInputElement).value)}
          />
        </Field>
        <Field
          label={t('settings.alerts.message')}
          error={(event.message ?? '').length > 240 ? t('settings.alerts.upTo240Characters') : undefined}
        >
          <textarea
            class="input"
            aria-label={t('settings.alerts.alertMessage')}
            value={event.message ?? ''}
            placeholder={defaultText(key, alerts.skyNightOnly).message}
            maxLength={240}
            onFocus={track('message')}
            onKeyUp={track('message')}
            onClick={track('message')}
            onInput={(e) => set(['events', key, 'message'], (e.target as HTMLTextAreaElement).value)}
          />
        </Field>
        <div class="var-chips" aria-label={t('settings.alerts.insertAValue')}>
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
              {t('settings.alerts.useTheDefaultWording')}
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
                options={soundOptions(event.sound, t('settings.alerts.default'))}
                disabled={off}
                onChange={(v) => set(['events', key, 'sound'], v)}
              />
            ) : (
              <span />
            ))}
          <ActionButton
            onClick={() => sendEventTest(key)}
            disabled={off || event.level === 0 || entry.state === 'inactive' || testResult?.type === 'pending'}
            title={
              event.level === 0
                ? t('settings.alerts.off')
                : entry.state === 'inactive'
                  ? entry.text
                  : t('settings.alerts.sendASampleLabelAlert', { label })
            }
          >
            {t('settings.alerts.test')}
          </ActionButton>
          <ActionButton
            onClick={() => setEditing(editing === key ? null : key)}
            disabled={off}
            title={t('settings.alerts.writeYourOwnTitleAnd')}
          >
            {event.title || event.message ? t('settings.alerts.text') : t('settings.alerts.text2')}
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
  const darknessNote = location
    ? describeDarkness(location.latitude, location.longitude, alerts.nightSunAltitudeDeg, status?.sky?.sunAltitudeDeg, deviceTime(status))
    : null;
  // Counts only channels that can deliver (FR-007).
  const channelEntries = (['pushover', 'ntfy', 'webhook', 'mqtt'] as const).map((channel) => dep(`alerts.${channel}.enabled`));
  const channelsOn = channelEntries.filter((e) => e.state !== 'off').length;
  const channelCount = channelEntries.filter((e) => e.state === 'active' || e.state === 'unknown').length;
  const wakePhones = deps.get('alerts.wakePhones');

  return (
    <>
      <SettingsCard
        id="alerts"
        title={t('settings.alerts.alerts')}
        hint={t('settings.alerts.pushNotificationsSentByThe')}
        badge={
          off ? undefined : (
            <StatusBadge
              tone={channelCount === 0 ? 'warn' : 'ok'}
              label={channelCount === 0 ? t('settings.alerts.noChannels') : t('settings.alerts.channelCount', { count: channelCount })}
            />
          )
        }
      >
        <Toggle label={t('settings.alerts.sendAlerts')} checked={alerts.enabled} onChange={(v) => set(['enabled'], v)} />
        {!off && channelsOn === 0 && <Requires tone="warn">{t('settings.alerts.turnOnAChannelBelow')}</Requires>}
        {!off && channelsOn > 0 && channelCount === 0 && (
          <Requires tone="warn">{t('settings.alerts.alertsReachNowhereNoChannel')}</Requires>
        )}
        {!off && (
          <Group title={t('settings.alerts.whenToSend')}>
            <SelectInput
              value={sendMode}
              ariaLabel={t('settings.alerts.whenToSend')}
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
                {shownSchedule?.armed === false ? t('settings.alerts.resumeAlerts') : t('settings.alerts.pauseAlerts')}
              </ActionButton>
              <InfoTip text={PAUSE_HINT} />
            </div>
            {status?.alpaca?.clients && describeSilentClients(status.alpaca.clients) && (
              <Note tone="warn">{describeSilentClients(status.alpaca.clients)}</Note>
            )}
          </Group>
        )}
      </SettingsCard>

      <SettingsCard title={t('settings.alerts.notifyMeWhen')} hint={LEVEL_HINT}>
        <fieldset class="card-body" disabled={off}>
          <div class={`event-table${pushoverOn ? ' with-sound' : ''}`}>
            <div class="event-row event-table-head" aria-hidden="true">
              <span />
              <span>{t('settings.alerts.level')}</span>
              {pushoverOn && <span>{t('settings.alerts.pushoverSound')}</span>}
              <span />
              <span />
            </div>
            {eventRow('unsafe', t('settings.alerts.itTurnsUnsafe'), { hint: t('settings.alerts.listsTheReasons') })}
            {eventRow('safe', t('settings.alerts.itSSafeAgain'))}
            {eventRow('rain_started', t('settings.alerts.rainStarts'))}
            {eventRow('rain_stopped', t('settings.alerts.rainStops'))}
            {eventRow('sensor_fault', t('settings.alerts.aSensorFails'), { hint: t('settings.alerts.includesTheRg15Lens') })}
            {eventRow('sensor_recovered', t('settings.alerts.aSensorRecovers'))}
            {eventRow('dew_risk', t('settings.alerts.dewRiskWithin'), {
              threshold: (
                <NumberInput
                  min={0}
                  max={10}
                  step={0.5}
                  unit="°C"
                  ariaLabel={t('settings.alerts.dewRiskMargin')}
                  value={alerts.dewRiskMarginC}
                  disabled={off}
                  onChange={(v) => set(['dewRiskMarginC'], v)}
                />
              ),
              hint: t('settings.alerts.temperatureWithinThisMarginOf'),
            })}
            {eventRow('clear_sky', t('settings.alerts.skiesClearUpBelow'), {
              threshold: (
                <NumberInput
                  min={0}
                  max={100}
                  step={1}
                  unit="%"
                  ariaLabel={t('settings.alerts.clearBelow')}
                  value={alerts.clearSkyCloudPercent}
                  disabled={off}
                  onChange={(v) => set(['clearSkyCloudPercent'], v)}
                />
              ),
              hint: t('settings.alerts.cloudCover2'),
            })}
            {eventRow('clouded_over', t('settings.alerts.skiesCloudOverAbove'), {
              threshold: (
                <NumberInput
                  min={0}
                  max={100}
                  step={1}
                  unit="%"
                  ariaLabel={t('settings.alerts.cloudedOverAbove')}
                  value={alerts.cloudedOverCloudPercent}
                  error={err('cloudedOverCloudPercent')}
                  disabled={off}
                  onChange={(v) => set(['cloudedOverCloudPercent'], v)}
                />
              ),
              hint: t('settings.alerts.cloudCover2'),
            })}
            {eventRow('client_lost', t('settings.alerts.theImagingAppStopsChecking'), {
              hint: t('settings.alerts.noRequestReachedTheSafety'),
            })}
            {eventRow('client_back', t('settings.alerts.theImagingAppIsBack'), {
              hint: t('settings.alerts.itStartedCheckingAgainAfter'),
            })}
            {eventRow('client_disconnected', t('settings.alerts.theImagingAppDisconnects'), {
              hint: t('settings.alerts.itDisconnectedNormallyForExample'),
            })}
          </div>
          {err('cloudedOverCloudPercent') && <Note tone="bad">{err('cloudedOverCloudPercent')}</Note>}
          {wakePhones.state === 'inactive' && <DepNote entry={wakePhones} onFix={fix} prefix={t('settings.alerts.phonesWontRing')} />}
          <div class="form-grid">
            <Field
              label={t('settings.alerts.silentForSafetyMonitor')}
              error={err('clientSilentSafetySeconds')}
              hint={t('settings.alerts.howLongWithoutARequest')}
            >
              <NumberInput
                min={0.5}
                max={60}
                step={0.5}
                unit="min"
                ariaLabel={t('settings.alerts.silentForSafetyMonitor')}
                value={secondsToMinutes(alerts.clientSilentSafetySeconds ?? 120)}
                disabled={off}
                onChange={(v) => set(['clientSilentSafetySeconds'], minutesToSeconds(v))}
              />
            </Field>
            <Field
              label={t('settings.alerts.silentForWeatherDevice')}
              error={err('clientSilentWeatherSeconds')}
              hint={t('settings.alerts.imagingAppsCheckWeatherLess')}
            >
              <NumberInput
                min={0.5}
                max={60}
                step={0.5}
                unit="min"
                ariaLabel={t('settings.alerts.silentForWeatherDevice')}
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
                label={t('settings.alerts.skyAlertsOnlyWhenIt')}
                checked={alerts.skyNightOnly}
                onChange={(v) => set(['skyNightOnly'], v)}
                disabled={off}
                hint={t('settings.alerts.fromTheSunSPosition')}
              />
              <DepToggle
                entry={dep('alerts.safetyNightOnly')}
                onFix={fix}
                label={t('settings.alerts.safetyAlertsOnlyWhenIt')}
                checked={alerts.safetyNightOnly}
                onChange={(v) => set(['safetyNightOnly'], v)}
                disabled={off}
                hint={t('settings.alerts.soDawnBrighteningTheSky')}
              />
            </div>
            <SelectInput
              value={String(alerts.nightSunAltitudeDeg)}
              ariaLabel={t('settings.alerts.darkMeans')}
              disabled={off || (!alerts.skyNightOnly && !alerts.safetyNightOnly)}
              options={[
                { value: '-0.833', label: t('settings.alerts.afterSunset') },
                { value: '-12', label: t('settings.alerts.nauticalDark12') },
                { value: '-18', label: t('settings.alerts.astronomicalDark18') },
              ]}
              onChange={(v) => set(['nightSunAltitudeDeg'], parseFloat(v))}
            />
          </div>
          {darknessNote && <Note>{darknessNote}</Note>}
          <div class="form-grid">
            <Field
              label={t('settings.alerts.cooldown')}
              error={err('cooldownSeconds')}
              hint={t('settings.alerts.minimumGapBetweenAlertsOf')}
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

      <SettingsCard title={t('settings.alerts.channels')} hint={t('settings.alerts.testsUseTheSavedSettings')}>
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
                <Field label={t('settings.alerts.userKey')} error={err('pushover.userKey')} hint={t('settings.alerts.yourUserKeyTopOf')}>
                  <TextInput
                    dataField="alerts.pushover.userKey"
                    type="password"
                    value={alerts.pushover.userKey}
                    onInput={(v) => set(['pushover', 'userKey'], v)}
                  />
                </Field>
                <Field
                  label={t('settings.alerts.appToken')}
                  error={err('pushover.appToken')}
                  hint={t('settings.alerts.createAnApplicationAtPushover')}
                >
                  <TextInput type="password" value={alerts.pushover.appToken} onInput={(v) => set(['pushover', 'appToken'], v)} />
                </Field>
                <Field label={t('settings.alerts.defaultSound')} hint={t('settings.alerts.usedWhereAnEventS')}>
                  <SelectInput
                    value={alerts.pushover.sound}
                    options={soundOptions(alerts.pushover.sound, t('settings.alerts.pushoverDefault'))}
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
                <Field label={t('settings.alerts.server')} error={err('ntfy.server')}>
                  <TextInput type="url" value={alerts.ntfy.server} onInput={(v) => set(['ntfy', 'server'], v)} />
                </Field>
                <Field label={t('settings.alerts.topic')} error={err('ntfy.topic')} hint={t('settings.alerts.ntfyTopicsPublic')}>
                  <TextInput dataField="alerts.ntfy.topic" value={alerts.ntfy.topic} onInput={(v) => set(['ntfy', 'topic'], v)} />
                </Field>
                <Field label={t('settings.alerts.token')}>
                  <TextInput
                    type="password"
                    value={alerts.ntfy.token}
                    placeholder={t('settings.alerts.optional')}
                    onInput={(v) => set(['ntfy', 'token'], v)}
                  />
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
            label={t('settings.alerts.webhook')}
            checked={alerts.webhook.enabled}
            onChange={(v) => set(['webhook', 'enabled'], v)}
            hint={t('settings.alerts.postsJsonDeviceEventTitle')}
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
                <Field label={t('settings.alerts.authorizationHeader')}>
                  <TextInput
                    type="password"
                    value={alerts.webhook.authHeader}
                    placeholder={t('settings.alerts.optional')}
                    onInput={(v) => set(['webhook', 'authHeader'], v)}
                  />
                </Field>
              </div>
              {alerts.webhook.url.startsWith('https://') && (
                <Toggle
                  label={t('settings.alerts.skipCertificateChecks')}
                  checked={alerts.webhook.insecureTls}
                  onChange={(v) => set(['webhook', 'insecureTls'], v)}
                  hint={t('settings.alerts.onlyForASelfSigned')}
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
            hint={t('settings.alerts.publishesEachAlertToTopic', { topic: config.mqtt.topic })}
          />
          {testButton('mqtt')}
        </Group>
      </SettingsCard>
    </>
  );
};

export default AlertsTab;
