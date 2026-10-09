import { FunctionalComponent } from 'preact';
import { useEffect, useRef } from 'preact/hooks';
import { t, type MessageKey } from '../../i18n';
import { LANGUAGES } from '../../i18n/languages';
import { deviceText } from '../../i18n/deviceMessage';
import { languageProblem } from '../../i18n/loader';
import { retryLanguage, useLanguage, useLanguageProgress, type LanguageProgress } from '../../hooks/useLanguage';
import { InfoTip, Note, Pill, ProgressMeter } from '../ui';
import { ActionButton, Field, SelectInput, SettingsCard } from './controls';

// Settings → Device → Language (specs/023-i18n, 026 FR-004): the device-wide
// language and its stored file. A change reports here, like a firmware
// update: the language with a pill, a progress meter while the device
// fetches the file, then one line with what happened and at most one action.

const nameOf = (code: string) => LANGUAGES.find((option) => option.code === code)?.name ?? code;

type Phase = LanguageProgress['phase'];

const PHASE: Record<Phase, { text: MessageKey; tone: string; step?: number }> = {
  downloading: { text: 'status.downloading', tone: 'pill-cyan', step: 40 },
  restarting: { text: 'language.phaseRestarting', tone: 'pill-amber', step: 60 },
  slow: { text: 'language.phaseSlow', tone: 'pill-amber', step: 80 },
  installed: { text: 'language.phaseInstalled', tone: 'pill-green' },
  failed: { text: 'language.phaseFailed', tone: 'pill-red' },
};

const Outcome: FunctionalComponent<{ value: LanguageProgress }> = ({ value }) => {
  const language = nameOf(value.code);
  if (value.phase === 'installed') return <Note tone="ok">{t('language.reloading', { language })}</Note>;
  if (value.phase === 'slow') return <Note>{t('language.slowNote')}</Note>;
  if (value.phase !== 'failed') return null;
  // The device's own reason first; it is the most specific.
  const text = value.error
    ? deviceText(value.error)
    : value.firmwareVersion
      ? t('language.noFileForVersion', { version: value.firmwareVersion })
      : t('language.downloadFailed');
  return (
    <Note tone="bad" action={{ label: t('language.retry'), onClick: () => void retryLanguage(value.code) }}>
      {text} <InfoTip text={t('language.failedHint')} />
    </Note>
  );
};

const Progress: FunctionalComponent<{ value: LanguageProgress }> = ({ value }) => {
  const phase = PHASE[value.phase];
  // Save sits at the bottom of the page: bring the progress into view once.
  const box = useRef<HTMLDivElement>(null);
  useEffect(() => box.current?.scrollIntoView?.({ block: 'nearest', behavior: 'smooth' }), []);
  return (
    <div class="language-steps" role="status" aria-live="polite" data-phase={value.phase} ref={box}>
      <div class="reading-row">
        <span class="reading-label">{nameOf(value.code)}</span>
        <Pill tone={phase.tone}>{t(phase.text)}</Pill>
      </div>
      {phase.step !== undefined && <ProgressMeter value={phase.step} label={t('language.downloadProgress')} />}
      <Outcome value={value} />
    </div>
  );
};

const PROBLEM: Partial<Record<ReturnType<typeof languageProblem>['kind'], MessageKey>> = {
  downloading: 'language.problemDownloading',
  missing: 'language.problemMissing',
  damaged: 'language.problemDamaged',
  otherVersion: 'language.problemOtherVersion',
};

// After a reload: why the UI isn't (fully) in the chosen language.
const Problem: FunctionalComponent<{ onRetry?: () => void }> = ({ onRetry }) => {
  const { kind, detail } = languageProblem();
  const key = PROBLEM[kind];
  if (!key) return null;
  // The device's own reason (e.g. a download a restart interrupted) beats the generic line.
  const text = kind === 'missing' && detail ? deviceText(detail) : t(key);
  return (
    <Note tone="warn" action={onRetry ? { label: t('language.retry'), onClick: onRetry } : undefined}>
      <span data-language-reason>{text}</span>
    </Note>
  );
};

const FileActions: FunctionalComponent<{ busy: boolean; retry: () => void; upload: (file: File) => void }> = ({ busy, retry, upload }) => {
  const fileInput = useRef<HTMLInputElement>(null);
  return (
    <div class="btn-row">
      <ActionButton onClick={retry} busy={busy} busyLabel={t('common.working')}>
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
          if (file) upload(file);
        }}
      />
    </div>
  );
};

const LanguageCard: FunctionalComponent<{ language: string; onChange: (code: string) => void }> = ({ language, onChange }) => {
  const { status, busy, message, retry, upload } = useLanguage();
  const progress = useLanguageProgress();
  const savedOther = Boolean(status && status.language !== 'en');

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
      {progress ? <Progress value={progress} /> : <Problem onRetry={savedOther ? () => void retry() : undefined} />}
      {message && <Note tone={message.tone === 'ok' ? 'ok' : 'bad'}>{message.text}</Note>}
      {savedOther && !progress && <FileActions busy={busy} retry={() => void retry()} upload={(file) => void upload(file)} />}
    </SettingsCard>
  );
};

export default LanguageCard;
