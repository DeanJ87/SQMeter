import { FunctionalComponent } from 'preact';
import type { SystemStatus } from '../../types';
import type { SettingsTabProps } from './context';
import { useDeviceActions, type DeviceActions } from '../../hooks/useDeviceActions';
import { Note } from '../ui';
import {
  ActionButton,
  DepNote,
  DepToggle,
  Field,
  Group,
  Requires,
  ResultNote,
  SettingsCard,
  StatusBadge,
  TextInput,
  Toggle,
} from './controls';
import { defaultBleConfig } from './defaults';
import { t } from '../../i18n';
import LanguageCard from './LanguageCard';

type BleAlarm = NonNullable<NonNullable<SystemStatus['ble']>['alarm']>;
type WithActions = SettingsTabProps & { actions: DeviceActions };

const randomPasskey = () => {
  const values = new Uint32Array(1);
  crypto.getRandomValues(values);
  return String(100000 + (values[0] % 900000));
};

// Password protection and command-line (ArduinoOTA) uploads.
const SecurityCard: FunctionalComponent<SettingsTabProps> = ({ config, update, error, deps, fix }) => {
  const auth = config.auth ?? { enabled: false, username: 'admin', password: '' };
  return (
    <SettingsCard id="security" title={t('settings.device.security')}>
      <Toggle
        label={t('settings.device.passwordProtectChanges')}
        checked={auth.enabled}
        onChange={(v) => update(['auth', 'enabled'], v)}
        hint={t('settings.device.requiredToChangeSettingsUpdate')}
      />
      {auth.enabled && (
        <div class="form-grid indent">
          <Field label={t('settings.device.username')} error={error('auth.username')}>
            <TextInput dataField="auth.username" value={auth.username} onInput={(v) => update(['auth', 'username'], v)} />
          </Field>
          <Field label={t('settings.device.password')} error={error('auth.password')} hint={t('settings.device.leaveTheMaskToKeep')}>
            <TextInput dataField="auth.password" type="password" value={auth.password} onInput={(v) => update(['auth', 'password'], v)} />
          </Field>
        </div>
      )}
      <DepToggle
        entry={deps.get('ota.enabled')}
        onFix={fix}
        label={t('settings.device.commandLineUploadsArduinoota')}
        checked={config.ota.enabled}
        onChange={(v) => update(['ota', 'enabled'], v)}
        hint={t('settings.device.forPioRunTUpload')}
      />
      {config.ota.enabled && (
        <div class="form-grid indent">
          <Field label={t('settings.device.uploadPassword')} error={error('ota.password') ?? error('otaPassword')}>
            <TextInput
              dataField="ota.password"
              type="password"
              value={config.ota.password}
              onInput={(v) => update(['ota', 'password'], v)}
            />
          </Field>
        </div>
      )}
    </SettingsCard>
  );
};

// Acknowledge the ringing alarm, and unpair every phone (with a confirm step).
const AlarmActions: FunctionalComponent<{ alarm: BleAlarm; actions: DeviceActions }> = ({ alarm, actions }) => {
  const { act, confirmUnpair, setConfirmUnpair, actionResult } = actions;
  return (
    <div class="btn-row">
      {alarm.active && (
        <ActionButton onClick={() => act('/api/ble/ack', t('settings.device.alarmAcknowledged'))}>
          {t('settings.device.acknowledgeAlarmSequence', { sequence: alarm.sequence })}
        </ActionButton>
      )}
      {confirmUnpair ? (
        <>
          <ActionButton
            variant="danger"
            onClick={() => {
              setConfirmUnpair(false);
              act('/api/ble/forget-bonds', t('settings.device.allPhonesUnpaired'));
            }}
          >
            {t('settings.device.unpairEveryPhone')}
          </ActionButton>
          <ActionButton onClick={() => setConfirmUnpair(false)}>{t('settings.device.cancel')}</ActionButton>
        </>
      ) : (
        <ActionButton onClick={() => setConfirmUnpair(true)} disabled={alarm.bondedPhones === 0}>
          {t('settings.device.unpairAllPhones')}
        </ActionButton>
      )}
      <ResultNote result={actionResult} />
    </div>
  );
};

