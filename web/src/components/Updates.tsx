import { FunctionalComponent } from 'preact';
import { useState, useEffect } from 'preact/hooks';
import { useAnnounceChange } from '../lib/a11y';
import { useWebSocket } from '../hooks/useWebSocket';
import { compareVersions, isVersionStale } from '../utils/versionCompare';
import type { GithubRelease, SystemStatus } from '../types';
import { Button, Card, Note, ProgressMeter, ReadingRow } from './ui';
import { t } from '../i18n';
import { deviceError } from '../i18n/deviceMessage';

type UpdateType = 'firmware' | 'filesystem';
type ReleaseTrack = 'stable' | 'beta';
type StatusMessage = SystemStatus | { type: 'ota_progress'; progress: number } | { error: string };

const GithubUpdates: FunctionalComponent = () => {
  const { data: statusMsg } = useWebSocket<StatusMessage>('/ws/status');
  const currentStatus = statusMsg && 'firmware' in statusMsg ? statusMsg : null;
  const currentVersion = currentStatus?.firmware?.version;

  const [track, setTrack] = useState<ReleaseTrack>('stable');
  const [releases, setReleases] = useState<GithubRelease[]>([]);
  const [checking, setChecking] = useState(false);
  const [checkError, setCheckError] = useState('');
  const [selectedTag, setSelectedTag] = useState('');
  const [applying, setApplying] = useState(false);
  const [applyProgress, setApplyProgress] = useState(0);
  const [applyStatus, setApplyStatus] = useState('');
  const [waitingForReboot, setWaitingForReboot] = useState(false);

  // Announce how an update ended, once (spec 022 FR-009 edge case).
  useAnnounceChange(applyStatus, (text) => (/complete|successful|fail|error/i.test(text) ? text : null));

  const isDemoMode = import.meta.env.VITE_DEMO_MODE === 'true';

  const checkForUpdates = async (selectedTrack: ReleaseTrack) => {
    setChecking(true);
    setCheckError('');
    try {
      const response = await fetch(`/api/updates/check?track=${selectedTrack}`);
      if (!response.ok) {
        const body = await response.json().catch(() => ({}));
        throw new Error(deviceError(body, `HTTP ${response.status}`));
      }
      const data: GithubRelease[] = await response.json();
      setReleases(data);
      setSelectedTag(data[0]?.tag ?? '');
    } catch (error) {
      setCheckError(t('updates.failedToCheckForUpdates', { error }));
      setReleases([]);
    } finally {
      setChecking(false);
    }
  };

  useEffect(() => {
    checkForUpdates(track);
  }, [track]);

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

  useEffect(() => {
    let checkInterval: number | undefined;

    if (waitingForReboot) {
      checkInterval = window.setInterval(async () => {
        try {
          const response = await fetch('/api/status');
          if (response.ok) {
            setApplyStatus(t('updates.updateSuccessfulDeviceIsBack'));
            setWaitingForReboot(false);
            window.clearInterval(checkInterval);
          }
        } catch {
          // Still offline, keep waiting
        }
      }, 2000);
    }

    return () => {
      if (checkInterval) window.clearInterval(checkInterval);
    };
  }, [waitingForReboot]);

  const selectedRelease = releases.find((r) => r.tag === selectedTag);
  const stale = currentVersion && selectedRelease ? isVersionStale(currentVersion, selectedRelease.tag) : false;
  // -1 older than installed (a downgrade), 0 installed, 1 newer, null unknown.
  const relation = (tag: string) => (currentVersion ? compareVersions(tag, currentVersion) : null);
  const selectedRelation = selectedRelease ? relation(selectedRelease.tag) : null;
  const installed = selectedRelation === 0;
  const downgrade = selectedRelation === -1;
  // Releases before this one can't fetch the release list (it outgrew their
  // buffer), so a device downgraded to one can only be updated by upload.
  const noOtaCheck = selectedRelease ? (compareVersions(selectedRelease.tag, '0.2.0-beta.3') ?? 0) < 0 : false;

  const applyUpdate = async () => {
    if (!selectedRelease) return;

    setApplying(true);
    setApplyProgress(0);
    setApplyStatus(t('updates.startingUpdate'));

    // Demo mode: the firmware isn't real, so fake progress the same way the
    // manual upload flow does (MSW can't push simulated WebSocket progress
    // messages timed against a fake download).
    if (isDemoMode) {
      let p = 0;
      const iv = setInterval(() => {
        p = Math.min(p + Math.random() * 12 + 6, 100);
        const rounded = Math.round(p);
        setApplyProgress(rounded);
        setApplyStatus(t('updates.applyingUpdateRounded', { rounded }));
        if (p >= 100) {
          clearInterval(iv);
          setApplyStatus(t('updates.updateCompleteDeviceIsRebooting'));
          setApplying(false);
          setWaitingForReboot(true);
        }
      }, 250);
      return;
    }

    try {
      const response = await fetch('/api/updates/apply', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          firmwareAssetUrl: selectedRelease.firmwareAssetUrl,
          firmwareAssetSize: selectedRelease.firmwareAssetSize,
          fsAssetUrl: selectedRelease.fsAssetUrl,
          fsAssetSize: selectedRelease.fsAssetSize,
        }),
      });
      const body = await response.json().catch(() => ({}));
      if (!response.ok || body.success !== true) {
        setApplyStatus(t('updates.failedToStartUpdateValue', { value: deviceError(body, `HTTP ${response.status}`) }));
        setApplying(false);
        return;
      }
      setApplyStatus(t('updates.updateStartedDownloadingOnDevice'));
    } catch (error) {
      setApplyStatus(t('updates.failedToStartUpdateError', { error }));
      setApplying(false);
    }
  };

  const statusTone = (text: string) => (/successful|complete/i.test(text) ? 'ok' : /fail/i.test(text) ? 'bad' : 'muted');

  return (
    <Card title={t('updates.firmware')} icon="upload" hint={t('updates.updatesFirmwareAndWebUi')}>
      <div class="card-body">
        <ReadingRow label={t('updates.installed')} value={currentVersion ? `v${currentVersion}` : '--'} valueClass="tone-cyan" />
        <div class="form-grid">
          <div class="field">
            <label class="field-label" for="release-track">
              {t('updates.releaseTrack')}
            </label>
            <select
              id="release-track"
              class="input"
              value={track}
              onChange={(e) => setTrack((e.target as HTMLSelectElement).value as ReleaseTrack)}
              disabled={checking || applying || waitingForReboot}
            >
              <option value="stable">{t('updates.stable')}</option>
              <option value="beta">{t('updates.beta')}</option>
            </select>
          </div>
          {releases.length > 0 && (
            <div class="field">
              <label class="field-label" for="release-select">
                {t('updates.release')}
              </label>
              <select
                id="release-select"
                class="input"
                value={selectedTag}
                onChange={(e) => setSelectedTag((e.target as HTMLSelectElement).value)}
                disabled={applying || waitingForReboot}
              >
                {releases.map((r) => (
                  <option key={r.tag} value={r.tag}>
                    {relation(r.tag) === 0
                      ? t('updates.releaseInstalled', { name: r.name, tag: r.tag })
                      : relation(r.tag) === -1
                        ? t('updates.releaseOlder', { name: r.name, tag: r.tag })
                        : t('updates.releaseOption', { name: r.name, tag: r.tag })}
                  </option>
                ))}
              </select>
            </div>
          )}
        </div>

        {checking && <Note>{t('updates.checkingGithub')}</Note>}
        {checkError && <Note tone="bad">{checkError}</Note>}
        {!checking && !checkError && releases.length === 0 && <Note>{t('updates.noTrackReleases', { track })}</Note>}
        {selectedRelease && !applyStatus && !downgrade && (
          <Note tone={stale ? 'warn' : 'muted'}>
            {stale
              ? t('updates.aNewerReleaseTagIs', { tag: selectedRelease.tag })
              : installed
                ? t('updates.thisReleaseIsInstalled')
                : t('updates.upToDate')}
          </Note>
        )}
        {selectedRelease && !applyStatus && downgrade && (
          <Note tone="warn">
            {t('updates.downgradeWarning', { tag: selectedRelease.tag, version: currentVersion })}
            {noOtaCheck && t('updates.itAlsoCanTCheck')}
          </Note>
        )}
        {applying && <ProgressMeter value={applyProgress} label={t('updates.updateProgress')} announceSteps />}
        {applyStatus && <Note tone={statusTone(applyStatus)}>{applyStatus}</Note>}

        {releases.length > 0 && (
          <div class="btn-row">
            <Button
              variant={downgrade ? 'danger' : 'primary'}
              onClick={applyUpdate}
              disabled={!selectedRelease || installed || waitingForReboot}
              busy={applying}
              busyLabel={t('common.updating')}
            >
              {waitingForReboot
                ? t('updates.waitingForRestart')
                : installed
                  ? t('updates.installed')
                  : t('updates.actionToTag', {
                      action: downgrade ? t('updates.downgrade') : t('updates.update'),
                      tag: selectedRelease?.tag ?? '...',
                    })}
            </Button>
          </div>
        )}
      </div>
    </Card>
  );
};

