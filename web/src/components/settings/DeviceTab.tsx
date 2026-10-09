import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import type { SettingsTabProps } from './context';
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

const randomPasskey = () => {
  const values = new Uint32Array(1);
  crypto.getRandomValues(values);
  return String(100000 + (values[0] % 900000));
};

const DeviceTab: FunctionalComponent<SettingsTabProps> = ({ config, update, error, hw, status, goTo, deps, fix }) => {
  const auth = config.auth ?? { enabled: false, username: 'admin', password: '' };
  const ble = status?.ble;
  const alarm = ble?.alarm;
  const bleConfig = { ...defaultBleConfig, ...config.ble };
  const hasPasskey = bleConfig.passkey !== '';
  // A visible 6-digit value was typed since the last save (stored ones come back masked).
  const newPasskey = /^\d{6}$/.test(bleConfig.passkey) ? bleConfig.passkey : null;
  const [confirmUnpair, setConfirmUnpair] = useState(false);
  const [actionResult, setActionResult] = useState<{ type: 'success' | 'error'; text: string } | null>(null);

  const act = async (url: string, success: string) => {
    setActionResult(null);
    try {
      const response = await fetch(url, { method: 'POST' });
      const body = await response.json().catch(() => ({}));
      setActionResult(response.ok ? { type: 'success', text: success } : { type: 'error', text: body.error ?? 'Failed' });
    } catch {
      setActionResult({ type: 'error', text: 'Could not reach the device' });
    }
  };

  return (
    <>
      <SettingsCard title="Device">
        <Field label="Name" error={error('deviceName')} hint="Shown in N.I.N.A., alerts and Bluetooth.">
          <TextInput dataField="deviceName" value={config.deviceName} onInput={(v) => update(['deviceName'], v)} />
        </Field>
      </SettingsCard>

      <SettingsCard id="security" title="Security">
        <Toggle
          label="Password-protect changes"
          checked={auth.enabled}
          onChange={(v) => update(['auth', 'enabled'], v)}
          hint="Required to change settings, update or restart. Readings stay public."
        />
        {auth.enabled && (
          <div class="form-grid indent">
            <Field label="Username" error={error('auth.username')}>
              <TextInput dataField="auth.username" value={auth.username} onInput={(v) => update(['auth', 'username'], v)} />
            </Field>
            <Field label="Password" error={error('auth.password')} hint="Leave the mask to keep the current password.">
              <TextInput dataField="auth.password" type="password" value={auth.password} onInput={(v) => update(['auth', 'password'], v)} />
            </Field>
          </div>
        )}
        <DepToggle
          entry={deps.get('ota.enabled')}
          onFix={fix}
          label="Command-line uploads (ArduinoOTA)"
          checked={config.ota.enabled}
          onChange={(v) => update(['ota', 'enabled'], v)}
          hint="For pio run -t upload --upload-port <ip>. The Updates page doesn't need this."
        />
        {config.ota.enabled && (
          <div class="form-grid indent">
            <Field label="Upload password" error={error('ota.password') ?? error('otaPassword')}>
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

      <SettingsCard
        id="ble"
        title="Bluetooth"
        hint="Broadcasts safety and rain state, and can wake a paired phone."
        badge={ble?.active ? <StatusBadge tone="ok" label={`${ble.clients} connected`} /> : undefined}
      >
        {hw.bleAvailable === false && <Requires>Needs the Bluetooth firmware build, installed over USB.</Requires>}
        {hw.bleAvailable && (
          <>
            <DepToggle
              entry={deps.get('ble.enabled')}
              onFix={fix}
              label="Turn on Bluetooth"
              checked={bleConfig.enabled}
              onChange={(v) => update(['ble', 'enabled'], v)}
              hint="WiFi and Bluetooth share one radio, so the web UI and Alpaca respond more slowly while it's on."
            />
            {bleConfig.enabled && (
              <Group
                title="Phone alarm"
                aside={
                  alarm?.serviceActive ? (
                    <StatusBadge
                      tone={alarm.active ? 'bad' : alarm.bondedPhones > 0 ? 'ok' : 'warn'}
                      label={
                        alarm.active
                          ? `Alarm #${alarm.sequence} ringing`
                          : `${alarm.bondedPhones} phone${alarm.bondedPhones === 1 ? '' : 's'} paired`
                      }
                    />
                  ) : undefined
                }
              >
                <Field
                  label="Pairing passkey"
                  error={error('ble.passkey')}
                  hint="6 digits, typed on the phone when pairing. Only paired phones get alarms. Empty turns the alarm off."
                >
                  <div class="input-row">
                    <TextInput
                      dataField="ble.passkey"
                      type="password"
                      value={bleConfig.passkey}
                      onInput={(v) => update(['ble', 'passkey'], v)}
                    />
                    <ActionButton onClick={() => update(['ble', 'passkey'], randomPasskey())}>Generate</ActionButton>
                  </div>
                  {newPasskey && <Requires>New passkey {newPasskey} - it's hidden once saved. Paired phones need re-pairing.</Requires>}
                </Field>

                {hasPasskey && deps.get('ble.phoneAlarm').state === 'inactive' && (
                  <DepNote entry={deps.get('ble.phoneAlarm')} onFix={fix} prefix="Phone alarm off" />
                )}
                {hasPasskey ? (
                  <Note action={{ label: 'Choose events', onClick: () => goTo('alerts', 'alerts') }}>
                    Events set to Wake me ring paired phones.
                  </Note>
                ) : (
                  <Requires>Set a passkey to turn on the phone alarm.</Requires>
                )}

                {alarm?.serviceActive && (
                  <div class="btn-row">
                    {alarm.active && (
                      <ActionButton onClick={() => act('/api/ble/ack', 'Alarm acknowledged.')}>
                        Acknowledge alarm #{alarm.sequence}
                      </ActionButton>
                    )}
                    {confirmUnpair ? (
                      <>
                        <ActionButton
                          variant="danger"
                          onClick={() => {
                            setConfirmUnpair(false);
                            act('/api/ble/forget-bonds', 'All phones unpaired.');
                          }}
                        >
                          Unpair every phone
                        </ActionButton>
                        <ActionButton onClick={() => setConfirmUnpair(false)}>Cancel</ActionButton>
                      </>
                    ) : (
                      <ActionButton onClick={() => setConfirmUnpair(true)} disabled={alarm.bondedPhones === 0}>
                        Unpair all phones
                      </ActionButton>
                    )}
                    <ResultNote result={actionResult} />
                  </div>
                )}
              </Group>
            )}
          </>
        )}
      </SettingsCard>
    </>
  );
};

export default DeviceTab;
