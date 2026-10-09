// The languages SQMeter ships (specs/023-i18n). Names are each language's own
// name for itself, so a user can always find theirs. Keep in step with
// lib/LanguageLogic (the device validates the same codes).

export const LANGUAGES = [
  { code: 'en', name: 'English' },
  { code: 'id', name: 'Bahasa Indonesia' },
  { code: 'de', name: 'Deutsch' },
  { code: 'es', name: 'Español' },
  { code: 'fr', name: 'Français' },
  { code: 'it', name: 'Italiano' },
  { code: 'nl', name: 'Nederlands' },
  { code: 'pl', name: 'Polski' },
  { code: 'pt-BR', name: 'Português (Brasil)' },
  { code: 'tr', name: 'Türkçe' },
  { code: 'ar', name: 'العربية' },
  { code: 'ja', name: '日本語' },
  { code: 'ko', name: '한국어' },
  { code: 'zh-Hans', name: '简体中文' },
] as const;

export type LanguageCode = (typeof LANGUAGES)[number]['code'];

const RTL: readonly string[] = ['ar'];

export const isRtl = (code: string) => RTL.includes(code);

export const isLanguageCode = (code: unknown): code is LanguageCode => LANGUAGES.some((language) => language.code === code);
