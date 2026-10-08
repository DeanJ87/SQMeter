import { FunctionalComponent } from 'preact';
import { useState, useEffect } from 'preact/hooks';
import { useWebSocket } from '../hooks/useWebSocket';
import { isVersionStale } from '../utils/versionCompare';
import type { GithubRelease, SystemStatus } from '../types';
import { Button, Card, Note, ProgressMeter, ReadingRow } from './ui';

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

  const isDemoMode = import.meta.env.VITE_DEMO_MODE === 'true';

  const checkForUpdates = async (selectedTrack: ReleaseTrack) => {
    setChecking(true);
    setCheckError('');
    try {
      const response = await fetch(`/api/updates/check?track=${selectedTrack}`);
      if (!response.ok) {
        const body = await response.json().catch(() => ({}));
        throw new Error(body.error || `HTTP ${response.status}`);
      }
      const data: GithubRelease[] = await response.json();
      setReleases(data);
      setSelectedTag(data[0]?.tag ?? '');
    } catch (error) {
      setCheckError(`Failed to check for updates: ${error}`);
      setReleases([]);
    } finally {
      setChecking(false);
    }
  };

  useEffect(() => {
    checkForUpdates(track);
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [track]);

  // Reflect firmware-pushed OTA progress/errors from /ws/status while applying
  useEffect(() => {
    if (!applying || !statusMsg) return;
    if ('type' in statusMsg && statusMsg.type === 'ota_progress') {
      setApplyProgress(statusMsg.progress);
      setApplyStatus(`Applying update... ${statusMsg.progress}%`);
      if (statusMsg.progress >= 100) {
        setApplyStatus('Update complete! Device is rebooting...');
        setApplying(false);
        setWaitingForReboot(true);
      }
    } else if ('error' in statusMsg) {
      setApplyStatus(`Update failed: ${statusMsg.error}`);
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
            setApplyStatus('Update successful. Device is back online.');
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

  const applyUpdate = async () => {
    if (!selectedRelease) return;

    setApplying(true);
    setApplyProgress(0);
    setApplyStatus('Starting update...');

    // Demo mode: the firmware isn't real, so fake progress the same way the
    // manual upload flow does (MSW can't push simulated WebSocket progress
    // messages timed against a fake download).
    if (isDemoMode) {
      let p = 0;
      const iv = setInterval(() => {
        p = Math.min(p + Math.random() * 12 + 6, 100);
        const rounded = Math.round(p);
        setApplyProgress(rounded);
        setApplyStatus(`Applying update... ${rounded}%`);
        if (p >= 100) {
          clearInterval(iv);
          setApplyStatus('Update complete! Device is rebooting...');
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
        setApplyStatus(`Failed to start update: ${body.error || `HTTP ${response.status}`}`);
        setApplying(false);
        return;
      }
      setApplyStatus('Update started, downloading on device...');
    } catch (error) {
      setApplyStatus(`Failed to start update: ${error}`);
      setApplying(false);
    }
  };

  const statusTone = (text: string) => (/successful|complete/i.test(text) ? 'ok' : /fail/i.test(text) ? 'bad' : 'muted');

  return (
    <Card title="Firmware" icon="upload" hint="Updates firmware and web UI together from GitHub releases. The device restarts when done.">
      <div class="card-body">
        <ReadingRow label="Installed" value={currentVersion ? `v${currentVersion}` : '--'} valueClass="tone-cyan" />
        <div class="form-grid">
          <div class="field">
            <label class="field-label" for="release-track">
              Release track
            </label>
            <select
              id="release-track"
              class="input"
              value={track}
              onChange={(e) => setTrack((e.target as HTMLSelectElement).value as ReleaseTrack)}
              disabled={checking || applying || waitingForReboot}
            >
              <option value="stable">Stable</option>
              <option value="beta">Beta</option>
            </select>
          </div>
          {releases.length > 0 && (
            <div class="field">
              <label class="field-label" for="release-select">
                Release
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
                    {r.name} ({r.tag})
                  </option>
                ))}
              </select>
            </div>
          )}
        </div>

        {checking && <Note>Checking GitHub...</Note>}
        {checkError && <Note tone="bad">{checkError}</Note>}
        {!checking && !checkError && releases.length === 0 && <Note>No {track} releases.</Note>}
        {selectedRelease && !applyStatus && (
          <Note tone={stale ? 'warn' : 'muted'}>{stale ? `A newer release (${selectedRelease.tag}) is available.` : 'Up to date.'}</Note>
        )}
        {applying && <ProgressMeter value={applyProgress} />}
        {applyStatus && <Note tone={statusTone(applyStatus)}>{applyStatus}</Note>}

        {releases.length > 0 && (
          <div class="btn-row">
            <Button
              variant="primary"
              onClick={applyUpdate}
              disabled={!selectedRelease || waitingForReboot}
              busy={applying}
              busyLabel="Updating..."
            >
              {waitingForReboot ? 'Waiting for restart...' : `Update to ${selectedRelease?.tag ?? '...'}`}
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

  useEffect(() => {
    let checkInterval: number | undefined;

    if (waitingForReboot) {
      setStatus('Restarting...');

      // Start checking if device is back online
      checkInterval = window.setInterval(async () => {
        try {
          const response = await fetch('/api/status');
          if (response.ok) {
            setStatus('Update successful. Device is back online.');
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
    setStatus('Uploading...');

    const endpoint = updateType === 'firmware' ? '/api/update' : '/api/update/fs';

    // Demo mode: MSW can't trigger XHR upload progress events, so fake it
    if (import.meta.env.VITE_DEMO_MODE === 'true') {
      let p = 0;
      const iv = setInterval(() => {
        p = Math.min(p + Math.random() * 12 + 6, 100);
        const rounded = Math.round(p);
        setUploadProgress(rounded);
        setStatus(`Uploading... ${rounded}%`);
        if (p >= 100) {
          clearInterval(iv);
          setStatus('Upload complete! Device is rebooting...');
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
          setStatus(`Uploading... ${progress}%`);
        }
      });

      xhr.addEventListener('load', () => {
        if (xhr.status === 200) {
          try {
            const response = JSON.parse(xhr.responseText);
            if (response.success === true) {
              setStatus('Upload complete! Device is rebooting...');
              setUploading(false);
              setWaitingForReboot(true);
            } else {
              const errorMsg = response.error || 'Unknown error';
              setStatus(`Upload failed: ${errorMsg}`);
              setUploading(false);
            }
          } catch (e) {
            setStatus('Upload complete! Device is rebooting...');
            setUploading(false);
            setWaitingForReboot(true);
          }
        } else {
          // Failures carry {"error": "..."} with a 4xx/5xx status.
          let errorMsg = `HTTP ${xhr.status}`;
          try {
            errorMsg = JSON.parse(xhr.responseText).error || errorMsg;
          } catch {
            // not JSON - keep the status code
          }
          setStatus(`Upload failed: ${errorMsg}`);
          setUploading(false);
        }
      });

      xhr.addEventListener('error', () => {
        if (uploadProgress === 100) {
          setStatus('Upload complete! Device is rebooting...');
          setUploading(false);
          setWaitingForReboot(true);
        } else {
          setStatus('Upload error occurred');
          setUploading(false);
        }
      });

      xhr.open('POST', endpoint);
      xhr.send(formData);
    } catch (error) {
      setStatus(`Failed to upload: ${error}`);
      setUploading(false);
    }
  };

  const fileName = updateType === 'firmware' ? 'firmware.bin' : 'littlefs.bin';
  const statusTone = /successful/i.test(status) ? 'ok' : /fail|error/i.test(status) ? 'bad' : 'muted';

  return (
    <div class="panel-page compact-page page-enter">
      <GithubUpdates />

      <Card
        title="Manual upload"
        icon="upload"
        hint="Flash a .bin you built yourself. Don't power off or close this tab until the device restarts. For both, upload firmware first, then the web UI."
      >
        <div class="card-body">
          <div class="form-grid">
            <div class="field">
              <label class="field-label" for="upload-type">
                Image
              </label>
              <select
                id="upload-type"
                class="input"
                value={updateType}
                onChange={(e) => setUpdateType((e.target as HTMLSelectElement).value as UpdateType)}
                disabled={uploading || waitingForReboot}
              >
                <option value="firmware">Firmware (firmware.bin)</option>
                <option value="filesystem">Web UI (littlefs.bin)</option>
              </select>
            </div>
            <div class="field">
              <label class="field-label" for="upload-file">
                File
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
          {file && (
            <Note>
              {file.name}, {(file.size / 1024).toFixed(0)} KB
            </Note>
          )}
          {uploading && <ProgressMeter value={uploadProgress} />}
          {status && <Note tone={statusTone}>{status}</Note>}
          <div class="btn-row">
            <Button onClick={handleUpload} disabled={!file || waitingForReboot} busy={uploading} busyLabel="Uploading...">
              {waitingForReboot ? 'Waiting for restart...' : `Upload ${fileName}`}
            </Button>
          </div>
        </div>
      </Card>
    </div>
  );
};

export default Updates;
