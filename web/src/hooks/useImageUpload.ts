import { useState } from 'preact/hooks';
import { t } from '../i18n';
import { fakeProgress, uploadImage, uploadOutcome } from '../lib/updates';
import { useWaitForReboot } from './useWaitForReboot';

export type UpdateType = 'firmware' | 'filesystem';

type UploadUi = {
  setProgress: (percent: number) => void;
  setStatus: (text: string) => void;
  /** The upload ended without the device taking the image. */
  stop: () => void;
  /** The device took the image and is restarting onto it. */
  rebooting: () => void;
};

// One upload; a stall is tried once more by itself. The device abandons a
// stalled upload, so the retry starts clean (spec 012 FR-003a).
const sendImage = (endpoint: string, file: File, ui: UploadUi, retriesLeft = 1) => {
  // How far this attempt got: a dropped connection after a complete upload
  // means the device restarted onto the new image.
  let lastProgress = 0;
  uploadImage(endpoint, file, {
    onProgress: (progress) => {
      lastProgress = progress;
      ui.setProgress(progress);
      ui.setStatus(t('updates.uploadingProgress', { progress }));
    },
    onLoad: (code, text) => {
      const outcome = uploadOutcome(code, text, t('updates.unknownError'));
      if (outcome.ok) return ui.rebooting();
      ui.setStatus(t('updates.uploadFailedErrormsg', { errorMsg: outcome.errorMsg }));
      ui.stop();
    },
    onError: () => {
      if (lastProgress === 100) return ui.rebooting();
      ui.setStatus(t('updates.uploadErrorOccurred'));
      ui.stop();
    },
    onStall: () => {
      if (retriesLeft > 0) {
        ui.setProgress(0);
        ui.setStatus(t('updates.uploadStalledRetrying'));
        sendImage(endpoint, file, ui, retriesLeft - 1);
        return;
      }
      ui.setStatus(t('updates.uploadStalled'));
      ui.stop();
    },
  });
};

// Flashing a .bin by hand: upload with progress, then wait for the restart.
export const useImageUpload = () => {
  const [file, setFile] = useState<File | null>(null);
  const [uploading, setUploading] = useState(false);
  const [uploadProgress, setUploadProgress] = useState(0);
  const [status, setStatus] = useState<string>('');
  const [waitingForReboot, setWaitingForReboot] = useState(false);

  useWaitForReboot(
    waitingForReboot,
    () => {
      setStatus(t('updates.updateSuccessfulDeviceIsBack'));
      setWaitingForReboot(false);
      setFile(null);
    },
    () => setStatus(t('common.restarting')),
  );

  const rebooting = () => {
    setStatus(t('updates.uploadCompleteDeviceIsRebooting'));
    setUploading(false);
    setWaitingForReboot(true);
  };

  const handleUpload = async (updateType: UpdateType) => {
    if (!file) return;

    setUploading(true);
    setUploadProgress(0);
    setStatus(t('common.uploading'));

    // Demo mode: MSW can't trigger XHR upload progress events, so fake it
    if (import.meta.env.VITE_DEMO_MODE === 'true') {
      fakeProgress((rounded) => {
        setUploadProgress(rounded);
        setStatus(t('updates.uploadingRounded', { rounded }));
      }, rebooting);
      return;
    }

    try {
      sendImage(updateType === 'firmware' ? '/api/update' : '/api/update/fs', file, {
        setProgress: setUploadProgress,
        setStatus,
        stop: () => setUploading(false),
        rebooting,
      });
    } catch (error) {
      setStatus(t('updates.failedToUploadError', { error }));
      setUploading(false);
    }
  };

  return { file, setFile, uploading, uploadProgress, status, setStatus, waitingForReboot, handleUpload };
};
