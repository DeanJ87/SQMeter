import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import { useAnnounceChange } from '../lib/a11y';
import { useWebSocket } from '../hooks/useWebSocket';
import { useReleaseApply, useReleases, type ReleaseTrack, type StatusMessage } from '../hooks/useReleaseUpdate';
import { useImageUpload, type UpdateType } from '../hooks/useImageUpload';
import { compareVersions, isVersionStale } from '../utils/versionCompare';
import type { GithubRelease } from '../types';
import { Button, Card, InfoTip, Note, ProgressMeter, ReadingRow } from './ui';
import { t } from '../i18n';
import { formatCount } from '../i18n/format';

// How a release relates to the installed firmware: -1 older (a downgrade),
// 0 installed, 1 newer, null unknown.
type Relation = number | null;

const releaseLabel = (release: GithubRelease, relation: Relation) => {
  const values = { name: release.name, tag: release.tag };
  if (relation === 0) return t('updates.releaseInstalled', values);
  if (relation === -1) return t('updates.releaseOlder', values);
  return t('updates.releaseOption', values);
};

const statusTone = (text: string) => (/successful|complete/i.test(text) ? 'ok' : /fail/i.test(text) ? 'bad' : 'muted');

const ReleaseSelect: FunctionalComponent<{
  releases: GithubRelease[];
  selectedTag: string;
  onSelect: (tag: string) => void;
  relation: (tag: string) => Relation;
  disabled: boolean;
}> = ({ releases, selectedTag, onSelect, relation, disabled }) => (
  <div class="field">
    <label class="field-label" for="release-select">
      {t('updates.release')}
    </label>
    <select
      id="release-select"
      class="input"
      value={selectedTag}
      onChange={(e) => onSelect((e.target as HTMLSelectElement).value)}
      disabled={disabled}
    >
      {releases.map((r) => (
        <option key={r.tag} value={r.tag}>
          {releaseLabel(r, relation(r.tag))}
        </option>
      ))}
    </select>
  </div>
);

// Whether the selected release is newer, installed or a downgrade.
const ReleaseVerdict: FunctionalComponent<{
  release: GithubRelease;
  currentVersion: string | undefined;
  relation: Relation;
}> = ({ release, currentVersion, relation }) => {
  if (relation === -1) {
    // Releases before this one can't fetch the release list (it outgrew their
    // buffer), so a device downgraded to one can only be updated by upload.
    const noOtaCheck = (compareVersions(release.tag, '0.2.0-beta.3') ?? 0) < 0;
    return (
      <Note tone="warn">
        {t('updates.downgradeWarning', { version: currentVersion })}{' '}
        <InfoTip text={noOtaCheck ? t('updates.downgradeHintNoOta') : t('updates.downgradeHint')} />
      </Note>
    );
  }
  const stale = currentVersion ? isVersionStale(currentVersion, release.tag) : false;
  return (
    <Note tone={stale ? 'warn' : 'muted'}>
      {stale
        ? t('updates.aNewerReleaseTagIs', { tag: release.tag })
        : relation === 0
          ? t('updates.thisReleaseIsInstalled')
          : t('updates.upToDate')}
    </Note>
  );
};

const applyLabel = (waitingForReboot: boolean, relation: Relation, tag: string | undefined) => {
  if (waitingForReboot) return t('updates.waitingForRestart');
  if (relation === 0) return t('updates.installed');
  return t('updates.actionToTag', {
    action: relation === -1 ? t('updates.downgrade') : t('updates.update'),
    tag: tag ?? '...',
  });
};

const ApplyButton: FunctionalComponent<{
  release: GithubRelease | undefined;
  relation: Relation;
  applying: boolean;
  waitingForReboot: boolean;
  onApply: () => void;
}> = ({ release, relation, applying, waitingForReboot, onApply }) => (
  <div class="btn-row">
    <Button
      variant={relation === -1 ? 'danger' : 'primary'}
      onClick={onApply}
      disabled={!release || relation === 0 || waitingForReboot}
      busy={applying}
      busyLabel={t('common.updating')}
    >
      {applyLabel(waitingForReboot, relation, release?.tag)}
    </Button>
  </div>
);

const TrackSelect: FunctionalComponent<{ track: ReleaseTrack; onChange: (track: ReleaseTrack) => void; disabled: boolean }> = ({
  track,
  onChange,
  disabled,
}) => (
  <div class="field">
    <label class="field-label" for="release-track">
      {t('updates.releaseTrack')}
    </label>
    <select
      id="release-track"
      class="input"
      value={track}
      onChange={(e) => onChange((e.target as HTMLSelectElement).value as ReleaseTrack)}
      disabled={disabled}
    >
      <option value="stable">{t('updates.stable')}</option>
      <option value="beta">{t('updates.beta')}</option>
    </select>
  </div>
);

