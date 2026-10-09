import type { AlertEventKey, Config } from '../types';
import { t } from '../i18n';

// Whether each setting that needs another setting (or hardware, or the
// network) is in effect - the device's rules from lib/SettingsDeps, mirrored
// so Settings can preview unsaved changes. The device's own report
// (GET /api/settings/effective) wins whenever the form is clean;
// test/fixtures/settings-deps/cases.json holds both to the same answers
// (specs/020-settings-dependencies).

export type DepState = 'off' | 'active' | 'inactive' | 'unknown';
export type Unmet = 'inactive' | 'fail-safe';

export interface DepFacts {
  wifiConnected: boolean;
  mqttConnected: boolean;
  clockSet: boolean;
  gpsRunning: boolean;
  gpsFix: boolean;
  bluetoothBuild: boolean;
  bluetoothRunning: boolean;
  pairedPhones: number;
  lightDetected: boolean;
  infraredDetected: boolean;
  environmentDetected: boolean;
}

export interface DepEntry {
  id: string;
  setting: string;
  state: DepState;
  reason?: string;
  text?: string;
  fix?: string; // "tab#anchor" or "restart"
  unmet?: Unmet;
  neutral?: boolean;
  // While the setting is off: what would make it inactive if it were
  // switched on (local evaluation only; the device doesn't report it).
  blockedBy?: { id: string; reason: string; text: string; fix: string };
}

export interface EffectiveReport {
  facts: DepFacts;
  settings: DepEntry[];
}

// Same codes, texts and fix targets as lib/SettingsDeps/catalogue.json (a
// test compares them).
export const REASONS: Record<string, { text: string; fix: string }> = {
  'alerts-off': { text: t('settingsDeps.alertsAreOff'), fix: 'alerts#alerts' },
  'mqtt-off': { text: t('settingsDeps.mqttIsOff'), fix: 'network#mqtt' },
  'mqtt-disconnected': { text: t('settingsDeps.brokerNotConnected'), fix: 'network#mqtt' },
  'wifi-disconnected': { text: t('settingsDeps.notConnectedToWifi'), fix: 'network#wifi' },
  'rain-off': { text: t('settingsDeps.rainSensorIsOff'), fix: 'sensors#rain' },
  'wind-off': { text: t('settingsDeps.anemometerIsOff'), fix: 'sensors#wind' },
  'gps-off': { text: t('settingsDeps.gpsIsOff'), fix: 'time#time-sources' },
  'gps-restart': { text: t('settingsDeps.gpsStartsAfterARestart'), fix: 'restart' },
  'gps-no-fix': { text: t('settingsDeps.noGpsFixUsingThe'), fix: 'time#location' },
  'light-missing': { text: t('settingsDeps.tsl2591NotDetected'), fix: 'sensors#sky-sensors' },
  'infrared-missing': { text: t('settingsDeps.mlx90614NotDetected'), fix: 'sensors#sky-sensors' },
  'environment-missing': { text: t('settingsDeps.bme280NotDetected'), fix: 'sensors#sky-sensors' },
  'location-unknown': { text: t('settingsDeps.needsYourLocation'), fix: 'time#location' },
  'alpaca-off': { text: t('settingsDeps.alpacaIsOff'), fix: 'safety#alpaca' },
  'ble-build': { text: t('settingsDeps.needsTheBluetoothFirmwareBuild'), fix: 'device#ble' },
  'ble-off': { text: t('settingsDeps.bluetoothIsOff'), fix: 'device#ble' },
  'ble-restart': { text: t('settingsDeps.bluetoothStartsAfterARestart'), fix: 'restart' },
  'no-passkey': { text: t('settingsDeps.noPairingPasskeySet'), fix: 'device#ble' },
  'no-phones': { text: t('settingsDeps.noPhonePaired'), fix: 'device#ble' },
  'clock-unset': { text: t('settingsDeps.theDeviceDoesnTKnow'), fix: 'time#time-sources' },
  'ota-no-password': { text: t('settingsDeps.setAnUploadPassword'), fix: 'device#security' },
};

