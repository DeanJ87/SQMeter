import type { AlertSchedule, AlpacaClientState, Config, SensorData, SensorHealth, SystemStatus } from '../types';
import { REASONS, type DepEntry, type EffectiveReport } from '../lib/settingsDeps';
import { describeSchedule } from '../components/settings/alertSchedule';
import { describeClient } from '../lib/alpacaClients';
import { formatAgeMs, formatAgo } from '../i18n/format';
import { deviceText } from '../i18n/deviceMessage';
import { t } from '../i18n';

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

const SENSOR_NAME: Record<Sensor, () => string> = {
  light: () => t('glance.sensor.light'),
  infrared: () => t('glance.sensor.infrared'),
  environment: () => t('glance.sensor.environment'),
  rain: () => t('glance.sensor.rain'),
  wind: () => t('glance.sensor.wind'),
  gps: () => t('glance.sensor.gps'),
};

export type { Sensor };

const SENSOR_EFFECT: Record<Sensor, () => string> = {
  light: () => t('glance.effect.light'),
  infrared: () => t('glance.effect.infrared'),
  environment: () => t('glance.effect.environment'),
  rain: () => t('glance.effect.rain'),
  wind: () => t('glance.effect.wind'),
  gps: () => t('glance.effect.gps'),
};

export const sensorEffect = (sensor: Sensor) => SENSOR_EFFECT[sensor]();

export const healthWords = (health: SensorHealth) =>
  health === 'missing' ? t('glance.health.missing') : health === 'stale' ? t('glance.health.stale') : t('glance.health.error');

// Settings (spec 020) whose being inactive matters to alerts or safety (FR-011).
const ALERT_CHANNEL_DEPS = new Set(['D-01', 'D-02', 'D-03', 'D-04']);
const ALERT_SAFETY_DEPS = new Set(['D-05', 'D-06', 'D-07', 'D-08', 'D-09', 'D-10', 'D-11', 'D-20', 'D-31', 'D-35', 'D-37']);

const freshness = ({ sensors, connected, quiet }: GlanceInput): GlanceItem => {
  if (!connected)
    return { id: 'freshness', severity: 'problem', priority: 1, text: t('glance.disconnected'), detail: t('glance.disconnectedDetail') };
  if (quiet)
    return {
      id: 'freshness',
      severity: 'problem',
      priority: 1,
      text: t('glance.updatesStopped'),
      detail: t('glance.updatesStoppedDetail'),
    };
  if (sensors?.dataStale)
    return {
      id: 'freshness',
      severity: 'problem',
      priority: 1,
      text: t('glance.stale', { age: formatAgeMs(sensors.dataAgeMs) }),
      detail: t('glance.staleDetail'),
    };
  return { id: 'freshness', severity: 'ok', priority: 1, text: t('glance.live') };
};

const verdict = ({ sensors }: GlanceInput): GlanceItem | null => {
  const safety = sensors?.safety;
  if (!safety) return null;
  if (safety.safe) return { id: 'safety-verdict', severity: 'ok', priority: 2, text: t('glance.safe') };
  const reasons = safety.reasons.map((reason) => deviceText(reason)).join(', ');
  const waiting = safety.rawSafe && safety.secondsUntilSafe > 0;
  return {
    id: 'safety-verdict',
    severity: 'problem',
    priority: 2,
    text: waiting ? t('glance.safeIn', { seconds: safety.secondsUntilSafe }) : t('glance.unsafe'),
    detail: reasons || undefined,
  };
};

const alertsItem = ({ config, schedule }: GlanceInput): GlanceItem | null => {
  if (!config?.alerts) return null;
  if (!config.alerts.enabled)
    return { id: 'alerts-state', severity: 'note', priority: 3, text: t('glance.alertsOff'), fix: settingsLink('alerts') };
  if (!schedule) return null;
  const text = describeSchedule(schedule);
  if (schedule.armed) return { id: 'alerts-state', severity: 'ok', priority: 3, text };
  const waiting = schedule.reason === 'waiting-for-client' || schedule.reason === 'client-disconnected';
  return {
    id: 'alerts-state',
    severity: 'problem',
    priority: 3,
    text,
    detail: waiting ? t('glance.alertsWaitingDetail') : undefined,
    fix: waiting ? { label: t('glance.openAlpaca'), href: '#/alpaca' } : { label: t('alertsBell.resume'), action: 'resume' },
  };
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
  return {
    id: 'alerts-mode-not-in-effect',
    severity: 'note',
    priority: 3,
    text: t('glance.sendModeNotInEffect'),
    fix: settingsLink('safety', 'alpaca'),
  };
};