const Updates: FunctionalComponent = () => {
  const [updateType, setUpdateType] = useState<UpdateType>('firmware');
  const [file, setFile] = useState<File | null>(null);
  const [uploading, setUploading] = useState(false);
  const [uploadProgress, setUploadProgress] = useState(0);
  const [status, setStatus] = useState<string>('');
  const [waitingForReboot, setWaitingForReboot] = useState(false);

  // Announce how an update ended, once (spec 022 FR-009 edge case).
  useAnnounceChange(status, (text) => (/complete|successful|fail|error/i.test(text) ? text : null));

  useEffect(() => {
    let checkInterval: number | undefined;

    if (waitingForReboot) {
      setStatus(t('common.restarting'));

      // Start checking if device is back online
      checkInterval = window.setInterval(async () => {
        try {
          const response = await fetch('/api/status');
          if (response.ok) {
            setStatus(t('updates.updateSuccessfulDeviceIsBack'));
            setWaitingForReboot(false);
            setFile(null);
            window.clearInterval(checkInterval);
          }
        } catch {
          // Still offline, keep waiting
        }
      }, 2000);
    }

    return () => {
      if (checkInterval) {
        window.clearInterval(checkInterval);
      }
    };
  }, [waitingForReboot]);

  const handleUpload = async () => {
    if (!file) return;

    setUploading(true);
    setUploadProgress(0);
    setStatus(t('common.uploading'));

    const endpoint = updateType === 'firmware' ? '/api/update' : '/api/update/fs';

    // Demo mode: MSW can't trigger XHR upload progress events, so fake it
    if (import.meta.env.VITE_DEMO_MODE === 'true') {
      let p = 0;
      const iv = setInterval(() => {
        p = Math.min(p + Math.random() * 12 + 6, 100);
        const rounded = Math.round(p);
        setUploadProgress(rounded);
        setStatus(t('updates.uploadingRounded', { rounded }));
        if (p >= 100) {
          clearInterval(iv);
          setStatus(t('updates.uploadCompleteDeviceIsRebooting'));
          setUploading(false);
          setWaitingForReboot(true);
        }
      }, 250);
      return;
    }

    const formData = new FormData();
    formData.append('update', file);

    try {
      const xhr = new XMLHttpRequest();

      xhr.upload.addEventListener('progress', (e) => {
        if (e.lengthComputable) {
          const progress = Math.round((e.loaded / e.total) * 100);
          setUploadProgress(progress);
          setStatus(t('updates.uploadingProgress', { progress }));
        }
      });

      xhr.addEventListener('load', () => {
        if (xhr.status === 200) {
          try {
            const response = JSON.parse(xhr.responseText);
            if (response.success === true) {
              setStatus(t('updates.uploadCompleteDeviceIsRebooting'));
              setUploading(false);
              setWaitingForReboot(true);
            } else {
              const errorMsg = deviceError(response, t('updates.unknownError'));
              setStatus(t('updates.uploadFailedErrormsg', { errorMsg }));
              setUploading(false);
            }
          } catch (e) {
            setStatus(t('updates.uploadCompleteDeviceIsRebooting'));
            setUploading(false);
            setWaitingForReboot(true);
          }
        } else {
          // Failures carry {"error": "..."} with a 4xx/5xx status.
          let errorMsg = `HTTP ${xhr.status}`;
          try {
            errorMsg = deviceError(JSON.parse(xhr.responseText), errorMsg);
          } catch {
            // not JSON - keep the status code
          }
          setStatus(t('updates.uploadFailedErrormsg', { errorMsg }));
          setUploading(false);
        }
      });

      xhr.addEventListener('error', () => {
        if (uploadProgress === 100) {
          setStatus(t('updates.uploadCompleteDeviceIsRebooting'));
          setUploading(false);
          setWaitingForReboot(true);
        } else {
          setStatus(t('updates.uploadErrorOccurred'));
          setUploading(false);
        }
      });

      xhr.open('POST', endpoint);
      xhr.send(formData);
    } catch (error) {
      setStatus(t('updates.failedToUploadError', { error }));
      setUploading(false);
    }
  };

  const fileName = updateType === 'firmware' ? 'firmware.bin' : 'littlefs.bin';
  const statusTone = /successful/i.test(status) ? 'ok' : /fail|error/i.test(status) ? 'bad' : 'muted';

  return (
    <div class="panel-page compact-page page-enter">
      <GithubUpdates />

      <Card title={t('updates.manualUpload')} icon="upload" hint={t('updates.flashABinYouBuilt')}>
        <div class="card-body">
          <div class="form-grid">
            <div class="field">
              <label class="field-label" for="upload-type">
                {t('updates.image')}
              </label>
              <select
                id="upload-type"
                class="input"
                value={updateType}
                onChange={(e) => setUpdateType((e.target as HTMLSelectElement).value as UpdateType)}
                disabled={uploading || waitingForReboot}
              >
                <option value="firmware">{t('updates.firmwareFirmwareBin')}</option>
                <option value="filesystem">{t('updates.webUiLittlefsBin')}</option>
              </select>
            </div>
            <div class="field">
              <label class="field-label" for="upload-file">
                {t('updates.file')}
              </label>
              <input
                id="upload-file"
                type="file"
                accept=".bin"
                class="input"
                onChange={(e) => {
                  const files = (e.target as HTMLInputElement).files;
                  setFile(files ? files[0] : null);
                  setStatus('');
                }}
                disabled={uploading || waitingForReboot}
              />
            </div>
          </div>
          {file && <Note>{t('updates.nameFixedKb', { name: file.name, fixed: (file.size / 1024).toFixed(0) })}</Note>}
          {uploading && <ProgressMeter value={uploadProgress} label={t('updates.uploadProgress')} announceSteps />}
          {status && <Note tone={statusTone}>{status}</Note>}
          <div class="btn-row">
            <Button onClick={handleUpload} disabled={!file || waitingForReboot} busy={uploading} busyLabel={t('common.uploading')}>
              {waitingForReboot ? t('updates.waitingForRestart') : t('updates.uploadFilename', { fileName })}
            </Button>
          </div>
        </div>
      </Card>
    </div>
  );
};

export default Updates;
