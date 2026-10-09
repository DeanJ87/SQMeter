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

export const i18nDocument = () => {
  const code = language();
  return {
    language: code,
    state: code === 'en' ? 'idle' : 'installed',
    firmwareVersion: VERSION,
    pack: code === 'en' ? null : { lang: code, version: VERSION, size: 0 },
  };
};

const notEnglish = () => language() !== 'en';

export const i18nHandlers = [
  http.get('/api/i18n', () => HttpResponse.json(i18nDocument())),
  http.post('/api/i18n/install', () =>
    notEnglish()
      ? HttpResponse.json({ started: true }, { status: 202 })
      : HttpResponse.json({ error: 'English is built in' }, { status: 409 }),
  ),
  http.post('/api/i18n/upload', () => HttpResponse.json({ success: true, status: i18nDocument() })),
  http.get('*/lang.json', async () => {
    const load = LOCALES[`../i18n/locales/${language()}.json`];
    if (!load) return new HttpResponse(null, { status: 404 });
    return HttpResponse.json({ lang: language(), version: VERSION, messages: await load() });
  }),
];