const channelsItem = ({ config, effective }: GlanceInput): GlanceItem | null => {
  const alerts = config?.alerts;
  if (!alerts?.enabled) return null;
  const on = (['pushover', 'ntfy', 'webhook', 'mqtt'] as const).filter((channel) => alerts[channel]?.enabled);
  if (!on.length) return { id: 'no-channel', severity: 'problem', priority: 3, text: t('glance.noChannelOn'), fix: settingsLink('alerts') };
  const blocked = inactive(effective, ALERT_CHANNEL_DEPS);
  const working = on.filter((channel) => !blocked.some((entry) => entry.setting === `alerts.${channel}.enabled`));
  if (working.length || !blocked.length) return null;
  return {
    id: 'no-channel',
    severity: 'problem',
    priority: 3,
    text: t('glance.noChannel'),
    detail: reasonText(blocked[0]),
    fix: settingsLink('alerts'),
  };
};

const DEVICES = [
  ['safetymonitor', () => t('glance.safetyMonitor')],
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
    return {
      id: 'imaging-app',
      severity: state.silent ? 'problem' : state.connected || state.watching ? 'ok' : 'note',
      priority: 4,
      text: t('glance.imagingApp', { device: name(), state: text }),
      detail: state.silent ? t('glance.imagingAppSilentDetail') : undefined,
    } satisfies GlanceItem;
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
    return [
      {
        id: 'sensor-faults',
        severity: 'problem',
        priority: 5,
        text: t('glance.sensorFault', { sensor: SENSOR_NAME[sensor](), state: healthWords(health.health) }),
        detail: SENSOR_EFFECT[sensor]() + age,
        fix: settingsLink('sensors'),
      } satisfies GlanceItem,
    ];
  });

const settingsItem = ({ effective }: GlanceInput): GlanceItem | null => {
  const entries = inactive(effective, ALERT_SAFETY_DEPS);
  if (!entries.length) return null;
  return {
    id: 'settings-not-in-effect',
    severity: 'note',
    priority: 6,
    text: t('glance.settingsNotInEffect', { count: entries.length }),
    detail: entries.map(reasonText).join(' · '),
    fix: settingsLink('alerts'),
  };
};

const clockItem = ({ sensors, status }: GlanceInput): GlanceItem | null => {
  if (sensors && !sensors.timeValid)
    return {
      id: 'clock-location',
      severity: 'problem',
      priority: 7,
      text: t('glance.clockNotSet'),
      detail: t('glance.clockNotSetDetail'),
      fix: settingsLink('time'),
    };
  if (status?.sky && (status.sky.locationSource === 'none' || !status.sky.nightKnown))
    return {
      id: 'clock-location',
      severity: 'problem',
      priority: 7,
      text: t('glance.noLocation'),
      detail: t('glance.noLocationDetail'),
      fix: settingsLink('time'),
    };
  return null;
};

const alarmItem = ({ status }: GlanceInput): GlanceItem | null =>
  status?.ble?.alarm?.active
    ? {
        id: 'phone-alarm',
        severity: 'problem',
        priority: 8,
        text: t('glance.phoneAlarm'),
        fix: { label: t('glance.acknowledge'), action: 'acknowledge' },
      }
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
  ].filter((item): item is GlanceItem => Boolean(item));
  const rank = { problem: 0, note: 1, ok: 2 } as const;
  return items.sort((a, b) => a.priority - b.priority || rank[a.severity] - rank[b.severity]);
};

/** The calm line when nothing is wrong: "Live · Safe · Sending alerts". */
export const healthyLine = (items: GlanceItem[]) =>
  items
    .filter((item) => item.severity === 'ok')
    .map((item) => item.text.replace(/[.。]$/, ''))
    .join(' · ');
