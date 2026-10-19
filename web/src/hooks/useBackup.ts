import { useState } from 'preact/hooks';
import { getJson } from '../lib/api';
import {
  applyRestore,
  backupFileName,
  MIN_PASSPHRASE_CHARS,
  passphraseLength,
  previewRestore,
  readBackupFile,
  requestBackup,
  saveBackupFile,
  type ChosenFile,
} from '../lib/backup';
import { previewDetails, type PreviewDetails, type PreviewFacts, type RestorePreview, type Settings } from '../lib/backupPreview';
import { deviceText } from '../i18n/deviceMessage';
import { t } from '../i18n';
import { useWaitForReboot } from './useWaitForReboot';

// The Backup & restore card's state (specs/033-config-backup contracts/ui.md):
// backing up (useBackup) and restoring (useRestore).

export interface Outcome {
  type: 'success' | 'error';
  text: string;
}

export const passphraseProblem = (passphrase: string, repeat: string): { passphrase?: string; repeat?: string } => {
  if (!passphrase) return {};
  if (passphraseLength(passphrase) < MIN_PASSPHRASE_CHARS) return { passphrase: t('settings.backup.atLeast8Characters') };
  return passphrase === repeat ? {} : { repeat: t('settings.backup.passphrasesDontMatch') };
};

const unreachable = () => t('settings.backup.couldNotReachTheDevice');

export const useBackup = (deviceName: string) => {
  const [passphrase, setPassphrase] = useState('');
  const [repeat, setRepeat] = useState('');
  const [showErrors, setShowErrors] = useState(false);
  const [busy, setBusy] = useState(false);
  const [outcome, setOutcome] = useState<Outcome | null>(null);

  const backUp = async () => {
    const problems = passphraseProblem(passphrase, repeat);
    setShowErrors(true);
    if (problems.passphrase || problems.repeat) return;
    setBusy(true);
    setOutcome(null);
    try {
      const result = await requestBackup(passphrase);
      if (!result.ok) {
        setOutcome({ type: 'error', text: deviceText(result.refusal.error) });
        return;
      }
      saveBackupFile(result.value, backupFileName(deviceName, new Date()));
      setOutcome({ type: 'success', text: passphrase ? t('settings.backup.savedWithSecrets') : t('settings.backup.savedWithoutSecrets') });
      setPassphrase('');
      setRepeat('');
      setShowErrors(false);
    } catch {
      setOutcome({ type: 'error', text: unreachable() });
    } finally {
      setBusy(false);
    }
  };

  const problems = showErrors ? passphraseProblem(passphrase, repeat) : {};
  return { passphrase, setPassphrase, repeat, setRepeat, problems, busy, outcome, backUp };
};

export interface Restore {
  file: ChosenFile;
  passphrase: string;
  preview: RestorePreview | null;
  details: PreviewDetails | null;
  refusal: string | null;
  wrongPassphrase: boolean;
}

const restoreOf = (file: ChosenFile, passphrase = '', refusal: string | null = null): Restore => ({
  file,
  passphrase,
  preview: null,
  details: null,
  refusal,
  wrongPassphrase: false,
});

// The device's preview of `file`, with what it changes here; never throws.
const fetchPreview = async (file: ChosenFile, passphrase: string, facts: PreviewFacts): Promise<Restore> => {
  try {
    const [result, current] = await Promise.all([previewRestore(file.text, passphrase), getJson<Settings>('/api/config')]);
    if (!result.ok)
      return {
        ...restoreOf(file, passphrase, deviceText(result.refusal.error)),
        wrongPassphrase: result.refusal.code === 'wrong-passphrase',
      };
    return {
      ...restoreOf(file, passphrase),
      preview: result.value,
      details: previewDetails(current ?? {}, file.settings, result.value, facts),
    };
  } catch {
    return restoreOf(file, passphrase, unreachable());
  }
};

// Uploading, previewing and applying a backup. `onRestored` runs when it's
// applied without a restart; with one, the page reloads once the device is back.
export const useRestore = (facts: PreviewFacts, onRestored: () => void) => {
  const [busy, setBusy] = useState<'read' | 'preview' | 'apply' | null>(null);
  const [restore, setRestore] = useState<Restore | null>(null);
  const [restarting, setRestarting] = useState(false);
  useWaitForReboot(restarting, () => window.location.reload());

  const runPreview = async (file: ChosenFile, passphrase: string) => {
    setBusy('preview');
    setRestore(await fetchPreview(file, passphrase, facts));
    setBusy(null);
  };

  const chooseFile = async (file: File) => {
    setBusy('read');
    const chosen = await readBackupFile(file);
    setBusy(null);
    if (!chosen) {
      setRestore(
        restoreOf({ name: file.name, text: '', header: null, settings: {}, hasSecrets: false }, '', t('settings.backup.fileTooLarge')),
      );
      return;
    }
    // A file with encrypted secrets asks for its passphrase first; without it this device keeps its own.
    if (chosen.hasSecrets) setRestore(restoreOf(chosen));
    else await runPreview(chosen, '');
  };

  const apply = async () => {
    if (!restore?.preview) return;
    setBusy('apply');
    try {
      const result = await applyRestore(restore.preview.token);
      if (!result.ok) setRestore(restoreOf(restore.file, restore.passphrase, deviceText(result.refusal.error)));
      else if (result.value.restarting) setRestarting(true);
      else {
        onRestored();
        setRestore(null);
      }
    } catch {
      setRestore({ ...restore, refusal: unreachable() });
    } finally {
      setBusy(null);
    }
  };

  return {
    busy,
    restore,
    restarting,
    chooseFile,
    apply,
    setPassphrase: (value: string) => restore && setRestore({ ...restore, passphrase: value, wrongPassphrase: false, refusal: null }),
    preview: () => (restore ? runPreview(restore.file, restore.passphrase) : Promise.resolve()),
    cancel: () => setRestore(null),
  };
};

export type BackupState = ReturnType<typeof useBackup>;
export type RestoreState = ReturnType<typeof useRestore>;
