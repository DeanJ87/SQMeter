import { FunctionalComponent } from 'preact';
import { useRef } from 'preact/hooks';
import type { SystemStatus } from '../../types';
import { useBackup, useRestore, type Restore, type RestoreState } from '../../hooks/useBackup';
import { sectionOf, changesNothing, type Change, type RestorePreview, type Warning } from '../../lib/backupPreview';
import { ReadingRow, Note } from '../ui';
import { ActionButton, Field, Group, ResultNote, SettingsCard, TextInput } from './controls';
import { listReasons, restartLabel } from './restart';
import { showToast } from '../toast';
import { formatDateTime } from '../../i18n/format';
import { t, type MessageKey } from '../../i18n';

// Settings → Device → Backup & restore (specs/033-config-backup, mockups/).
// DS-01/DS-03 shared parts, DS-04 explanations in "?", DS-05 one path per row.

// The card each settings section belongs to, by its first path part.
const SECTION_LABELS: Record<string, () => string> = {
  general: () => t('settings.device.device'),
  wifi: () => 'WiFi',
  mqtt: () => 'MQTT',
  ota: () => t('settings.device.commandLineUploadsArduinoota'),
  auth: () => t('settings.device.security'),
  ntp: () => 'NTP',
  gps: () => 'GPS',
  rain: () => t('settings.sensors.rainSensor'),
  sensor: () => t('system.sensors'),
  skyAveraging: () => t('settings.sensors.skyQuality'),
  skyCalibration: () => t('settings.sensors.skyQuality'),
  cloudDetection: () => t('settings.sensors.cloudDetection'),
  alpaca: () => t('settings.tabs.safety'),
  ble: () => t('settings.device.bluetooth'),
  location: () => t('settings.time.location'),
  wind: () => t('settings.sensors.wind'),
  alerts: () => t('settings.alerts.alerts'),
};
const sectionLabel = (path: string) => SECTION_LABELS[sectionOf(path)]?.() ?? sectionOf(path);

const shown = (value: unknown) => (value === '' ? t('settings.backup.empty') : typeof value === 'string' ? value : JSON.stringify(value));

const REASONS: Record<RestorePreview['notApplicable'][number]['reason'], MessageKey> = {
  pin: 'settings.backup.pinNotOnThisBoard',
  analog: 'settings.backup.notAnAnalogPinHere',
  bluetooth: 'settings.backup.needsTheBluetoothBuild',
  'pushover-keys': 'settings.backup.needsThePushoverKeys',
};

const warningText = (warning: Warning) => {
  switch (warning.code) {
    case 'wifi-network':
      return t('settings.backup.joinsWifi', { ssid: warning.ssid });
    case 'device-password':
      return t('settings.backup.asksForTheBackupsPassword');
    case 'address':
      return t('settings.backup.movesToHostname', { hostname: warning.hostname });
    case 'imaging-app':
      return t('settings.backup.interruptsTheImagingApp');
    case 'newer-firmware':
      return t('settings.backup.madeByNewerFirmware', { firmware: warning.firmware });
  }
};

const ChangeGroups: FunctionalComponent<{ changes: Change[] }> = ({ changes }) => {
  const groups = new Map<string, Change[]>();
  changes.forEach((change) => groups.set(sectionLabel(change.path), [...(groups.get(sectionLabel(change.path)) ?? []), change]));
  return (
    <>
      {[...groups].map(([label, items]) => (
        <Group key={label} title={label}>
          {items.map((c) => (
            <ReadingRow
              key={c.path}
              label={c.path}
              labelClass="mono"
              valueClass="mono"
              value={t('settings.backup.fromTo', { from: shown(c.from), to: shown(c.to) })}
            />
          ))}
        </Group>
      ))}
    </>
  );
};

const SecretsGroup: FunctionalComponent<{ preview: RestorePreview }> = ({ preview }) => {
  const { state, changed, unchanged } = preview.secrets;
  return (
    <Group title={t('settings.backup.passwordsAndKeys')}>
      {state === 'kept' && <Note>{t('settings.backup.secretsKeptNoPassphrase')}</Note>}
      {state === 'none' && <Note>{t('settings.backup.secretsNotInTheBackup')}</Note>}
      {changed.map((path) => (
        <ReadingRow key={path} label={path} labelClass="mono" value={t('settings.backup.willChange')} />
      ))}
      {state === 'restored' && unchanged.length > 0 && <Note>{t('settings.backup.moreUnchanged', { count: unchanged.length })}</Note>}
    </Group>
  );
};

const PathGroup: FunctionalComponent<{ title: string; paths: string[]; value: string }> = ({ title, paths, value }) =>
  paths.length === 0 ? null : (
    <Group title={title}>
      {paths.map((path) => (
        <ReadingRow key={path} label={path} labelClass="mono" value={value} />
      ))}
    </Group>
  );

