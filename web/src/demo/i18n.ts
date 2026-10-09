import { http, HttpResponse } from 'msw';
import { isLanguageCode } from '../i18n/languages';
import { demoDevice } from './device';

// The demo's language files (specs/023-i18n FR-018): the same flow as a
// device - the language is a device setting, the file is "downloaded" and
// served at /lang.json - but the files come from the demo site itself (each
// locale is its own chunk), so nothing leaves the browser.

const LOCALES = import.meta.glob('../i18n/locales/*.json', { import: 'default' });
const VERSION = '0.2.0-beta.3';

const language = (): string => demoDevice.rawConfig().language ?? 'en';

// ?lang=ar opens the demo in that language (the per-language checks and
// screenshots). This module loads after the device starts and before the UI
// loads its language.
const linked = typeof location === 'undefined' ? null : new URLSearchParams(location.search).get('lang');
if (linked && isLanguageCode(linked) && linked !== language())
  demoDevice.applyConfig(JSON.stringify({ ...demoDevice.rawConfig(), language: linked }));

// A new language "downloads" for a moment, as on a device. Setting
// sessionStorage `sqm.demo.languageFail` makes the download fail the way a
// device does when no language file is published for its firmware, and
// `interrupted` the way it reports a download a restart cut short (tests).
const DOWNLOAD_MS = 1500;
const FAILURES: Record<string, string> = {
  '1': "Couldn't download the language file for this firmware version",
  interrupted: "The last language download didn't finish - choose the language again to retry",
};
// The language the demo opened in counts as already installed.
let chosen: { code: string; at: number } | null = null;

const downloadFailure = () => {
  try {
    return FAILURES[sessionStorage.getItem('sqm.demo.languageFail') ?? ''];
  } catch {
    return undefined;
  }
};

export const i18nDocument = () => {
  const code = language();
  if (!chosen) chosen = { code, at: 0 };
  else if (code !== chosen.code) chosen = { code, at: Date.now() };
  const base = { language: code, firmwareVersion: VERSION };
  if (code === 'en') return { ...base, state: 'idle', pack: null };
  if (Date.now() - chosen.at < DOWNLOAD_MS) return { ...base, state: 'downloading', pack: null };
  const failure = downloadFailure();
  if (failure) return { ...base, state: 'failed', pack: null, error: failure };
  return { ...base, state: 'installed', pack: { lang: code, version: VERSION, size: 0 } };
};

const restartDownload = () => {
  chosen = { code: language(), at: Date.now() };
};

const notEnglish = () => language() !== 'en';

export const i18nHandlers = [
  http.get('/api/i18n', () => HttpResponse.json(i18nDocument())),
  http.post('/api/i18n/install', () => {
    if (!notEnglish()) return HttpResponse.json({ error: 'English is built in' }, { status: 409 });
    restartDownload();
    return HttpResponse.json({ started: true }, { status: 202 });
  }),
  // As the device: the browser checked the file and says which language it is.
  http.post('/api/i18n/upload', ({ request }) => {
    const lang = new URL(request.url).searchParams.get('lang') ?? '';
    if (!isLanguageCode(lang) || lang === 'en')
      return HttpResponse.json({ error: "That isn't a SQMeter language file (.json.gz)" }, { status: 400 });
    demoDevice.applyConfig(JSON.stringify({ ...demoDevice.rawConfig(), language: lang }));
    return HttpResponse.json(i18nDocument());
  }),
  http.get('*/lang.json', async () => {
    const load = LOCALES[`../i18n/locales/${language()}.json`];
    if (!load) return new HttpResponse(null, { status: 404 });
    return HttpResponse.json({ lang: language(), version: VERSION, messages: await load() });
  }),
];
