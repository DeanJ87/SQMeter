import { useState } from 'preact/hooks';
import { t } from '../i18n';
import { fakeProgress, uploadImage, uploadOutcome } from '../lib/updates';
import { useWaitForReboot } from './useWaitForReboot';

export type UpdateType = 'firmware' | 'filesystem';

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

    // The progress when the upload started: a dropped connection after a
    // complete upload means the device restarted.
    const progressAtStart = uploadProgress;
    try {
      uploadImage(updateType === 'firmware' ? '/api/update' : '/api/update/fs', file, {
        onProgress: (progress) => {
          setUploadProgress(progress);
          setStatus(t('updates.uploadingProgress', { progress }));
        },
        onLoad: (code, text) => {
          const outcome = uploadOutcome(code, text, t('updates.unknownError'));
          if (outcome.ok) return rebooting();
          setStatus(t('updates.uploadFailedErrormsg', { errorMsg: outcome.errorMsg }));
          setUploading(false);
        },
        onError: () => {
          if (progressAtStart === 100) rebooting();
          else {
            setStatus(t('updates.uploadErrorOccurred'));
            setUploading(false);
          }
        },
      });
    } catch (error) {
      setStatus(t('updates.failedToUploadError', { error }));
      setUploading(false);
    }
  };

  return { file, setFile, uploading, uploadProgress, status, setStatus, waitingForReboot, handleUpload };
};
