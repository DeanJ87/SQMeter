import en from './en.json';
import { isRtl, type LanguageCode } from './languages';

// The UI's text, by key (specs/023-i18n). English is built in; another
// language's messages are installed before the app modules load (main.tsx),
// so module-level labels translate too. A key missing from the installed
// language falls back to English (FR-013).

export type MessageKey = keyof typeof en;
type PluralForms = Partial<Record<Intl.LDMLPluralRule, string>>;
export type Messages = Record<string, string | PluralForms>;
export type Params = Record<string, unknown>;

const english = en as Messages;
let active: Messages = english;
let language: LanguageCode = 'en';
let pluralRules = new Intl.PluralRules('en');

/** Installs a language's messages (English when `messages` is null). */
export function setLanguage(code: LanguageCode, messages: Messages | null) {
  language = messages ? code : 'en';
  active = messages ?? english;
  pluralRules = new Intl.PluralRules(language);
  if (typeof document !== 'undefined') {
    document.documentElement.lang = language;
    document.documentElement.dir = isRtl(language) ? 'rtl' : 'ltr';
  }
}

/** The active language, also the locale for number and date formatting. */
export const currentLanguage = (): LanguageCode => language;

const pick = (value: string | PluralForms | undefined, count: number | undefined): string | undefined => {
  if (value === undefined || typeof value === 'string') return value;
  if (count === 0 && value.zero !== undefined) return value.zero; // an explicit zero form wins in every language
  const category = count === undefined ? 'other' : pluralRules.select(count);
  return value[category] ?? value.other;
};

const fill = (text: string, params?: Params) =>
  params ? text.replace(/\{(\w+)\}/g, (match, name: string) => (name in params ? String(params[name]) : match)) : text;

/** The message for `key` in the active language, with `{name}` placeholders filled. */
export function t(key: MessageKey, params?: Params): string {
  const count = typeof params?.count === 'number' ? params.count : undefined;
  const text = pick(active[key], count) ?? pick(english[key], count) ?? key;
  return fill(text, params);
}

/** Like t() for a key that comes from data (device message IDs); null when unknown. */
export function tMaybe(key: string, params?: Params): string | null {
  return key in english ? t(key as MessageKey, params) : null;
}
