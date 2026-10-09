import { FunctionalComponent } from 'preact';
import { route } from 'preact-router';
import { t } from '../i18n';
import { deviceText } from '../i18n/deviceMessage';
import { LANGUAGES } from '../i18n/languages';
import { dismissLanguageProgress, retryLanguage, useLanguageProgress, type LanguageProgress } from '../hooks/useLanguage';
import { Note } from './ui';

// The language switch in progress (specs/023-i18n FR-024): what the device is
// doing while it fetches the file, and what to do if it can't. Shown on every
// page, in the current (usually English) language, until the page reloads.

const nameOf = (code: string) => LANGUAGES.find((option) => option.code === code)?.name ?? code;

const text = (value: LanguageProgress) => {
  const language = nameOf(value.code);
  switch (value.phase) {
    case 'downloading':
      return t('language.progressDownloading', { language });
    case 'restarting':
      return t('language.progressRestarting');
    case 'installed':
      return t('language.progressInstalled', { language });
    case 'slow':
      return t('language.progressSlow', { language });
    case 'failed':
      return t('language.progressFailed', { language, reason: value.error ? deviceText(value.error) : t('language.downloadFailed') });
  }
};

const LanguageProgressBanner: FunctionalComponent = () => {
  const value = useLanguageProgress();
  if (!value) return null;
  const failed = value.phase === 'failed';
  return (
    <div class="language-progress" role="status" aria-live="polite" data-phase={value.phase}>
      <Note tone={failed ? 'bad' : value.phase === 'installed' ? 'ok' : 'warn'}>{text(value)}</Note>
      {failed && (
        <>
          <Note>{t('language.progressFailedHelp', { version: value.firmwareVersion ?? '-' })}</Note>
          <div class="btn-row">
            <button type="button" class="btn btn-default btn-sm" onClick={() => void retryLanguage(value.code)}>
              {t('language.retry')}
            </button>
            <button
              type="button"
              class="btn btn-link btn-sm"
              onClick={() => {
                dismissLanguageProgress();
                route('/settings?tab=device&section=language');
              }}
            >
              {t('layout.languageSettings')}
            </button>
            <button type="button" class="btn btn-link btn-sm" onClick={dismissLanguageProgress}>
              {t('toast.dismiss')}
            </button>
          </div>
        </>
      )}
    </div>
  );
};

export default LanguageProgressBanner;
