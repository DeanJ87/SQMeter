import { useCallback, useEffect, useState } from 'preact/hooks';
import { t } from '../i18n';
import { deviceError } from '../i18n/deviceMessage';
import { parseLanguageFile, type DeviceLanguage } from '../i18n/loader';

// The device's language file (specs/023-i18n): its state, a retry, and the
// hand upload for devices without internet. Data layer for Settings → Device.

const POLL_MS = 1000;
const WAIT_MS = 60_000;
// A restarting device often doesn't refuse the connection, it just doesn't
// answer: give up on a poll after this, so the page can say it's restarting.
const POLL_TIMEOUT_MS = 3000;

export const fetchLanguageStatus = async (): Promise<DeviceLanguage | null> => {
  const abort = new AbortController();
  const timer = setTimeout(() => abort.abort(), POLL_TIMEOUT_MS);
  try {
    const response = await fetch('/api/i18n', { cache: 'no-store', signal: abort.signal });
    return response.ok ? ((await response.json()) as DeviceLanguage) : null;
  } catch {
    return null;
  } finally {
    clearTimeout(timer);
  }
};

/** Waits for the device to finish installing; true when the file is there. */
export const waitForLanguage = async (code: string, deadline = Date.now() + WAIT_MS): Promise<DeviceLanguage | null> => {
  for (;;) {
    const status = await fetchLanguageStatus();
    const done = status && status.state !== 'downloading' && status.state !== 'restoring';
    if (done || Date.now() > deadline) return status && status.language === code ? status : null;
    await new Promise((resolve) => setTimeout(resolve, POLL_MS));
  }
};

/** Reads a .json.gz (or .json) language file in the browser and checks it (FR-012). */
export const readLanguageFile = async (file: File) => {
  const gz = new Uint8Array(await file.slice(0, 2).arrayBuffer());
  const isGzip = gz[0] === 0x1f && gz[1] === 0x8b;
  const stream = isGzip && 'DecompressionStream' in window ? file.stream().pipeThrough(new DecompressionStream('gzip')) : file.stream();
  try {
    return { isGzip, parsed: parseLanguageFile(JSON.parse(await new Response(stream).text())) };
  } catch {
    return { isGzip, parsed: null };
  }
};

// --- Switching language: progress the UI shows while the device fetches the file.

export type LanguageProgress =
  | { phase: 'downloading'; code: string }
  | { phase: 'restarting'; code: string } // the device isn't answering (restarting)
  | { phase: 'installed'; code: string }
  | { phase: 'failed'; code: string; error?: string; firmwareVersion?: string }
  | { phase: 'slow'; code: string }; // still going after the wait: carry on without reloading

let progress: LanguageProgress | null = null;
const progressListeners = new Set<(value: LanguageProgress | null) => void>();
const setProgress = (value: LanguageProgress | null) => {
  progress = value;
  progressListeners.forEach((listener) => listener(value));
};

/** The language switch in progress, for the banner on every page. */
export const useLanguageProgress = () => {
  const [value, setValue] = useState<LanguageProgress | null>(progress);
  useEffect(() => {
    progressListeners.add(setValue);
    return () => void progressListeners.delete(setValue);
  }, []);
  return value;
};

const UNREACHABLE_AFTER = 1; // failed or timed-out polls in a row before "restarting"
const RELOAD_DELAY_MS = 800;

// One poll's verdict: the next phase, or null to keep waiting.
const phaseFor = (code: string, status: DeviceLanguage | null, misses: number): LanguageProgress | null => {
  if (!status) return misses >= UNREACHABLE_AFTER ? { phase: 'restarting', code } : null;
  const pending = status.state === 'downloading' || status.state === 'restoring' || (status.state === 'idle' && !status.error);
  if (status.language !== code || pending) return { phase: 'downloading', code };
  if (status.state === 'installed' || status.pack?.lang === code) return { phase: 'installed', code };
  return { phase: 'failed', code, error: status.error, firmwareVersion: status.firmwareVersion };
};

/** Follows the device until the file is installed (then reloads into it), it fails, or the wait runs out. */
const followInstall = async (code: string, deadline: number) => {
  setProgress({ phase: 'downloading', code });
  let misses = 0;
  while (Date.now() <= deadline) {
    const status = await fetchLanguageStatus();
    misses = status ? 0 : misses + 1;
    const next = phaseFor(code, status, misses);
    if (next) setProgress(next);
    if (next?.phase === 'failed') return;
    if (next?.phase === 'installed') {
      // Same address the page came from: a restart never moves the browser elsewhere.
      setTimeout(() => window.location.reload(), RELOAD_DELAY_MS);
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, POLL_MS));
  }
  setProgress({ phase: 'slow', code });
};

/** After the language setting is saved: show progress, then reload into the language once the device has it. */
export const applyLanguage = async (code: string) => {
  if (code === 'en') {
    window.location.reload(); // English is built in
    return;
  }
  await followInstall(code, Date.now() + WAIT_MS);
};

/** Asks the device to download the language again, and follows it. */
export const retryLanguage = async (code: string) => {
  setProgress({ phase: 'downloading', code });
  try {
    const response = await fetch('/api/i18n/install', { method: 'POST' });
    if (!response.ok) {
      const body = (await response.json().catch(() => null)) as { error?: string } | null;
      setProgress({ phase: 'failed', code, error: body?.error });
      return;
    }
  } catch {
    setProgress({ phase: 'restarting', code });
  }
  await followInstall(code, Date.now() + WAIT_MS);
};

export const dismissLanguageProgress = () => setProgress(null);

export const useLanguage = () => {
  const [status, setStatus] = useState<DeviceLanguage | null>(null);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState<{ tone: 'ok' | 'bad'; text: string } | null>(null);

  const refresh = useCallback(async () => setStatus(await fetchLanguageStatus()), []);
  useEffect(() => void refresh(), [refresh]);

  // Progress and the outcome show in the language banner on every page.
  const retry = async () => {
    setBusy(true);
    setMessage(null);
    try {
      await retryLanguage(status?.language ?? '');
      await refresh();
    } finally {
      setBusy(false);
    }
  };

  const upload = async (file: File) => {
    setBusy(true);
    setMessage(null);
    try {
      const { isGzip, parsed } = await readLanguageFile(file);
      if (!isGzip || !parsed) {
        setMessage({ tone: 'bad', text: t('language.notALanguageFile') });
        return;
      }
      const form = new FormData();
      form.append('file', file);
      const query = new URLSearchParams({ lang: parsed.lang, version: parsed.version });
      const response = await fetch(`/api/i18n/upload?${query}`, { method: 'POST', body: form });
      if (response.ok) window.location.reload();
      else setMessage({ tone: 'bad', text: deviceError(await response.json().catch(() => null), t('language.uploadFailed')) });
    } catch {
      setMessage({ tone: 'bad', text: t('language.couldNotReachDevice') });
    } finally {
      setBusy(false);
    }
  };

  return { status, busy, message, retry, upload, refresh };
};