const alarmBadge = (alarm: BleAlarm | undefined) => {
  if (!alarm?.serviceActive) return undefined;
  const tone = alarm.active ? 'bad' : alarm.bondedPhones > 0 ? 'ok' : 'warn';
  const label = alarm.active
    ? t('settings.device.alarmSequenceRinging', { sequence: alarm.sequence })
    : t('settings.device.phonesPaired', { count: alarm.bondedPhones });
  return <StatusBadge tone={tone} label={label} />;
};

// The BLE phone alarm: the pairing passkey, which events ring, and the alarm.
const PhoneAlarm: FunctionalComponent<WithActions> = ({ config, update, error, status, goTo, deps, fix, actions }) => {
  const alarm = status?.ble?.alarm;
  const bleConfig = { ...defaultBleConfig, ...config.ble };
  const hasPasskey = bleConfig.passkey !== '';
  // A visible 6-digit value was typed since the last save (stored ones come back masked).
  const newPasskey = /^\d{6}$/.test(bleConfig.passkey) ? bleConfig.passkey : null;
  return (
    <Group title={t('settings.device.phoneAlarm')} aside={alarmBadge(alarm)}>
      <Field label={t('settings.device.pairingPasskey')} error={error('ble.passkey')} hint={t('settings.device.6DigitsTypedOnThe')}>
        <div class="input-row">
          <TextInput dataField="ble.passkey" type="password" value={bleConfig.passkey} onInput={(v) => update(['ble', 'passkey'], v)} />
          <ActionButton onClick={() => update(['ble', 'passkey'], randomPasskey())}>{t('settings.device.generate')}</ActionButton>
        </div>
        {newPasskey && <Requires>{t('settings.device.newPasskeyNewpasskeyItS', { newPasskey })}</Requires>}
      </Field>

      {hasPasskey && deps.get('ble.phoneAlarm').state === 'inactive' && (
        <DepNote entry={deps.get('ble.phoneAlarm')} onFix={fix} prefix={t('settings.device.phoneAlarmOff')} />
      )}
      {hasPasskey ? (
        <Note action={{ label: t('settings.device.chooseEvents'), onClick: () => goTo('alerts', 'alerts') }}>
          {t('settings.device.eventsSetToWakeMe')}
        </Note>
      ) : (
        <Requires>{t('settings.device.setAPasskeyToTurn')}</Requires>
      )}

      {alarm?.serviceActive && <AlarmActions alarm={alarm} actions={actions} />}
    </Group>
  );
};

// Bluetooth on/off (BLE firmware build only) and the phone alarm.
const BluetoothCard: FunctionalComponent<WithActions> = (props) => {
  const { config, update, hw, status, deps, fix } = props;
  const ble = status?.ble;
  const bleConfig = { ...defaultBleConfig, ...config.ble };
  return (
    <SettingsCard
      id="ble"
      title={t('settings.device.bluetooth')}
      hint={t('settings.device.broadcastsSafetyAndRainState')}
      badge={ble?.active ? <StatusBadge tone="ok" label={t('settings.device.bleClientsConnected', { count: ble.clients })} /> : undefined}
    >
      {hw.bleAvailable === false && <Requires>{t('settings.device.needsTheBluetoothFirmwareBuild')}</Requires>}
      {hw.bleAvailable && (
        <>
          <DepToggle
            entry={deps.get('ble.enabled')}
            onFix={fix}
            label={t('settings.device.turnOnBluetooth')}
            checked={bleConfig.enabled}
            onChange={(v) => update(['ble', 'enabled'], v)}
            hint={t('settings.device.wifiAndBluetoothShareOne')}
          />
          {bleConfig.enabled && <PhoneAlarm {...props} />}
        </>
      )}
    </SettingsCard>
  );
};

const DeviceTab: FunctionalComponent<SettingsTabProps> = (props) => {
  const { config, update, error } = props;
  const actions = useDeviceActions();
  return (
    <>
      <SettingsCard title={t('settings.device.device')}>
        <Field label={t('settings.device.name')} error={error('deviceName')} hint={t('settings.device.shownInNIN')}>
          <TextInput dataField="deviceName" value={config.deviceName} onInput={(v) => update(['deviceName'], v)} />
        </Field>
      </SettingsCard>

      <LanguageCard language={config.language ?? 'en'} onChange={(code) => update(['language'], code)} />

      <SecurityCard {...props} />

      <BluetoothCard {...props} actions={actions} />
    </>
  );
};

export default DeviceTab;
