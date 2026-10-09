import type { AlertSchedule, AlpacaClientState, Config, SensorData, SensorHealth, SystemStatus } from '../types';
import { REASONS, type DepEntry, type EffectiveReport } from '../lib/settingsDeps';
import { describeSchedule } from '../components/settings/alertSchedule';
import { describeClient } from '../lib/alpacaClients';
import { formatAgeMs, formatAgo } from '../i18n/format';
import { deviceText } from '../i18n/deviceMessage';
import { t, type MessageKey } from '../i18n';

// The at-a-glance area (specs/025): what the dashboard says before anything
// is clicked. One function decides what shows and in which order; the
// component only renders it. Each item's id is an entry of inventory.json.

export type Severity = 'ok' | 'note' | 'problem';

export type GlanceFix = { label: string; href: string } | { label: string; action: 'resume' | 'acknowledge' };

export interface GlanceItem {
  id: string; // inventory.json entry
  severity: Severity;
  priority: number; // the spec's Edge Cases order
  text: string;
  detail?: string; // the consequence, or the list behind a summary
  fix?: GlanceFix;
}

export interface GlanceInput {
  sensors: SensorData | null;
  status: SystemStatus | null;
  config: Config | null;
  effective: EffectiveReport | null;
  connected: boolean; // the stream is open
  quiet: boolean; // no message for longer than the quiet threshold
  schedule: AlertSchedule | null; // live alert schedule (useAlertSchedule)
}

type Sensor = 'light' | 'infrared' | 'environment' | 'rain' | 'wind' | 'gps';

// Each sensor's name and what its failure costs.
const SENSORS: Record<Sensor, [MessageKey, MessageKey]> = {
  light: ['glance.sensor.light', 'glance.effect.light'],
  infrared: ['glance.sensor.infrared', 'glance.effect.infrared'],
  environment: ['glance.sensor.environment', 'glance.effect.environment'],
  rain: ['settings.sensors.rainSensor', 'glance.effect.rain'],
  wind: ['settings.sensors.anemometer', 'glance.effect.wind'],
  gps: ['glance.sensor.gps', 'glance.effect.gps'],
};

export type { Sensor };

export const sensorEffect = (sensor: Sensor) => t(SENSORS[sensor][1]);

// The spec's Edge Cases order, by item.
const PRIORITY: Record<string, number> = {
  freshness: 1,
  'safety-verdict': 2,
  'alerts-state': 3,
  'alerts-mode-not-in-effect': 3,
  'no-channel': 3,
  'imaging-app': 4,
  'sensor-faults': 5,
  'settings-not-in-effect': 6,
  'clock-location': 7,
  'phone-alarm': 8,
};

const item = (id: string, severity: Severity, text: string, detail?: string, fix?: GlanceFix): GlanceItem => ({
  id,
  severity,
  priority: PRIORITY[id],
  text,
  detail,
  fix,
});

export const healthWords = (health: SensorHealth) =>
  health === 'missing' ? t('settings.sensors.notResponding') : health === 'stale' ? t('system.stale') : t('system.error');

// Settings (spec 020) whose being inactive matters to alerts or safety (FR-011).
const ALERT_CHANNEL_DEPS = new Set(['D-01', 'D-02', 'D-03', 'D-04']);
const ALERT_SAFETY_DEPS = new Set(['D-05', 'D-06', 'D-07', 'D-08', 'D-09', 'D-10', 'D-11', 'D-20', 'D-31', 'D-35', 'D-37']);

const freshness = ({ sensors, connected, quiet }: GlanceInput): GlanceItem => {
  if (!connected) return item('freshness', 'problem', t('glance.disconnected'), t('glance.disconnectedDetail'));
  if (quiet) return item('freshness', 'problem', t('glance.updatesStopped'), t('glance.updatesStoppedDetail'));
  if (sensors?.dataStale)
    return item('freshness', 'problem', t('glance.stale', { age: formatAgeMs(sensors.dataAgeMs) }), t('glance.staleDetail'));
  return item('freshness', 'ok', t('dashboard.live'));
};

