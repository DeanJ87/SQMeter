import { compareVersions } from '../utils/versionCompare';

// What a restore changes, worded from the device's preview
// (specs/033-config-backup contracts/rest-api.md): the device decides what is
// applied and returns the candidate's masked settings; the differences, the
// settings kept or ignored and the cut-off warnings are listed here, so the
// device image stays small (research R7).

export type SecretsState = 'restored' | 'kept' | 'none';
export type NotApplicableReason = 'pin' | 'analog' | 'bluetooth' | 'pushover-keys';
export type RestartCode = 'wifi' | 'ota' | 'gps' | 'i2c' | 'ipv6' | 'alpaca' | 'ble';

export type Settings = { [key: string]: unknown };

export interface BackupHeader {
  format: number;
  firmware?: string;
  board?: string;
  build?: string;
  deviceName?: string;
  createdAt?: string;
}

export interface RestorePreview {
  token: string;
  backup: BackupHeader;
  settings: Settings;
  secrets: { state: SecretsState; changed: string[]; unchanged: string[] };
  notApplicable: { path: string; value?: unknown; reason: NotApplicableReason }[];
  restart: RestartCode[];
}

export interface Change {
  path: string;
  from: unknown;
  to: unknown;
}

export type Warning =
  | { code: 'wifi-network'; ssid: string }
  | { code: 'device-password' }
  | { code: 'address'; hostname: string }
  | { code: 'imaging-app' }
  | { code: 'newer-firmware'; firmware: string };

export interface PreviewFacts {
  firmware?: string; // this device's version
  imagingAppConnected: boolean;
}

export const SECRET_PATHS = [
  'wifi.password',
  'mqtt.password',
  'ota.password',
  'auth.password',
  'ble.passkey',
  'alerts.pushover.userKey',
  'alerts.pushover.appToken',
  'alerts.ntfy.token',
  'alerts.webhook.authHeader',
] as const;

// Written for older firmware only (alerts.sendMode is the setting).
const DERIVED = 'alerts.armWithAlpaca';
const reported = (path: string) => path !== DERIVED && !(SECRET_PATHS as readonly string[]).includes(path);
const isObject = (value: unknown): value is Settings => typeof value === 'object' && value !== null && !Array.isArray(value);
const join = (base: string, key: string) => (base ? `${base}.${key}` : key);

/** Paths in `have` that `other` lacks; a whole missing section is one path. */
export const missingPaths = (have: Settings, other: Settings, base = ''): string[] =>
  Object.entries(have).flatMap(([key, value]) => {
    const path = join(base, key);
    if (!reported(path)) return [];
    if (!(key in other)) return [path];
    const next = other[key];
    return isObject(value) && isObject(next) ? missingPaths(value, next, path) : [];
  });

/** Every reported setting whose value differs. */
export const changedSettings = (before: Settings, after: Settings, base = ''): Change[] =>
  Object.entries(before).flatMap(([key, from]) => {
    const path = join(base, key);
    const to = after[key];
    if (!reported(path) || to === undefined) return [];
    if (isObject(from) && isObject(to)) return changedSettings(from, to, path);
    return JSON.stringify(from) === JSON.stringify(to) ? [] : [{ path, from, to }];
  });

const at = (settings: Settings, path: string): unknown =>
  path.split('.').reduce<unknown>((node, key) => (isObject(node) ? node[key] : undefined), settings);

const passwordWarning = (current: Settings, preview: RestorePreview) => {
  const after = preview.settings;
  const on = at(after, 'auth.enabled') === true && at(after, 'auth.password') !== '';
  const changed =
    at(current, 'auth.enabled') !== true ||
    at(current, 'auth.password') === '' ||
    at(current, 'auth.username') !== at(after, 'auth.username') ||
    preview.secrets.changed.includes('auth.password');
  return on && changed;
};

/** Warnings about changes that can cut the owner off or interrupt a session (FR-008). */
export const restoreWarnings = (current: Settings, preview: RestorePreview, facts: PreviewFacts): Warning[] => {
  const after = preview.settings;
  const warnings: Warning[] = [];
  const ssid = String(at(after, 'wifi.ssid') ?? '');
  if (ssid !== String(at(current, 'wifi.ssid') ?? '')) warnings.push({ code: 'wifi-network', ssid });
  if (passwordWarning(current, preview)) warnings.push({ code: 'device-password' });
  const hostname = String(at(after, 'wifi.hostname') ?? '');
  if (hostname !== at(current, 'wifi.hostname') || at(after, 'wifi.mdns') !== at(current, 'wifi.mdns'))
    warnings.push({ code: 'address', hostname });
  if (preview.restart.length > 0 && facts.imagingAppConnected) warnings.push({ code: 'imaging-app' });
  const made = preview.backup.firmware;
  if (made && facts.firmware && (compareVersions(made, facts.firmware) ?? 0) > 0) warnings.push({ code: 'newer-firmware', firmware: made });
  return warnings;
};

export interface PreviewDetails {
  changes: Change[];
  kept: string[];
  ignored: string[];
  warnings: Warning[];
}

/** Everything the preview lists, from this device's settings and the uploaded file's. */
export const previewDetails = (
  current: Settings,
  fileSettings: Settings,
  preview: RestorePreview,
  facts: PreviewFacts,
): PreviewDetails => ({
  changes: changedSettings(current, preview.settings),
  kept: missingPaths(current, fileSettings),
  ignored: missingPaths(fileSettings, preview.settings),
  warnings: restoreWarnings(current, preview, facts),
});

/** The first part of a path: the section a change is listed under ("general" for top-level settings). */
export const sectionOf = (path: string) => (path.includes('.') ? path.slice(0, path.indexOf('.')) : 'general');

/** Whether nothing at all would change. */
export const changesNothing = (details: PreviewDetails, preview: RestorePreview) =>
  details.changes.length === 0 && preview.secrets.changed.length === 0;