// Label for the one-click fix of each reason.
export const FIX_LABEL: Record<string, string> = {
  'alerts-off': t('settingsDeps.turnOnAlerts'),
  'mqtt-off': t('settingsDeps.turnOnMqtt'),
  'mqtt-disconnected': t('settingsDeps.mqttSettings'),
  'wifi-disconnected': t('settingsDeps.wifiSettings'),
  'rain-off': t('settingsDeps.turnOn'),
  'wind-off': t('settingsDeps.turnOn'),
  'gps-off': t('settingsDeps.turnOnGps'),
  'gps-restart': t('settingsDeps.restart'),
  'gps-no-fix': t('settingsDeps.location'),
  'light-missing': t('settingsDeps.skySensors'),
  'infrared-missing': t('settingsDeps.skySensors'),
  'environment-missing': t('settingsDeps.skySensors'),
  'location-unknown': t('settingsDeps.setLocation'),
  'alpaca-off': t('settingsDeps.turnOnAlpaca'),
  'ble-build': t('settingsDeps.bluetooth'),
  'ble-off': t('settingsDeps.turnOnBluetooth'),
  'ble-restart': t('settingsDeps.restart'),
  'no-passkey': t('settingsDeps.setAPasskey'),
  'no-phones': t('settingsDeps.pairAPhone'),
  'clock-unset': t('settingsDeps.timeSources'),
  'ota-no-password': t('settingsDeps.setPassword'),
};

// One link of a dependency chain: met, or the catalogue ID and reason that
// make the setting inactive. `met === null`: unknown (no facts yet).
type Link = [id: string, reason: string, met: boolean | null];

const EVENT_KEYS: AlertEventKey[] = [
  'unsafe',
  'safe',
  'rain_started',
  'rain_stopped',
  'sensor_fault',
  'sensor_recovered',
  'dew_risk',
  'clear_sky',
  'clouded_over',
  'client_lost',
  'client_back',
  'client_disconnected',
];

// Three-valued logic for links whose facts may be unknown (null).
const and = (...values: (boolean | null)[]): boolean | null => {
  if (values.includes(false)) return false;
  return values.includes(null) ? null : true;
};
const or = (...values: (boolean | null)[]): boolean | null => {
  if (values.includes(true)) return true;
  return values.includes(null) ? null : false;
};

// A setting from the config, or the device's default when the config
// doesn't carry it (older firmware, partial mocks).
const read = <T>(config: Config, path: string, fallback: T): T => {
  let value: unknown = config;
  for (const key of path.split('.')) value = value && typeof value === 'object' ? (value as Record<string, unknown>)[key] : undefined;
  return value === undefined || value === null ? fallback : (value as T);
};
const on = (config: Config, path: string, fallback = false) => read(config, path, fallback);

type Shown = { unmet?: Unmet; neutral?: boolean };
const NEUTRAL: Shown = { neutral: true };
const FAIL_SAFE: Shown = { unmet: 'fail-safe' };

// The first unmet link makes an entry inactive; a link that's unknown makes
// it unknown unless a later one is unmet.
const decide = (entry: DepEntry, chain: Link[]) => {
  entry.state = 'active';
  for (const [linkId, reason, met] of chain) {
    if (met === null) entry.state = 'unknown';
    if (met !== false) continue;
    Object.assign(entry, { state: 'inactive', id: linkId, reason, ...REASONS[reason] });
    return;
  }
};

// While off: what would make it inactive if it were switched on.
const blocker = (chain: Link[]) => {
  const unmet = chain.find(([, , met]) => met === false);
  return unmet ? { id: unmet[0], reason: unmet[1], ...REASONS[unmet[1]] } : undefined;
};

// What every section needs, worked out once.
interface Context {
  config: Config;
  fact: <K extends keyof DepFacts>(key: K) => DepFacts[K] | null;
  hasPhones: boolean | null;
  locationKnown: boolean | null;
  phonesRing: boolean | null; // paired phones ring for Wake-level events, alerts on or not
  add: (setting: string, id: string, isOn: boolean, chain: Link[], shown?: Shown) => void;
}

