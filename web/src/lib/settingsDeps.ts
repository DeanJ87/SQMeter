import type { AlertEventKey, Config } from '../types';

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
  'alerts-off': { text: 'Alerts are off', fix: 'alerts#alerts' },
  'mqtt-off': { text: 'MQTT is off', fix: 'network#mqtt' },
  'mqtt-disconnected': { text: 'Broker not connected', fix: 'network#mqtt' },
  'wifi-disconnected': { text: 'Not connected to WiFi', fix: 'network#wifi' },
  'rain-off': { text: 'Rain sensor is off', fix: 'sensors#rain' },
  'wind-off': { text: 'Anemometer is off', fix: 'sensors#wind' },
  'gps-off': { text: 'GPS is off', fix: 'time#time-sources' },
  'gps-restart': { text: 'GPS starts after a restart', fix: 'restart' },
  'gps-no-fix': { text: 'No GPS fix - using the location in Settings', fix: 'time#location' },
  'light-missing': { text: 'TSL2591 not detected', fix: 'sensors#sky-sensors' },
  'infrared-missing': { text: 'MLX90614 not detected', fix: 'sensors#sky-sensors' },
  'environment-missing': { text: 'BME280 not detected', fix: 'sensors#sky-sensors' },
  'location-unknown': { text: 'Needs your location', fix: 'time#location' },
  'alpaca-off': { text: 'Alpaca is off', fix: 'safety#alpaca' },
  'ble-build': { text: 'Needs the Bluetooth firmware build', fix: 'device#ble' },
  'ble-off': { text: 'Bluetooth is off', fix: 'device#ble' },
  'ble-restart': { text: 'Bluetooth starts after a restart', fix: 'restart' },
  'no-passkey': { text: 'No pairing passkey set', fix: 'device#ble' },
  'no-phones': { text: 'No phone paired', fix: 'device#ble' },
  'clock-unset': { text: "The device doesn't know the time yet", fix: 'time#time-sources' },
  'ota-no-password': { text: 'Set an upload password', fix: 'device#security' },
};

// Label for the one-click fix of each reason.
export const FIX_LABEL: Record<string, string> = {
  'alerts-off': 'Turn on alerts',
  'mqtt-off': 'Turn on MQTT',
  'mqtt-disconnected': 'MQTT settings',
  'wifi-disconnected': 'WiFi settings',
  'rain-off': 'Turn on',
  'wind-off': 'Turn on',
  'gps-off': 'Turn on GPS',
  'gps-restart': 'Restart',
  'gps-no-fix': 'Location',
  'light-missing': 'Sky sensors',
  'infrared-missing': 'Sky sensors',
  'environment-missing': 'Sky sensors',
  'location-unknown': 'Set location',
  'alpaca-off': 'Turn on Alpaca',
  'ble-build': 'Bluetooth',
  'ble-off': 'Turn on Bluetooth',
  'ble-restart': 'Restart',
  'no-passkey': 'Set a passkey',
  'no-phones': 'Pair a phone',
  'clock-unset': 'Time sources',
  'ota-no-password': 'Set password',
};

// One link of a dependency chain: met, or the catalogue ID and reason that
// make the setting inactive. `met === null`: unknown (no facts yet).
type Link = [id: string, reason: string, met: boolean | null];

const EVENT_KEYS: AlertEventKey[] = ['unsafe', 'safe', 'rain_started', 'rain_stopped', 'sensor_fault', 'sensor_recovered', 'dew_risk', 'clear_sky', 'clouded_over'];

/**
 * Every reported setting, in catalogue order. With `facts === null` (status
 * not loaded yet) runtime and hardware links are unknown: they never make a
 * setting inactive, and a setting only they could decide is "unknown".
 */