const verdict = ({ sensors }: GlanceInput): GlanceItem | null => {
  const safety = sensors?.safety;
  if (!safety) return null;
  if (safety.safe) return item('safety-verdict', 'ok', t('safetyCard.safe'));
  const reasons = safety.reasons.map((reason) => deviceText(reason)).join(', ');
  const waiting = safety.rawSafe && safety.secondsUntilSafe > 0;
  const text = waiting ? t('glance.safeIn', { seconds: safety.secondsUntilSafe }) : t('glance.unsafe');
  return item('safety-verdict', 'problem', text, reasons || undefined);
};

const alertsItem = ({ config, schedule }: GlanceInput): GlanceItem | null => {
  if (!config?.alerts) return null;
  if (!config.alerts.enabled) return item('alerts-state', 'note', t('glance.alertsOff'), undefined, settingsLink('alerts'));
  if (!schedule) return null;
  const text = describeSchedule(schedule);
  if (schedule.armed) return item('alerts-state', 'ok', text);
  const waiting = schedule.reason === 'waiting-for-client' || schedule.reason === 'client-disconnected';
  return waiting
    ? item('alerts-state', 'problem', text, t('glance.alertsWaitingDetail'), { label: t('glance.openAlpaca'), href: '#/alpaca' })
    : item('alerts-state', 'problem', text, undefined, { label: t('alertsBell.resume'), action: 'resume' });
};

const settingsLink = (tab: string, anchor?: string) => ({
  label: t('glance.openSettings'),
  href: `#/settings?tab=${tab}${anchor ? `&section=${anchor}` : ''}`,
});

// The device reports its reason in English; Settings shows the translated text (spec 020).
const reasonText = (entry: DepEntry) => (entry.reason && REASONS[entry.reason]?.text) || entry.text || '';

const inactive = (effective: EffectiveReport | null, ids: Set<string>): DepEntry[] =>
  (effective?.settings ?? []).filter((entry) => entry.state === 'inactive' && ids.has(entry.id));

const sendModeItem = ({ config, effective, status }: GlanceInput): GlanceItem | null => {
  if (config?.alerts?.sendMode !== 'whileConnected') return null;
  const alpacaOff = status?.alpaca ? !status.alpaca.enabled : effective?.settings.some((e) => e.id === 'D-12' && e.state === 'inactive');
  if (!alpacaOff) return null;
  return item('alerts-mode-not-in-effect', 'note', t('glance.sendModeNotInEffect'), undefined, settingsLink('safety', 'alpaca'));
};

const channelsItem = ({ config, effective }: GlanceInput): GlanceItem | null => {
  const alerts = config?.alerts;
  if (!alerts?.enabled) return null;
  const on = (['pushover', 'ntfy', 'webhook', 'mqtt'] as const).filter((channel) => alerts[channel]?.enabled);
  if (!on.length) return item('no-channel', 'problem', t('glance.noChannelOn'), undefined, settingsLink('alerts'));
  const blocked = inactive(effective, ALERT_CHANNEL_DEPS);
  const working = on.filter((channel) => !blocked.some((entry) => entry.setting === `alerts.${channel}.enabled`));
  if (working.length || !blocked.length) return null;
  return item('no-channel', 'problem', t('glance.noChannel'), reasonText(blocked[0]), settingsLink('alerts'));
};

const DEVICES = [
  ['safetymonitor', () => t('settings.alertSchedule.safetyMonitor')],
  ['observingconditions', () => t('glance.weatherDevice')],
] as const;

const imagingAppItems = ({ config, status }: GlanceInput): GlanceItem[] => {
  const clients = status?.alpaca?.clients;
  if (!clients) return [];
  const needed = config?.alerts?.enabled && config.alerts.sendMode === 'whileConnected';
  const seen = (state: AlpacaClientState) => state.connected || state.watching || state.silent || state.lastCheckedAgeMs !== null;
  return DEVICES.filter(([device]) => needed || seen(clients[device])).map(([device, name]) => {
    const state = clients[device];
    const { text } = describeClient(state);
    return item(
      'imaging-app',
      state.silent ? 'problem' : state.connected || state.watching ? 'ok' : 'note',
      t('glance.imagingApp', { device: name(), state: text }),
      state.silent ? t('glance.imagingAppSilentDetail') : undefined,
    );
  });
};