const level = (c: Context, key: AlertEventKey) => read(c.config, `alerts.events.${key}.level`, 0);

// Alert channels are push only: they need the master switch. Links in
// catalogue order, so the channel's own dependency comes before "alerts are
// off" - a test send ignores the latter.
const addAlertChannels = (c: Context) => {
  const alertsOn: Link = ['D-04', 'alerts-off', on(c.config, 'alerts.enabled')];
  const wifi: Link = ['D-03', 'wifi-disconnected', c.fact('wifiConnected')];
  for (const channel of ['pushover', 'ntfy', 'webhook'])
    c.add(`alerts.${channel}.enabled`, 'D-03', on(c.config, `alerts.${channel}.enabled`), [wifi, alertsOn]);
  c.add('alerts.mqtt.enabled', 'D-01', on(c.config, 'alerts.mqtt.enabled'), [
    ['D-01', 'mqtt-off', on(c.config, 'mqtt.enabled')],
    ['D-02', 'mqtt-disconnected', c.fact('mqttConnected')],
    alertsOn,
  ]);
};

const addAlertEvents = (c: Context) => {
  const alertsEnabled = on(c.config, 'alerts.enabled');
  const rainOn: Link = ['D-05', 'rain-off', on(c.config, 'rain.enabled')];
  const bmeFound: Link = ['D-06', 'environment-missing', c.fact('environmentDetected')];
  const mlxFound: Link = ['D-07', 'infrared-missing', c.fact('infraredDetected')];
  // Imaging apps connect over Alpaca (spec 021).
  const alpacaOn: Link = ['D-37', 'alpaca-off', on(c.config, 'alpaca.enabled')];
  const eventLinks: Partial<Record<AlertEventKey, [string, Link[]]>> = {
    rain_started: ['D-05', [rainOn]],
    rain_stopped: ['D-05', [rainOn]],
    dew_risk: ['D-06', [bmeFound]],
    clear_sky: ['D-07', [mlxFound]],
    clouded_over: ['D-07', [mlxFound]],
    client_lost: ['D-37', [alpacaOn]],
    client_back: ['D-37', [alpacaOn]],
    client_disconnected: ['D-37', [alpacaOn]],
  };
  for (const key of EVENT_KEYS) {
    const [id, links] = eventLinks[key] ?? ['D-04', []];
    // Wake-level events still ring paired phones with push alerts off.
    const event: Link = ['D-04', 'alerts-off', or(alertsEnabled, and(level(c, key) === 4, c.phonesRing))];
    c.add(`alerts.events.${key}.level`, id, level(c, key) !== 0, [event, ...links]);
  }
};

const addAlertOptions = (c: Context) => {
  // "Wake me" also escalates Pushover and ntfy; only the phone ringing needs
  // Bluetooth. Muted: the default events at Wake me shouldn't warn on a
  // standard build.
  const wakePhones: Link[] = [
    ['D-08', 'ble-build', c.fact('bluetoothBuild')],
    ['D-08', 'ble-off', on(c.config, 'ble.enabled')],
    ['D-35', 'ble-restart', c.fact('bluetoothRunning')],
    ['D-08', 'no-passkey', read(c.config, 'ble.passkey', '') !== ''],
    ['D-08', 'no-phones', c.hasPhones],
  ];
  c.add(
    'alerts.wakePhones',
    'D-08',
    EVENT_KEYS.some((key) => level(c, key) === 4),
    wakePhones,
    NEUTRAL,
  );
  const alerting: Link = ['D-04', 'alerts-off', or(on(c.config, 'alerts.enabled'), c.phonesRing)];
  const skyNightOnly = on(c.config, 'alerts.skyNightOnly', true);
  const safetyNightOnly = on(c.config, 'alerts.safetyNightOnly', true);
  c.add('alerts.skyNightOnly', 'D-09', skyNightOnly, [alerting, ['D-09', 'location-unknown', c.locationKnown]]);
  c.add('alerts.safetyNightOnly', 'D-10', safetyNightOnly, [alerting, ['D-10', 'location-unknown', c.locationKnown]]);
  c.add('alerts.nightSunAltitudeDeg', 'D-11', skyNightOnly || safetyNightOnly, [alerting]);
  c.add('alerts.sendMode', 'D-12', read<string>(c.config, 'alerts.sendMode', 'any') === 'whileConnected', [
    alerting,
    ['D-12', 'alpaca-off', on(c.config, 'alpaca.enabled')],
  ]);
};

