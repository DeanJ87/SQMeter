import { useCallback, useEffect, useState } from 'preact/hooks';
import { t } from '../i18n';
import { deviceError } from '../i18n/deviceMessage';
import { parseLanguageFile, type DeviceLanguage } from '../i18n/loader';

// The device's language file (specs/023-i18n): its state, a retry, and the
// hand upload for devices without internet. Data layer for Settings → Device.

const POLL_MS = 1000;
const WAIT_MS = 60_000;

export const fetchLanguageStatus = async (): Promise<DeviceLanguage | null> => {
  try {
    const response = await fetch('/api/i18n', { cache: 'no-store' });
    return response.ok ? ((await response.json()) as DeviceLanguage) : null;
  } catch {
    return null;
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

/** After the language setting is saved: reload into it once the device has the file. */
export const applyLanguage = async (code: string) => {
  if (code !== 'en') await waitForLanguage(code);
  // English, installed, or failed: reload either way - the loader falls back
  // to English and the Language card says why.
  window.location.reload();
};

export const useLanguage = () => {
  const [status, setStatus] = useState<DeviceLanguage | null>(null);
  const [busy, setBusy] = useState(false);
  const [message, setMessage] = useState<{ tone: 'ok' | 'bad'; text: string } | null>(null);

  const refresh = useCallback(async () => setStatus(await fetchLanguageStatus()), []);
  useEffect(() => void refresh(), [refresh]);

  const finish = async (code: string) => {
    const result = await waitForLanguage(code);
    setStatus(result);
    if (result?.state === 'installed') window.location.reload();
    else setMessage({ tone: 'bad', text: result?.error ? deviceError(result, '') : t('language.downloadFailed') });
  };

  const retry = async () => {
    setBusy(true);
    setMessage(null);
    try {
      const response = await fetch('/api/i18n/install', { method: 'POST' });
      if (!response.ok)
        setMessage({ tone: 'bad', text: deviceError(await response.json().catch(() => null), t('language.downloadFailed')) });
      else await finish(status?.language ?? '');
    } catch {
      setMessage({ tone: 'bad', text: t('language.couldNotReachDevice') });
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
