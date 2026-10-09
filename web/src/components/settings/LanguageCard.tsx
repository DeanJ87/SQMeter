import { FunctionalComponent } from 'preact';
import { useRef } from 'preact/hooks';
import { t } from '../../i18n';
import { LANGUAGES } from '../../i18n/languages';
import { deviceText } from '../../i18n/deviceMessage';
import { languageProblem } from '../../i18n/loader';
import { useLanguage } from '../../hooks/useLanguage';
import { Note } from '../ui';
import { ActionButton, Field, SelectInput, SettingsCard } from './controls';

// Settings → Device → Language (specs/023-i18n): the device-wide language, the
// state of its stored language file, a retry and a hand upload.

const problemText = (kind: ReturnType<typeof languageProblem>['kind']) => {
  switch (kind) {
    case 'downloading':
      return t('language.problemDownloading');
    case 'missing':
      return t('language.problemMissing');
    case 'damaged':
      return t('language.problemDamaged');
    case 'otherVersion':
      return t('language.problemOtherVersion');
    default:
      return null;
  }
};

const LanguageCard: FunctionalComponent<{ language: string; onChange: (code: string) => void }> = ({ language, onChange }) => {
  const { status, busy, message, retry, upload } = useLanguage();
  const fileInput = useRef<HTMLInputElement>(null);
  const { kind, detail } = languageProblem();
  const problem = problemText(kind);
  // The device's own reason, e.g. a download a restart interrupted.
  const reason = kind === 'missing' && detail ? deviceText(detail) : null;
  const savedOther = status && status.language !== 'en';

  return (
    <SettingsCard id="language" title={t('language.title')}>
      <Field label={t('language.label')} hint={t('language.hint')}>
        <SelectInput
          dataField="language"
          value={language}
          onChange={onChange}
          options={LANGUAGES.map((option) => ({ value: option.code, label: option.name }))}
        />
      </Field>
      <Note>{t('language.alertsStayEnglish')}</Note>
      {problem && (
        <Note tone="warn" action={savedOther ? { label: t('language.retry'), onClick: () => void retry() } : undefined}>
          {problem}
          {reason && (
            <>
              {' '}
              <span data-language-reason>{reason}</span>
            </>
          )}
        </Note>
      )}
      {message && <Note tone={message.tone === 'ok' ? 'ok' : 'bad'}>{message.text}</Note>}
      {savedOther && (
        <div class="btn-row">
          <ActionButton onClick={() => void retry()} busy={busy} busyLabel={t('common.working')}>
            {t('language.downloadAgain')}
          </ActionButton>
          <ActionButton onClick={() => fileInput.current?.click()} disabled={busy}>
            {t('language.uploadFile')}
          </ActionButton>
          <input
            ref={fileInput}
            type="file"
            accept=".gz,application/gzip"
            hidden
            onChange={(event) => {
              const file = (event.target as HTMLInputElement).files?.[0];
              if (file) void upload(file);
            }}
          />
        </div>
      )}
    </SettingsCard>
  );
};

export default LanguageCard;
