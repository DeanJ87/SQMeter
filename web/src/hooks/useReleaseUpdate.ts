import { useEffect, useState } from 'preact/hooks';
import type { GithubRelease, SystemStatus } from '../types';
import { t } from '../i18n';
import { saveUpdateCheck } from '../lib/lastUpdateCheck';
import { fakeProgress, fetchReleases, startReleaseUpdate } from '../lib/updates';
import { useWaitForReboot } from './useWaitForReboot';

export type ReleaseTrack = 'stable' | 'beta';
export type StatusMessage = SystemStatus | { type: 'ota_progress'; progress: number } | { error: string };

// The releases on a track, from the device's GitHub check.
export const useReleases = (track: ReleaseTrack) => {
  const [releases, setReleases] = useState<GithubRelease[]>([]);
  const [checking, setChecking] = useState(false);
  const [checkError, setCheckError] = useState('');
  const [selectedTag, setSelectedTag] = useState('');

  useEffect(() => {
    const checkForUpdates = async (selectedTrack: ReleaseTrack) => {
      setChecking(true);
      setCheckError('');
      try {
        const data = await fetchReleases(selectedTrack);
        setReleases(data);
        setSelectedTag(data[0]?.tag ?? '');
        saveUpdateCheck(data[0]?.tag ?? null);
      } catch (error) {
        setCheckError(t('updates.failedToCheckForUpdates', { error }));
        setReleases([]);
      } finally {
        setChecking(false);
      }
    };
    checkForUpdates(track);
  }, [track]);

  return { releases, checking, checkError, selectedTag, setSelectedTag };
};

// Installing a release on the device: start it, follow the progress the
// firmware pushes on /ws/status, then wait for the device to come back.
export const useReleaseApply = (statusMsg: StatusMessage | null) => {
  const [applying, setApplying] = useState(false);
  const [applyProgress, setApplyProgress] = useState(0);
  const [applyStatus, setApplyStatus] = useState('');
  const [waitingForReboot, setWaitingForReboot] = useState(false);

  // Reflect firmware-pushed OTA progress/errors from /ws/status while applying
  useEffect(() => {
    if (!applying || !statusMsg) return;
    if ('type' in statusMsg && statusMsg.type === 'ota_progress') {
      setApplyProgress(statusMsg.progress);
      setApplyStatus(t('updates.applyingUpdateProgress', { progress: statusMsg.progress }));
      if (statusMsg.progress >= 100) {
        setApplyStatus(t('updates.updateCompleteDeviceIsRebooting'));
        setApplying(false);
        setWaitingForReboot(true);
      }
    } else if ('error' in statusMsg) {
      setApplyStatus(t('updates.updateFailedError', { error: statusMsg.error }));
      setApplying(false);
    }
  }, [statusMsg, applying]);

  useWaitForReboot(waitingForReboot, () => {
    setApplyStatus(t('updates.updateSuccessfulDeviceIsBack'));
    setWaitingForReboot(false);
  });

  const applyUpdate = async (release: GithubRelease | undefined) => {
    if (!release) return;

    setApplying(true);
    setApplyProgress(0);
    setApplyStatus(t('updates.startingUpdate'));

    // Demo mode: the firmware isn't real, so fake progress the same way the
    // manual upload flow does (MSW can't push simulated WebSocket progress
    // messages timed against a fake download).
    if (import.meta.env.VITE_DEMO_MODE === 'true') {
      fakeProgress(
        (rounded) => {
          setApplyProgress(rounded);
          setApplyStatus(t('updates.applyingUpdateRounded', { rounded }));
        },
        () => {
          setApplyStatus(t('updates.updateCompleteDeviceIsRebooting'));
          setApplying(false);
          setWaitingForReboot(true);
        },
      );
      return;
    }

    try {
      const refused = await startReleaseUpdate(release);
      if (refused !== null) {
        setApplyStatus(t('updates.failedToStartUpdateValue', { value: refused }));
        setApplying(false);
        return;
      }
      setApplyStatus(t('updates.updateStartedDownloadingOnDevice'));
    } catch (error) {
      setApplyStatus(t('updates.failedToStartUpdateError', { error }));
      setApplying(false);
    }
  };

  return { applying, applyProgress, applyStatus, waitingForReboot, applyUpdate };
};
