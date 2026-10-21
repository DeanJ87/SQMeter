import { bodyOf, postJson } from './api';
import type { BackupHeader, RestorePreview, Settings } from './backupPreview';

// Settings backup and restore against the device (specs/033-config-backup,
// contracts/rest-api.md), and the file in the browser.

/** A file bigger than this isn't read (backups are about 20 KB at most, pretty-printed). */
export const MAX_FILE_BYTES = 64 * 1024;
export const MIN_PASSPHRASE_CHARS = 8;

export interface BackupRefusal {
  error: string;
  code: string;
}

export type Result<T> = { ok: true; value: T } | { ok: false; refusal: BackupRefusal };

const refusalOf = async (response: Response): Promise<BackupRefusal> => {
  const body = await bodyOf(response);
  return {
    error: typeof body.error === 'string' ? body.error : `HTTP ${response.status}`,
    code: typeof body.code === 'string' ? body.code : '',
  };
};

/** Characters as people count them (code points), like the device. */
export const passphraseLength = (passphrase: string) => [...passphrase].length;

/** POST /api/backup: the backup file's JSON, from a background task on the device (a second or two with a passphrase). */
export const requestBackup = async (passphrase: string): Promise<Result<Settings>> => {
  const response = await postJson('/api/backup', passphrase ? { passphrase } : {});
  if (!response.ok) return { ok: false, refusal: await refusalOf(response) };
  return { ok: true, value: (await response.json()) as Settings };
};

/** "sqmeter-<name>-<YYYY-MM-DD>.json" in the browser's date. */
export const backupFileName = (deviceName: string, date: Date) => {
  const name = deviceName.replace(/[^A-Za-z0-9_-]/g, '-') || 'device';
  const day = `${date.getFullYear()}-${String(date.getMonth() + 1).padStart(2, '0')}-${String(date.getDate()).padStart(2, '0')}`;
  return `sqmeter-${name}-${day}.json`;
};

/** Saves the backup as a readable (pretty-printed) file. */
export const saveBackupFile = (backup: Settings, fileName: string) => {
  const url = URL.createObjectURL(new Blob([`${JSON.stringify(backup, null, 2)}\n`], { type: 'application/json' }));
  const link = document.createElement('a');
  link.href = url;
  link.download = fileName;
  link.click();
  setTimeout(() => URL.revokeObjectURL(url), 1000);
};

/** An uploaded file, as far as the browser can tell before the device reads it. */
export interface ChosenFile {
  name: string;
  text: string; // compact JSON when it parsed, else the text as uploaded
  header: BackupHeader | null; // null: not a format-1 backup (the device decides)
  settings: Settings; // the settings in the file ({} when unreadable)
  hasSecrets: boolean;
}

const isObject = (value: unknown): value is Settings => typeof value === 'object' && value !== null && !Array.isArray(value);

const headerOf = (file: Settings): BackupHeader | null => {
  if (typeof file.sqmeterBackup !== 'number') return null;
  const text = (key: string) => (typeof file[key] === 'string' ? (file[key] as string) : undefined);
  return {
    format: file.sqmeterBackup,
    firmware: text('firmware'),
    board: text('board'),
    build: text('build'),
    deviceName: text('deviceName'),
    createdAt: text('createdAt'),
  };
};

/** Reads a chosen file; null when it's over MAX_FILE_BYTES (refused before reading). */
export const readBackupFile = async (file: File): Promise<ChosenFile | null> => {
  if (file.size > MAX_FILE_BYTES) return null;
  const raw = await file.text();
  try {
    const parsed: unknown = JSON.parse(raw);
    if (!isObject(parsed)) return { name: file.name, text: raw, header: null, settings: {}, hasSecrets: false };
    const header = headerOf(parsed);
    const settings = header ? (isObject(parsed.settings) ? parsed.settings : {}) : parsed;
    return { name: file.name, text: JSON.stringify(parsed), header, settings, hasSecrets: isObject(parsed.secrets) };
  } catch {
    // Not JSON: sent as it is, so the device gives the reason (damaged, or not a backup).
    return { name: file.name, text: raw, header: null, settings: {}, hasSecrets: false };
  }
};

/** POST /api/backup/preview: nothing is applied. */
export const previewRestore = async (text: string, passphrase: string): Promise<Result<RestorePreview>> => {
  const response = await postJson('/api/backup/preview', passphrase ? { backup: text, passphrase } : { backup: text });
  if (!response.ok) return { ok: false, refusal: await refusalOf(response) };
  return { ok: true, value: (await response.json()) as RestorePreview };
};

/** POST /api/backup/apply with the preview's token. */
export const applyRestore = async (token: string): Promise<Result<{ restarting: boolean }>> => {
  const response = await postJson('/api/backup/apply', { token });
  if (!response.ok) return { ok: false, refusal: await refusalOf(response) };
  const body = await bodyOf(response);
  return { ok: true, value: { restarting: body.restarting === true } };
};