const addMqtt = (c: Context) => {
  const mqttOn: Link = ['D-13', 'mqtt-off', on(c.config, 'mqtt.enabled')];
  const homeAssistant = on(c.config, 'mqtt.homeAssistant.enabled');
  c.add('mqtt.homeAssistant.enabled', 'D-13', homeAssistant, [mqttOn]);
  c.add('mqtt.homeAssistant.alertsSwitch', 'D-13', homeAssistant, [
    mqttOn,
    ['D-13', 'alerts-off', or(on(c.config, 'alerts.enabled'), c.phonesRing)],
  ]);
  // Publishing a switched-off sensor sends nothing: harmless, shown muted.
  const publishMqtt: Link = ['D-14', 'mqtt-off', on(c.config, 'mqtt.enabled')];
  const gpsLinks: Link[] = [publishMqtt, ['D-14', 'gps-off', on(c.config, 'gps.enabled')], ['D-35', 'gps-restart', c.fact('gpsRunning')]];
  c.add('mqtt.publish.gps', 'D-14', on(c.config, 'mqtt.publish.gps', true), gpsLinks, NEUTRAL);
  c.add(
    'mqtt.publish.rain',
    'D-14',
    on(c.config, 'mqtt.publish.rain', true),
    [publishMqtt, ['D-14', 'rain-off', on(c.config, 'rain.enabled')]],
    NEUTRAL,
  );
  c.add(
    'mqtt.publish.wind',
    'D-14',
    on(c.config, 'mqtt.publish.wind', true),
    [publishMqtt, ['D-14', 'wind-off', on(c.config, 'wind.enabled')]],
    NEUTRAL,
  );
};

// Unmet behaviour confirmed from the device's SafetyEvaluator (research R4).
const addSafetyRules = (c: Context) => {
  const rule = (key: string) => on(c.config, `alpaca.${key}`);
  // Both ship on while the rain sensor ships off: not in effect, shown muted.
  const rainOn: Link = ['D-15', 'rain-off', on(c.config, 'rain.enabled')];
  const ignored: Shown = { unmet: 'inactive', neutral: true };
  c.add('alpaca.rainUnsafeEnabled', 'D-15', rule('rainUnsafeEnabled'), [rainOn], ignored);
  c.add('alpaca.rainSensorRequired', 'D-15', rule('rainSensorRequired'), [rainOn], ignored);
  const windOn: Link = ['D-16', 'wind-off', on(c.config, 'wind.enabled')];
  c.add('alpaca.windSpeedUnsafeEnabled', 'D-16', rule('windSpeedUnsafeEnabled'), [windOn], FAIL_SAFE);
  c.add('alpaca.windGustUnsafeEnabled', 'D-16', rule('windGustUnsafeEnabled'), [windOn], FAIL_SAFE);
  c.add(
    'alpaca.cloudCoverEnabled',
    'D-17',
    rule('cloudCoverEnabled'),
    [['D-17', 'infrared-missing', c.fact('infraredDetected')]],
    FAIL_SAFE,
  );
  c.add('alpaca.sqmMinEnabled', 'D-18', rule('sqmMinEnabled'), [['D-18', 'light-missing', c.fact('lightDetected')]], FAIL_SAFE);
  const bme: Link = ['D-19', 'environment-missing', c.fact('environmentDetected')];
  c.add('alpaca.humidityMaxEnabled', 'D-19', rule('humidityMaxEnabled'), [bme], FAIL_SAFE);
  c.add('alpaca.dewpointMarginEnabled', 'D-19', rule('dewpointMarginEnabled'), [bme], FAIL_SAFE);
};

