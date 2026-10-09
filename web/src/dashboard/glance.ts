import type { AlertSchedule, AlpacaClientState, Config, SensorData, SensorHealth, SystemStatus } from '../types';
import { REASONS, type DepEntry, type EffectiveReport } from '../lib/settingsDeps';
import { describeSchedule } from '../components/settings/alertSchedule';
import { sensorName, type SensorId } from '../lib/sensorNames';
import { describeClient, type ClientView } from '../lib/alpacaClients';
import { formatAgeMs, formatAgo } from '../i18n/format';
import { deviceText } from '../i18n/deviceMessage';
import { languageProblem } from '../i18n/loader';
import { t, type MessageKey } from '../i18n';
import { appHref } from '../lib/appHref';

// What the Status card shows (specs/025 content, specs/026 presentation). One
// function decides what shows and in which order; StatusCard only renders it.
// Each item's id is an entry of inventory.json. Every item is short: a name
// (`label`), a one- or two-word state for its pill, and the detail behind "?".

export type Severity = 'ok' | 'note' | 'problem';

export type GlanceFix = { label: string; href: string } | { label: string; action: 'resume' | 'acknowledge' };

export interface GlanceItem {
  id: string; // inventory.json entry
  severity: Severity;
  priority: number; // the spec's Edge Cases order
  label: string; // the tile label or row name, e.g. "IR sky sensor"
  state: string; // the pill, e.g. "Error"
  sub?: string; // a tile's one line under the pill, e.g. "Checked 3 s ago"
  detail?: string; // behind "?": the consequence or the list behind a summary
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

type Sensor = SensorId;
export type { Sensor };

// What a sensor's failure costs.
const EFFECT: Record<Sensor, MessageKey> = {
  light: 'status.effect.light',
  infrared: 'status.effect.infrared',
  environment: 'status.effect.environment',
  rain: 'status.effect.rain',
  wind: 'status.effect.wind',
  gps: 'status.effect.gps',
};

export const sensorEffect = (sensor: Sensor) => t(EFFECT[sensor]);

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
  language: 6,
  'clock-location': 7,
  'phone-alarm': 8,
};

interface Parts {
  label: string;
  state: string;
  sub?: string;
  detail?: string;
  fix?: GlanceFix;
}

const item = (id: string, severity: Severity, parts: Parts): GlanceItem => ({ id, severity, priority: PRIORITY[id], ...parts });

export const healthWords = (health: SensorHealth) =>
  health === 'missing' ? t('settings.sensors.notResponding') : health === 'stale' ? t('system.stale') : t('system.error');

// Settings (spec 020) whose being inactive matters to alerts or safety (FR-011).
const ALERT_CHANNEL_DEPS = new Set(['D-01', 'D-02', 'D-03', 'D-04']);
const ALERT_SAFETY_DEPS = new Set(['D-05', 'D-06', 'D-07', 'D-08', 'D-09', 'D-10', 'D-11', 'D-20', 'D-31', 'D-35', 'D-37']);

const settingsLink = (tab: string, anchor?: string): GlanceFix => ({
  label: t('glance.openSettings'),
  href: appHref(`/settings?tab=${tab}${anchor ? `&section=${anchor}` : ''}`),
});

const freshness = ({ sensors, connected, quiet }: GlanceInput): GlanceItem => {
  const label = t('status.data');
  if (!connected) return item('freshness', 'problem', { label, state: t('status.offline'), detail: t('status.offlineHint') });
  if (quiet) return item('freshness', 'problem', { label, state: t('status.noUpdates'), detail: t('status.noUpdatesHint') });
  if (sensors?.dataStale)
    return item('freshness', 'problem', {
      label,
      state: t('system.stale'),
      sub: t('status.ageOld', { age: formatAgeMs(sensors.dataAgeMs) }),
      detail: t('status.staleHint'),
    });
  return item('freshness', 'ok', { label, state: t('dashboard.live') });
};

const verdict = ({ sensors }: GlanceInput): GlanceItem | null => {
  const safety = sensors?.safety;
  if (!safety) return null;
  const label = t('status.safety');
  if (safety.safe) return item('safety-verdict', 'ok', { label, state: t('safetyCard.safe') });
  const reasons = safety.reasons.map((reason) => deviceText(reason)).join(', ');
  const waiting = safety.rawSafe && safety.secondsUntilSafe > 0;
  const sub = waiting ? t('status.safeIn', { seconds: safety.secondsUntilSafe }) : reasons || undefined;
  return item('safety-verdict', 'problem', { label, state: t('status.unsafe'), sub });
};

const alertsItem = ({ config, schedule }: GlanceInput): GlanceItem | null => {
  if (!config?.alerts) return null;
  const label = t('status.alerts');
  if (!config.alerts.enabled)
    return item('alerts-state', 'note', { label, state: t('status.off'), sub: t('status.nothingSent'), fix: settingsLink('alerts') });
  if (!schedule) return null;
  if (schedule.armed) return item('alerts-state', 'ok', { label, state: t('status.sending') });
  const waiting = schedule.reason === 'waiting-for-client' || schedule.reason === 'client-disconnected';
  const sub = describeSchedule(schedule);
  return waiting
    ? item('alerts-state', 'problem', { label, state: t('status.waiting'), sub, detail: t('glance.alertsWaitingDetail') })
    : item('alerts-state', 'problem', { label, state: t('status.paused'), sub, fix: { label: t('alertsBell.resume'), action: 'resume' } });
};

// The device reports its reason in English; Settings shows the translated text (spec 020).
const reasonText = (entry: DepEntry) => (entry.reason && REASONS[entry.reason]?.text) || entry.text || '';