// Which sensors the dashboard expects (research D6): light and IR always;
// the BME280 once it has worked; rain, wind and GPS when switched on.
export const expectedSensors = (status: SystemStatus | null, config: Config | null): Sensor[] => {
  const environment = status?.sensors.environment;
  const list: Sensor[] = ['light', 'infrared'];
  if (environment && environment.status !== 'missing') list.push('environment');
  if (config?.rain?.enabled) list.push('rain');
  if (config?.wind?.enabled) list.push('wind');
  if (config?.gps?.enabled) list.push('gps');
  return list;
};

const sensorHealth = (sensor: Sensor, input: GlanceInput): { health: SensorHealth; ageMs?: number } | null => {
  const entry = input.status?.sensors[sensor];
  if (entry) return { health: entry.status, ageMs: entry.ageMs };
  const reading = input.sensors?.[sensor] as { status?: SensorHealth; ageMs?: number } | undefined;
  return reading?.status ? { health: reading.status, ageMs: reading.ageMs } : null;
};

const sensorItems = (input: GlanceInput): GlanceItem[] =>
  expectedSensors(input.status, input.config).flatMap((sensor) => {
    const health = sensorHealth(sensor, input);
    if (!health || health.health === 'ok') return [];
    const age = health.ageMs ? ` ${t('glance.lastReading', { ago: formatAgo(health.ageMs) })}` : '';
    const [name, effect] = SENSORS[sensor];
    const text = t('glance.sensorFault', { sensor: t(name), state: healthWords(health.health) });
    return [item('sensor-faults', 'problem', text, t(effect) + age, settingsLink('sensors'))];
  });

const settingsItem = ({ effective }: GlanceInput): GlanceItem | null => {
  const entries = inactive(effective, ALERT_SAFETY_DEPS);
  if (!entries.length) return null;
  const text = t('glance.settingsNotInEffect', { count: entries.length });
  return item('settings-not-in-effect', 'note', text, entries.map(reasonText).join(' · '), settingsLink('alerts'));
};

const clockItem = ({ sensors, status }: GlanceInput): GlanceItem | null => {
  if (sensors && !sensors.timeValid)
    return item('clock-location', 'problem', t('glance.clockNotSet'), t('glance.clockNotSetDetail'), settingsLink('time'));
  if (status?.sky && (status.sky.locationSource === 'none' || !status.sky.nightKnown))
    return item('clock-location', 'problem', t('glance.noLocation'), t('glance.noLocationDetail'), settingsLink('time'));
  return null;
};

const alarmItem = ({ status }: GlanceInput): GlanceItem | null =>
  status?.ble?.alarm?.active
    ? item('phone-alarm', 'problem', t('glance.phoneAlarm'), undefined, { label: t('glance.acknowledge'), action: 'acknowledge' })
    : null;

/** Everything the area shows, most important first. */
export const glanceItems = (input: GlanceInput): GlanceItem[] => {
  const items = [
    freshness(input),
    verdict(input),
    alertsItem(input),
    sendModeItem(input),
    channelsItem(input),
    ...imagingAppItems(input),
    ...sensorItems(input),
    settingsItem(input),
    clockItem(input),
    alarmItem(input),
  ].filter((entry): entry is GlanceItem => Boolean(entry));
  const rank = { problem: 0, note: 1, ok: 2 } as const;
  return items.sort((a, b) => a.priority - b.priority || rank[a.severity] - rank[b.severity]);
};

/** The calm line when nothing is wrong: "Live · Safe · Sending alerts". */
export const healthyLine = (items: GlanceItem[]) =>
  items
    .filter((entry) => entry.severity === 'ok')
    .map((entry) => entry.text.replace(/[.。]$/, ''))
    .join(' · ');