// The GitHub check's state: checking, failed, or nothing on this track.
const CheckNotes: FunctionalComponent<{ checking: boolean; checkError: string; empty: boolean; track: ReleaseTrack }> = ({
  checking,
  checkError,
  empty,
  track,
}) => (
  <>
    {checking && <Note>{t('updates.checkingGithub')}</Note>}
    {checkError && <Note tone="bad">{checkError}</Note>}
    {!checking && !checkError && empty && <Note>{t('updates.noTrackReleases', { track })}</Note>}
  </>
);

const GithubUpdates: FunctionalComponent = () => {
  const { data: statusMsg } = useWebSocket<StatusMessage>('/ws/status');
  const currentStatus = statusMsg && 'firmware' in statusMsg ? statusMsg : null;
  const currentVersion = currentStatus?.firmware?.version;

  const [track, setTrack] = useState<ReleaseTrack>('stable');
  const { releases, checking, checkError, selectedTag, setSelectedTag } = useReleases(track);
  const { applying, applyProgress, applyStatus, waitingForReboot, applyUpdate } = useReleaseApply(statusMsg);

  // Announce how an update ended, once (spec 022 FR-009 edge case).
  useAnnounceChange(applyStatus, (text) => (/complete|successful|fail|error/i.test(text) ? text : null));

  const selectedRelease = releases.find((r) => r.tag === selectedTag);
  const relation = (tag: string): Relation => (currentVersion ? compareVersions(tag, currentVersion) : null);
  const selectedRelation = selectedRelease ? relation(selectedRelease.tag) : null;
  const busy = applying || waitingForReboot;

  return (
    <Card title={t('updates.firmware')} icon="upload" hint={t('updates.updatesFirmwareAndWebUi')}>
      <div class="card-body">
        <ReadingRow label={t('updates.installed')} value={currentVersion ? `v${currentVersion}` : '--'} valueClass="tone-cyan" />
        <div class="form-grid">
          <TrackSelect track={track} onChange={setTrack} disabled={checking || busy} />
          {releases.length > 0 && (
            <ReleaseSelect releases={releases} selectedTag={selectedTag} onSelect={setSelectedTag} relation={relation} disabled={busy} />
          )}
        </div>

        <CheckNotes checking={checking} checkError={checkError} empty={releases.length === 0} track={track} />
        {selectedRelease && !applyStatus && (
          <ReleaseVerdict release={selectedRelease} currentVersion={currentVersion} relation={selectedRelation} />
        )}
        {applying && <ProgressMeter value={applyProgress} label={t('updates.updateProgress')} announceSteps />}
        {applyStatus && <Note tone={statusTone(applyStatus)}>{applyStatus}</Note>}

        {releases.length > 0 && (
          <ApplyButton
            release={selectedRelease}
            relation={selectedRelation}
            applying={applying}
            waitingForReboot={waitingForReboot}
            onApply={() => applyUpdate(selectedRelease)}
          />
        )}
      </div>
    </Card>
  );
};

const uploadStatusTone = (status: string) => (/successful/i.test(status) ? 'ok' : /fail|error/i.test(status) ? 'bad' : 'muted');

const Updates: FunctionalComponent = () => {
  const [updateType, setUpdateType] = useState<UpdateType>('firmware');
  const { file, setFile, uploading, uploadProgress, status, setStatus, waitingForReboot, handleUpload } = useImageUpload();

  // Announce how an update ended, once (spec 022 FR-009 edge case).
  useAnnounceChange(status, (text) => (/complete|successful|fail|error/i.test(text) ? text : null));

  const fileName = updateType === 'firmware' ? 'firmware.bin' : 'littlefs.bin';

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
          {file && <Note>{t('updates.nameFixedKb', { name: file.name, fixed: formatCount(Math.round(file.size / 1024)) })}</Note>}
          {uploading && <ProgressMeter value={uploadProgress} label={t('updates.uploadProgress')} announceSteps />}
          {status && <Note tone={uploadStatusTone(status)}>{status}</Note>}
          <div class="btn-row">
            <Button
              onClick={() => handleUpload(updateType)}
              disabled={!file || waitingForReboot}
              busy={uploading}
              busyLabel={t('common.uploading')}
            >
              {waitingForReboot ? t('updates.waitingForRestart') : t('updates.uploadFilename', { fileName })}
            </Button>
          </div>
        </div>
      </Card>
    </div>
  );
};

export default Updates;