const inactive = (effective: EffectiveReport | null, ids: Set<string>): DepEntry[] =>
  (effective?.settings ?? []).filter((entry) => entry.state === 'inactive' && ids.has(entry.id));

const sendModeItem = ({ config, effective, status }: GlanceInput): GlanceItem | null => {
  if (config?.alerts?.sendMode !== 'whileConnected') return null;
  const alpacaOff = status?.alpaca ? !status.alpaca.enabled : effective?.settings.some((e) => e.id === 'D-12' && e.state === 'inactive');
  if (!alpacaOff) return null;
  return item('alerts-mode-not-in-effect', 'note', {
    label: t('status.sendMode'),
    state: t('status.notInEffect'),
    detail: t('status.sendModeHint'),
    fix: settingsLink('safety', 'alpaca'),
  });
};

const channelsItem = ({ config, effective }: GlanceInput): GlanceItem | null => {
  const alerts = config?.alerts;
  if (!alerts?.enabled) return null;
  const label = t('status.channels');
  const on = (['pushover', 'ntfy', 'webhook', 'mqtt'] as const).filter((channel) => alerts[channel]?.enabled);
  if (!on.length)
    return item('no-channel', 'problem', { label, state: t('status.noneOn'), detail: t('status.noneOnHint'), fix: settingsLink('alerts') });
  const blocked = inactive(effective, ALERT_CHANNEL_DEPS);
  const working = on.filter((channel) => !blocked.some((entry) => entry.setting === `alerts.${channel}.enabled`));
  if (working.length || !blocked.length) return null;
  return item('no-channel', 'problem', { label, state: t('status.cantSend'), detail: reasonText(blocked[0]), fix: settingsLink('alerts') });
};

const DEVICES = [
  ['safetymonitor', () => t('alpaca.safetyMonitor')],
  ['observingconditions', () => t('alpaca.weatherDevice')],
] as const;

const SEVERITY: Record<ClientView['tone'], Severity> = { ok: 'ok', warn: 'problem', muted: 'note' };

const imagingAppItems = ({ config, status }: GlanceInput): GlanceItem[] => {
  const clients = status?.alpaca?.clients;
  if (!clients) return [];
  const needed = config?.alerts?.enabled && config.alerts.sendMode === 'whileConnected';
  const seen = (state: AlpacaClientState) => state.connected || state.watching || state.silent || state.lastCheckedAgeMs !== null;
  return DEVICES.filter(([device]) => needed || seen(clients[device])).map(([device, name]) => {
    const state = clients[device];
    const view = describeClient(state);
    return item('imaging-app', SEVERITY[view.tone], {
      label: t('status.imagingApp'),
      state: view.state,
      sub: view.checked ? t('status.deviceChecked', { device: name(), checked: view.checked }) : name(),
      detail: state.silent ? t('status.goneQuietHint') : undefined,
    });
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
      item('sensor-faults', 'problem', {
        label: sensorName(sensor),
        state: healthWords(health.health),
        detail: sensorEffect(sensor) + age,
        fix: settingsLink('sensors'),
      }),
    ];
  });

const settingsItem = ({ effective }: GlanceInput): GlanceItem | null => {
  const entries = inactive(effective, ALERT_SAFETY_DEPS);
  if (!entries.length) return null;
  return item('settings-not-in-effect', 'note', {
    label: t('status.settings'),
    state: t('status.notInEffectCount', { count: entries.length }),
    detail: entries.map(reasonText).join('; '),
    fix: settingsLink('alerts'),
  });
};

const clockItem = ({ sensors, status }: GlanceInput): GlanceItem | null => {
  if (sensors && !sensors.timeValid)
    return item('clock-location', 'problem', {
      label: t('status.clock'),
      state: t('status.notSet'),
      detail: t('status.clockHint'),
      fix: settingsLink('time'),
    });
  if (status?.sky && (status.sky.locationSource === 'none' || !status.sky.nightKnown))
    return item('clock-location', 'problem', {
      label: t('status.location'),
      state: t('dashboard.unknown'),
      detail: t('status.locationHint'),
      fix: settingsLink('time'),
    });
  return null;
};

const languageItem = (): GlanceItem | null => {
  const { kind, detail } = languageProblem();
  if (kind === 'none' || kind === 'otherVersion') return null;
  const downloading = kind === 'downloading';
  return item('language', downloading ? 'note' : 'problem', {
    label: t('language.label'),
    state: downloading ? t('status.downloading') : t('status.notLoaded'),
    detail: detail ? deviceText(detail) : t('status.languageHint'),
    fix: settingsLink('device', 'language'),
  });
};

const alarmItem = ({ status }: GlanceInput): GlanceItem | null =>
  status?.ble?.alarm?.active
    ? item('phone-alarm', 'problem', {
        label: t('status.phoneAlarm'),
        state: t('status.ringing'),
        fix: { label: t('glance.acknowledge'), action: 'acknowledge' },
      })
    : null;

/** Everything the Status card shows, most important first. */
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
    languageItem(),
    clockItem(input),
    alarmItem(input),
  ].filter((entry): entry is GlanceItem => Boolean(entry));
  const rank = { problem: 0, note: 1, ok: 2 } as const;
  return items.sort((a, b) => a.priority - b.priority || rank[a.severity] - rank[b.severity]);
};

/** How many things need a look: every item that isn't ok. */
export const toCheck = (items: GlanceItem[]) => items.filter((entry) => entry.severity !== 'ok');

/** A screen-reader line for an item, e.g. "IR sky sensor: Error". */
export const spoken = (entry: GlanceItem) => `${entry.label}: ${entry.state}`;