export function evaluate(config: Config, facts: DepFacts | null): DepEntry[] {
  const out: DepEntry[] = [];
  const fact = <K extends keyof DepFacts>(key: K): DepFacts[K] | null => (facts ? facts[key] : null);
  const add = (setting: string, id: string, on: boolean, chain: Link[], extra: { unmet?: Unmet; neutral?: boolean } = {}) => {
    const entry: DepEntry = { id, setting, state: 'off', ...extra };
    if (!on) {
      const unmet = chain.find(([, , met]) => met === false);
      if (unmet) entry.blockedBy = { id: unmet[0], reason: unmet[1], ...REASONS[unmet[1]] };
    } else {
      entry.state = 'active';
      for (const [linkId, reason, met] of chain) {
        if (met === null) {
          entry.state = 'unknown';
          continue;
        }
        if (met) continue;
        entry.state = 'inactive';
        entry.id = linkId;
        entry.reason = reason;
        entry.text = REASONS[reason].text;
        entry.fix = REASONS[reason].fix;
        break;
      }
    }
    out.push(entry);
  };
  const and = (...values: (boolean | null)[]): boolean | null =>
    values.some((v) => v === false) ? false : values.some((v) => v === null) ? null : true;
  const or = (...values: (boolean | null)[]): boolean | null =>
    values.some((v) => v === true) ? true : values.some((v) => v === null) ? null : false;

  const a = config.alerts;
  const alertsEnabled = a?.enabled ?? false;
  const level = (key: AlertEventKey) => a?.events?.[key]?.level ?? 0;
  const ble = config.ble ?? { enabled: false, passkey: '' };
  const phoneAlarm = (ble.passkey ?? '') !== '';
  const locationKnown = or(config.location?.set ?? false, and(fact('gpsRunning'), fact('gpsFix')));
  const phonesRing = and(fact('bluetoothBuild'), ble.enabled, fact('bluetoothRunning'), phoneAlarm, facts ? facts.pairedPhones > 0 : null);

  const alertsOn: Link = ['D-04', 'alerts-off', alertsEnabled];
  const wifi: Link = ['D-03', 'wifi-disconnected', fact('wifiConnected')];
  const alerting: Link = ['D-04', 'alerts-off', or(alertsEnabled, phonesRing)];
  const event = (key: AlertEventKey): Link => ['D-04', 'alerts-off', or(alertsEnabled, and(level(key) === 4, phonesRing))];

  // Alert channels (push only: they need the master switch). Links in
  // catalogue order, so the channel's own dependency comes before "alerts
  // are off" - a test send ignores the latter.
  add('alerts.pushover.enabled', 'D-03', a?.pushover?.enabled ?? false, [wifi, alertsOn]);
  add('alerts.ntfy.enabled', 'D-03', a?.ntfy?.enabled ?? false, [wifi, alertsOn]);
  add('alerts.webhook.enabled', 'D-03', a?.webhook?.enabled ?? false, [wifi, alertsOn]);
  add('alerts.mqtt.enabled', 'D-01', a?.mqtt?.enabled ?? false, [
    ['D-01', 'mqtt-off', config.mqtt.enabled],
    ['D-02', 'mqtt-disconnected', fact('mqttConnected')],
    alertsOn,
  ]);

  // Alert events.
  const rainEnabled = config.rain?.enabled ?? false;
  const rainOn: Link = ['D-05', 'rain-off', rainEnabled];
  const bmeFound: Link = ['D-06', 'environment-missing', fact('environmentDetected')];
  const mlxFound: Link = ['D-07', 'infrared-missing', fact('infraredDetected')];
  const eventLinks: Partial<Record<AlertEventKey, [string, Link[]]>> = {
    rain_started: ['D-05', [rainOn]],
    rain_stopped: ['D-05', [rainOn]],
    dew_risk: ['D-06', [bmeFound]],
    clear_sky: ['D-07', [mlxFound]],
    clouded_over: ['D-07', [mlxFound]],
  };
  for (const key of EVENT_KEYS) {
    const [id, links] = eventLinks[key] ?? ['D-04', []];
    add(`alerts.events.${key}.level`, id, level(key) !== 0, [event(key), ...links]);
  }
  // "Wake me" also escalates Pushover and ntfy; only the phone ringing needs Bluetooth.
  add('alerts.wakePhones', 'D-08', EVENT_KEYS.some((key) => level(key) === 4), [
    ['D-08', 'ble-build', fact('bluetoothBuild')],
    ['D-08', 'ble-off', ble.enabled],
    ['D-35', 'ble-restart', fact('bluetoothRunning')],
    ['D-08', 'no-passkey', phoneAlarm],
    ['D-08', 'no-phones', facts ? facts.pairedPhones > 0 : null],
  ], { neutral: true }); // muted: the default events at Wake me shouldn't warn on a standard build
  const skyNightOnly = a?.skyNightOnly ?? true;
  const safetyNightOnly = a?.safetyNightOnly ?? true;
  add('alerts.skyNightOnly', 'D-09', skyNightOnly, [alerting, ['D-09', 'location-unknown', locationKnown]]);
  add('alerts.safetyNightOnly', 'D-10', safetyNightOnly, [alerting, ['D-10', 'location-unknown', locationKnown]]);
  add('alerts.nightSunAltitudeDeg', 'D-11', skyNightOnly || safetyNightOnly, [alerting]);
  add('alerts.armWithAlpaca', 'D-12', a?.armWithAlpaca ?? false, [alerting, ['D-12', 'alpaca-off', config.alpaca?.enabled ?? false]]);

  // MQTT and Home Assistant.
  const mqttOn: Link = ['D-13', 'mqtt-off', config.mqtt.enabled];
  const homeAssistant = config.mqtt.homeAssistant?.enabled ?? false;
  add('mqtt.homeAssistant.enabled', 'D-13', homeAssistant, [mqttOn]);
  add('mqtt.homeAssistant.alertsSwitch', 'D-13', homeAssistant, [mqttOn, ['D-13', 'alerts-off', or(alertsEnabled, phonesRing)]]);
  const publish = config.mqtt.publish;
  const publishMqtt: Link = ['D-14', 'mqtt-off', config.mqtt.enabled];
  add('mqtt.publish.gps', 'D-14', publish?.gps ?? true, [publishMqtt, ['D-14', 'gps-off', config.gps.enabled], ['D-35', 'gps-restart', fact('gpsRunning')]], {
    neutral: true,
  });
  add('mqtt.publish.rain', 'D-14', publish?.rain ?? true, [publishMqtt, ['D-14', 'rain-off', rainEnabled]], { neutral: true });
  add('mqtt.publish.wind', 'D-14', publish?.wind ?? true, [publishMqtt, ['D-14', 'wind-off', config.wind?.enabled ?? false]], { neutral: true });

  // Safety rules (unmet behaviour confirmed from the device's SafetyEvaluator).
  const s = config.alpaca;
  const rainRule: Link = ['D-15', 'rain-off', rainEnabled];
  // Both ship on while the rain sensor ships off: not in effect, shown muted.
  add('alpaca.rainUnsafeEnabled', 'D-15', s?.rainUnsafeEnabled ?? false, [rainRule], { unmet: 'inactive', neutral: true });
  add('alpaca.rainSensorRequired', 'D-15', s?.rainSensorRequired ?? false, [rainRule], { unmet: 'inactive', neutral: true });
  const windOn: Link = ['D-16', 'wind-off', config.wind?.enabled ?? false];
  add('alpaca.windSpeedUnsafeEnabled', 'D-16', s?.windSpeedUnsafeEnabled ?? false, [windOn], { unmet: 'fail-safe' });
  add('alpaca.windGustUnsafeEnabled', 'D-16', s?.windGustUnsafeEnabled ?? false, [windOn], { unmet: 'fail-safe' });
  add('alpaca.cloudCoverEnabled', 'D-17', s?.cloudCoverEnabled ?? false, [['D-17', 'infrared-missing', fact('infraredDetected')]], { unmet: 'fail-safe' });
  add('alpaca.sqmMinEnabled', 'D-18', s?.sqmMinEnabled ?? false, [['D-18', 'light-missing', fact('lightDetected')]], { unmet: 'fail-safe' });
  const bme: Link = ['D-19', 'environment-missing', fact('environmentDetected')];
  add('alpaca.humidityMaxEnabled', 'D-19', s?.humidityMaxEnabled ?? false, [bme], { unmet: 'fail-safe' });
  add('alpaca.dewpointMarginEnabled', 'D-19', s?.dewpointMarginEnabled ?? false, [bme], { unmet: 'fail-safe' });

  // Sensors, time and location.
  add('wind.directionEnabled', 'D-23', config.wind?.directionEnabled ?? false, [['D-23', 'wind-off', config.wind?.enabled ?? false]]);
  add('rain.dailyResetEnabled', 'D-24', config.rain?.dailyResetEnabled ?? false, [
    ['D-24', 'rain-off', rainEnabled],
    ['D-24', 'clock-unset', fact('clockSet')],
  ]);
  add('location.showSunMoon', 'D-25', config.location?.showSunMoon ?? true, [['D-25', 'location-unknown', locationKnown]], { neutral: true });
  add('gps.enabled', 'D-26', config.gps.enabled, [
    ['D-35', 'gps-restart', fact('gpsRunning')],
    ['D-26', 'gps-no-fix', fact('gpsFix')],
  ]);
  add('ntp.enabled', 'D-28', config.ntp.enabled, [['D-28', 'wifi-disconnected', fact('wifiConnected')]]);
  add('skyCalibration.enabled', 'D-29', config.skyCalibration?.enabled ?? false, [['D-29', 'light-missing', fact('lightDetected')]]);

  // Device.
  const bleBuild: Link = ['D-30', 'ble-build', fact('bluetoothBuild')];
  const bleRunning: Link = ['D-35', 'ble-restart', fact('bluetoothRunning')];
  add('ble.enabled', 'D-30', ble.enabled, [bleBuild, bleRunning]);
  add('ble.phoneAlarm', 'D-31', phoneAlarm, [bleBuild, ['D-31', 'ble-off', ble.enabled], bleRunning]);
  add('ota.enabled', 'D-32', config.ota.enabled, [['D-32', 'ota-no-password', (config.ota.password ?? '') !== '']]);
  add('wifi.mdns', 'D-36', config.wifi.mdns ?? true, [['D-36', 'wifi-disconnected', fact('wifiConnected')]]);
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
const NON_BLOCKING = new Set(['wifi-disconnected', 'mqtt-disconnected', 'clock-unset', 'gps-no-fix', 'gps-restart', 'ble-restart', 'ota-no-password', 'no-passkey', 'no-phones']);
export const blocksSwitchingOn = (reason: string | undefined) => reason !== undefined && !NON_BLOCKING.has(reason);

/** Settings that saving the draft would make inactive (were active or off before). */
export const newlyInactive = (saved: DepEntry[], draft: DepEntry[]): DepEntry[] => {
  const before = new Map(saved.map((e) => [e.setting, e.state]));
  return draft.filter((e) => e.state === 'inactive' && !e.neutral && before.get(e.setting) !== 'inactive');
};
