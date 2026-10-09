import { setLanguage, type Messages } from './index';
import { isLanguageCode, type LanguageCode } from './languages';

// Loads the device's language before the app starts (specs/023-i18n FR-007,
// FR-013): asks the device which language it is set to, fetches the stored
// file, checks it, and installs it. Anything wrong leaves English in place and
// records why, for the notice in the UI. Never throws.

export type LanguageProblem =
  | 'none' // English, or the chosen language loaded
  | 'downloading' // the device is still fetching the file
  | 'missing' // no file on the device (offline, or the download failed)
  | 'damaged' // the file isn't a usable language file
  | 'otherVersion'; // loaded, but made for another firmware version (missing keys show in English)

export interface LanguageFile {
  lang: string;
  version: string;
  messages: Messages;
}

export interface DeviceLanguage {
  language: string;
  state: 'idle' | 'downloading' | 'installed' | 'failed' | 'restoring';
  pack: { lang: string; version: string; size: number } | null;
  error?: string;
  firmwareVersion?: string;
}

let problem: { code: LanguageCode | null; kind: LanguageProblem; detail?: string } = { code: null, kind: 'none' };

/** Why the UI isn't (fully) in the chosen language, for the notice in Settings and the header. */
export const languageProblem = () => problem;

/** A parsed language file, or null if it isn't one (FR-013). */
export function parseLanguageFile(raw: unknown, expected?: string): LanguageFile | null {
  if (!raw || typeof raw !== 'object') return null;
  const file = raw as Partial<LanguageFile>;
  if (!isLanguageCode(file.lang) || file.lang === 'en' || (expected && file.lang !== expected)) return null;
  if (!file.messages || typeof file.messages !== 'object' || Array.isArray(file.messages)) return null;
  const valid = Object.values(file.messages).every(
    (value) =>
      typeof value === 'string' || (value && typeof value === 'object' && Object.values(value).every((v) => typeof v === 'string')),
  );
  return valid ? { lang: file.lang, version: String(file.version ?? ''), messages: file.messages } : null;
}

const getJson = async (url: string) => {
  const response = await fetch(url, { cache: 'no-store' });
  if (!response.ok) throw new Error(`HTTP ${response.status}`);
  return response.json();
};

/** Installs the device's language; resolves when the UI can render. */
export async function loadLanguage(): Promise<void> {
  try {
    const device = (await getJson('/api/i18n')) as DeviceLanguage;
    const code = device.language;
    if (!isLanguageCode(code) || code === 'en') return;
    if (device.state === 'downloading' || device.state === 'restoring') problem = { code, kind: 'downloading' };
    if (!device.pack || device.pack.lang !== code) {
      if (problem.kind === 'none') problem = { code, kind: 'missing', detail: device.error };
      return;
    }
    const file = parseLanguageFile(await getJson('/lang.json'), code);
    if (!file) {
      problem = { code, kind: 'damaged' };
      return;
    }
    setLanguage(code, file.messages);
    if (device.firmwareVersion && file.version && file.version !== '-' && file.version !== device.firmwareVersion)
      problem = { code, kind: 'otherVersion', detail: file.version };
  } catch {
    // Older firmware without /api/i18n, or no device: English.
  }
}