const addSensorsAndTime = (c: Context) => {
  c.add('wind.directionEnabled', 'D-23', on(c.config, 'wind.directionEnabled'), [['D-23', 'wind-off', on(c.config, 'wind.enabled')]]);
  c.add('rain.dailyResetEnabled', 'D-24', on(c.config, 'rain.dailyResetEnabled'), [
    ['D-24', 'rain-off', on(c.config, 'rain.enabled')],
    ['D-24', 'clock-unset', c.fact('clockSet')],
  ]);
  c.add(
    'location.showSunMoon',
    'D-25',
    on(c.config, 'location.showSunMoon', true),
    [['D-25', 'location-unknown', c.locationKnown]],
    NEUTRAL,
  );
  c.add('gps.enabled', 'D-26', on(c.config, 'gps.enabled'), [
    ['D-35', 'gps-restart', c.fact('gpsRunning')],
    ['D-26', 'gps-no-fix', c.fact('gpsFix')],
  ]);
  c.add('ntp.enabled', 'D-28', on(c.config, 'ntp.enabled'), [['D-28', 'wifi-disconnected', c.fact('wifiConnected')]]);
  c.add('skyCalibration.enabled', 'D-29', on(c.config, 'skyCalibration.enabled'), [['D-29', 'light-missing', c.fact('lightDetected')]]);
};

const addDevice = (c: Context) => {
  const bleEnabled = on(c.config, 'ble.enabled');
  const bleBuild: Link = ['D-30', 'ble-build', c.fact('bluetoothBuild')];
  const bleRunning: Link = ['D-35', 'ble-restart', c.fact('bluetoothRunning')];
  c.add('ble.enabled', 'D-30', bleEnabled, [bleBuild, bleRunning]);
  c.add('ble.phoneAlarm', 'D-31', read(c.config, 'ble.passkey', '') !== '', [bleBuild, ['D-31', 'ble-off', bleEnabled], bleRunning]);
  c.add('ota.enabled', 'D-32', on(c.config, 'ota.enabled'), [['D-32', 'ota-no-password', read(c.config, 'ota.password', '') !== '']]);
  c.add('wifi.mdns', 'D-36', on(c.config, 'wifi.mdns', true), [['D-36', 'wifi-disconnected', c.fact('wifiConnected')]]);
};

/**
 * Every reported setting, in catalogue order. With `facts === null` (status
 * not loaded yet) runtime and hardware links are unknown: they never make a
 * setting inactive, and a setting only they could decide is "unknown".
 */
export function evaluate(config: Config, facts: DepFacts | null): DepEntry[] {
  const out: DepEntry[] = [];
  const fact = <K extends keyof DepFacts>(key: K): DepFacts[K] | null => (facts ? facts[key] : null);
  const hasPhones = facts ? facts.pairedPhones > 0 : null;
  const phoneAlarm = read(config, 'ble.passkey', '') !== '';
  const context: Context = {
    config,
    fact,
    hasPhones,
    locationKnown: or(on(config, 'location.set'), and(fact('gpsRunning'), fact('gpsFix'))),
    // Paired phones ring for Wake-level events even with push alerts off, so
    // events, arming and the night-only options still matter to them.
    phonesRing: and(fact('bluetoothBuild'), on(config, 'ble.enabled'), fact('bluetoothRunning'), phoneAlarm, hasPhones),
    add: (setting, id, isOn, chain, shown = {}) => {
      const entry: DepEntry = { id, setting, state: 'off', ...shown };
      if (isOn) decide(entry, chain);
      else entry.blockedBy = blocker(chain);
      if (!entry.blockedBy) delete entry.blockedBy;
      out.push(entry);
    },
  };
  [addAlertChannels, addAlertEvents, addAlertOptions, addMqtt, addSafetyRules, addSensorsAndTime, addDevice].forEach((section) =>
    section(context),
  );
  return out;
}

/** Lookup by setting path; what Settings shows for each dependent setting. */
export interface EffectiveView {
  get: (setting: string) => DepEntry;
  /** Inactive with a reason, or null. */
  reason: (setting: string) => DepEntry | null;
}

const UNKNOWN = (setting: string): DepEntry => ({ id: '', setting, state: 'unknown' });