const PreviewView: FunctionalComponent<{ restore: Restore & { preview: RestorePreview }; state: RestoreState }> = ({ restore, state }) => {
  const { preview } = restore;
  const details = restore.details ?? { changes: [], kept: [], ignored: [], warnings: [] };
  const nothing = changesNothing(details, preview);
  return (
    <>
      <ChangeGroups changes={details.changes} />
      <SecretsGroup preview={preview} />
      {preview.notApplicable.length > 0 && (
        <Group title={t('settings.backup.cantApplyHere')}>
          {preview.notApplicable.map((n) => (
            <ReadingRow key={n.path} label={n.path} labelClass="mono" value={t(REASONS[n.reason], { value: shown(n.value) })} />
          ))}
        </Group>
      )}
      <PathGroup title={t('settings.backup.notInTheBackup')} paths={details.kept} value={t('settings.backup.kept')} />
      <PathGroup title={t('settings.backup.unknownToThisFirmware')} paths={details.ignored} value={t('settings.backup.ignored')} />
      {nothing && <Note>{t('settings.backup.nothingWillChange')}</Note>}
      {preview.restart.length > 0 && (
        <Note tone="warn">{t('settings.backup.restartsToApply', { listReasons: listReasons(preview.restart.map(restartLabel)) })}</Note>
      )}
      {details.warnings.map((warning) => (
        <Note key={warning.code} tone="warn">
          {warningText(warning)}
        </Note>
      ))}
      {state.restarting && <Note>{t('settings.backup.restoredRestarting')}</Note>}
      <div class="btn-row">
        <ActionButton onClick={state.apply} disabled={nothing || state.restarting} busy={state.busy === 'apply' || state.restarting}>
          {t('settings.backup.restore')}
        </ActionButton>
        {!state.restarting && <ActionButton onClick={state.cancel}>{t('settings.device.cancel')}</ActionButton>}
      </div>
    </>
  );
};

const FileRows: FunctionalComponent<{ restore: Restore }> = ({ restore }) => {
  const header = restore.file.header;
  return (
    <>
      <ReadingRow label={t('updates.file')} value={restore.file.name} valueClass="mono" />
      {header?.deviceName && <ReadingRow label={t('settings.device.device')} value={header.deviceName} />}
      {header?.createdAt && <ReadingRow label={t('settings.backup.made')} value={formatDateTime(new Date(header.createdAt))} />}
      {header?.firmware && <ReadingRow label={t('system.firmware')} value={header.firmware} />}
      {header?.build && (
        <ReadingRow
          label={t('system.build')}
          value={header.build === 'ble' ? t('settings.device.bluetooth') : t('settings.backup.standard')}
        />
      )}
    </>
  );
};

const RestoreView: FunctionalComponent<{ restore: Restore; state: RestoreState }> = ({ restore, state }) => {
  const asking = !restore.preview && !restore.refusal && restore.file.hasSecrets;
  return (
    <Group title={t('settings.backup.restore')}>
      <FileRows restore={restore} />
      {(asking || restore.wrongPassphrase) && (
        <Field
          label={t('settings.backup.passphraseForThisFile')}
          hint={t('settings.backup.passphraseForThisFileHint')}
          error={restore.wrongPassphrase ? (restore.refusal ?? undefined) : undefined}
        >
          <TextInput type="password" value={restore.passphrase} onInput={state.setPassphrase} />
        </Field>
      )}
      {restore.preview ? (
        <PreviewView restore={{ ...restore, preview: restore.preview }} state={state} />
      ) : (
        <>
          {restore.refusal && !restore.wrongPassphrase && <Note tone="bad">{restore.refusal}</Note>}
          <div class="btn-row">
            {(asking || restore.wrongPassphrase) && (
              <ActionButton onClick={() => void state.preview()} busy={state.busy === 'preview'}>
                {t('settings.backup.preview')}
              </ActionButton>
            )}
            <ActionButton onClick={state.cancel}>{t('settings.device.cancel')}</ActionButton>
          </div>
        </>
      )}
    </Group>
  );
};

const BackupCard: FunctionalComponent<{ deviceName: string; status: SystemStatus | null }> = ({ deviceName, status }) => {
  const clients = status?.alpaca?.clients;
  const imagingAppConnected = Boolean(clients && Object.values(clients).some((c) => c.connected || c.watching));
  const backup = useBackup(deviceName);
  const restoring = useRestore({ firmware: status?.firmware?.version, imagingAppConnected }, () =>
    showToast({ message: t('settings.backup.settingsRestored') }),
  );
  const fileInput = useRef<HTMLInputElement>(null);
  return (
    <SettingsCard id="backup" title={t('settings.backup.title')} hint={t('settings.backup.hint')}>
      <Field label={t('settings.backup.passphrase')} hint={t('settings.backup.passphraseHint')} error={backup.problems.passphrase}>
        <TextInput type="password" value={backup.passphrase} onInput={backup.setPassphrase} error={backup.problems.passphrase} />
      </Field>
      {backup.passphrase && (
        <Field label={t('settings.backup.repeatPassphrase')} error={backup.problems.repeat}>
          <TextInput type="password" value={backup.repeat} onInput={backup.setRepeat} error={backup.problems.repeat} />
        </Field>
      )}
      <div class="btn-row">
        <ActionButton onClick={() => void backup.backUp()} busy={backup.busy}>
          {t('settings.backup.backUpSettings')}
        </ActionButton>
        <ActionButton onClick={() => fileInput.current?.click()} disabled={restoring.busy !== null || restoring.restarting}>
          {t('settings.backup.restoreSettings')}
        </ActionButton>
        <input
          ref={fileInput}
          type="file"
          accept=".json,application/json"
          hidden
          aria-label={t('settings.backup.restoreSettings')}
          onChange={(event) => {
            const input = event.target as HTMLInputElement;
            const file = input.files?.[0];
            input.value = '';
            if (file) void restoring.chooseFile(file);
          }}
        />
      </div>
      <ResultNote result={backup.outcome} />
      {restoring.restore && <RestoreView restore={restoring.restore} state={restoring} />}
    </SettingsCard>
  );
};

export default BackupCard;
