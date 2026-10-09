import type { GithubRelease } from '../types';
import { deviceError } from '../i18n/deviceMessage';
import { bodyOf, postJson, request } from './api';

// Firmware updates (OTA): the release list the device fetched from GitHub,
// starting a device-side update, and uploading an image by hand.

/** The releases on `track`; throws with the device's reason when it can't say. */
export const fetchReleases = async (track: string): Promise<GithubRelease[]> => {
  const response = await request(`/api/updates/check?track=${track}`);
  if (!response.ok) {
    const body = await bodyOf(response);
    throw new Error(deviceError(body, `HTTP ${response.status}`));
  }
  return response.json();
};

/** Ask the device to download and install `release`; null when it started, else why not. Rejects when unreachable. */
export const startReleaseUpdate = async (release: GithubRelease): Promise<string | null> => {
  const response = await postJson('/api/updates/apply', {
    firmwareAssetUrl: release.firmwareAssetUrl,
    firmwareAssetSize: release.firmwareAssetSize,
    fsAssetUrl: release.fsAssetUrl,
    fsAssetSize: release.fsAssetSize,
  });
  const body = await bodyOf(response);
  return !response.ok || body.success !== true ? deviceError(body, `HTTP ${response.status}`) : null;
};

export type UploadHandlers = {
  onProgress: (percent: number) => void;
  onLoad: (status: number, responseText: string) => void;
  onError: () => void;
};

/** POST `file` as the `update` form field with upload progress (fetch can't report it). */
export const uploadImage = (endpoint: string, file: File, handlers: UploadHandlers) => {
  const formData = new FormData();
  formData.append('update', file);
  const xhr = new XMLHttpRequest();
  xhr.upload.addEventListener('progress', (e) => {
    if (e.lengthComputable) handlers.onProgress(Math.round((e.loaded / e.total) * 100));
  });
  xhr.addEventListener('load', () => handlers.onLoad(xhr.status, xhr.responseText));
  xhr.addEventListener('error', handlers.onError);
  xhr.open('POST', endpoint);
  xhr.send(formData);
};

/**
 * What an upload's reply means: the device is restarting onto the new image,
 * or it refused (with its reason, or `unknown` when it gave none).
 */
export const uploadOutcome = (status: number, responseText: string, unknown: string): { ok: true } | { ok: false; errorMsg: string } => {
  if (status === 200) {
    try {
      const response = JSON.parse(responseText);
      return response.success === true ? { ok: true } : { ok: false, errorMsg: deviceError(response, unknown) };
    } catch {
      // A 200 that isn't JSON: the device took the image.
      return { ok: true };
    }
  }
  // Failures carry {"error": "..."} with a 4xx/5xx status.
  let errorMsg = `HTTP ${status}`;
  try {
    errorMsg = deviceError(JSON.parse(responseText), errorMsg);
  } catch {
    // not JSON - keep the status code
  }
  return { ok: false, errorMsg };
};

/** Demo mode: fake progress in steps, since the emulated device can't report a real one. */
export const fakeProgress = (onStep: (percent: number) => void, onDone: () => void) => {
  let p = 0;
  const iv = setInterval(() => {
    p = Math.min(p + Math.random() * 12 + 6, 100);
    onStep(Math.round(p));
    if (p >= 100) {
      clearInterval(iv);
      onDone();
    }
  }, 250);
};