export const viewOf = (entries: DepEntry[] | null): EffectiveView => {
  const bySetting = new Map((entries ?? []).map((e) => [e.setting, e]));
  const get = (setting: string) => bySetting.get(setting) ?? UNKNOWN(setting);
  return {
    get,
    reason: (setting) => {
      const entry = get(setting);
      return entry.state === 'inactive' ? entry : null;
    },
  };
};

/**
 * What Settings shows: the device's report while the form matches what the
 * device runs, else a preview of the draft with the device's facts (FR-006).
 */
export const effectiveEntries = (draft: Config, report: EffectiveReport | null, dirty: boolean): DepEntry[] => {
  const local = evaluate(draft, report?.facts ?? null);
  if (!report || dirty) return local;
  const device = new Map(report.settings.map((e) => [e.setting, e]));
  // Switched on in the form: the device's answer. Off: the local entry,
  // which also says what would block switching it on.
  return local.map((e) => {
    const reported = device.get(e.setting);
    return e.state !== 'off' && reported && reported.state !== 'off' ? reported : e;
  });
};

// Runtime conditions (network, clock, GPS fix, a restart) never block a
// toggle; settings and missing hardware do (FR-005, constitution V).
// The OTA password and the passkey are typed in once the setting is on, so
// they don't block it either.
const NON_BLOCKING = new Set([
  'wifi-disconnected',
  'mqtt-disconnected',
  'clock-unset',
  'gps-no-fix',
  'gps-restart',
  'ble-restart',
  'ota-no-password',
  'no-passkey',
  'no-phones',
]);
export const blocksSwitchingOn = (reason: string | undefined) => reason !== undefined && !NON_BLOCKING.has(reason);

/** Settings that saving the draft would make inactive (were active or off before). */
export const newlyInactive = (saved: DepEntry[], draft: DepEntry[]): DepEntry[] => {
  const before = new Map(saved.map((e) => [e.setting, e.state]));
  return draft.filter((e) => e.state === 'inactive' && !e.neutral && before.get(e.setting) !== 'inactive');
};

/** The note under a dependent setting: why it's inactive, or what keeps it from being switched on. */
export interface DepNoteContent {
  id: string;
  reason: string;
  text: string;
  tone: 'warn' | 'info';
  target: DepEntry; // what its fix action goes to
}

export const noteFor = (entry: DepEntry, prefix = t('settingsDeps.inactive')): DepNoteContent | null => {
  if (entry.state === 'inactive' && entry.reason) {
    return { id: entry.id, reason: entry.reason, text: `${prefix} - ${entry.text}`, tone: entry.neutral ? 'info' : 'warn', target: entry };
  }
  const blocked = entry.state === 'off' ? entry.blockedBy : undefined;
  return blocked
    ? { id: blocked.id, reason: blocked.reason, text: `${blocked.text}.`, tone: 'info', target: { ...entry, ...blocked } }
    : null;
};

/** What saving the draft would make inactive; nothing while the form is clean (FR-006). */
export const previewInactive = (draft: Config, saved: Config | null, report: EffectiveReport | null, dirty: boolean): DepEntry[] =>
  dirty && saved ? newlyInactive(effectiveEntries(saved, report, false), evaluate(draft, report?.facts ?? null)) : [];

/** Where an inactive setting's one-click fix goes: a Settings tab and section, or a restart. */
export const fixTarget = (entry: DepEntry): { restart: true } | { tab: string; anchor?: string } => {
  if (entry.fix === 'restart') return { restart: true };
  const [tab, anchor] = (entry.fix ?? '').split('#');
  return { tab, anchor };
};

/** GET /api/settings/effective: the device's report, or null if it can't be read. */
export const fetchEffectiveReport = async (): Promise<EffectiveReport | null> => {
  try {
    const response = await fetch('/api/settings/effective');
    const data = response.ok ? await response.json() : null;
    return data && Array.isArray(data.settings) ? (data as EffectiveReport) : null;
  } catch {
    return null; // device unreachable: Settings previews from the form alone
  }
};
