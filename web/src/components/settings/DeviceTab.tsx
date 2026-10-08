import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import type { SettingsTabProps } from './context';
import { ActionButton, Field, Group, Requires, ResultNote, SettingsCard, StatusBadge, TextInput, Toggle } from './controls';
import { defaultBleConfig } from './defaults';

const randomPasskey = () => {
  const values = new Uint32Array(1);
  crypto.getRandomValues(values);
  return String(100000 + (values[0] % 900000));
};

const DeviceTab: FunctionalComponent<SettingsTabProps> = ({ config, update, error, hw, status, goTo }) => {
  const auth = config.auth ?? { enabled: false, username: 'admin', password: '' };
  const ble = status?.ble;
  const alarm = ble?.alarm;
  const bleConfig = { ...defaultBleConfig, ...config.ble };
  const hasPasskey = bleConfig.passkey !== '';
  // A visible 6-digit value means it was typed since the last save (stored ones come back masked).
  const passkeyChanged = /^\d{6}$/.test(bleConfig.passkey);
  const [confirmForget, setConfirmForget] = useState(false);
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
      <SettingsCard title="Device" description="Identifies this SQMeter on your network, in N.I.N.A., in alerts and over Bluetooth.">
        <Field label="Device name" error={error('deviceName')}>
          <TextInput dataField="deviceName" value={config.deviceName} onInput={(v) => update(['deviceName'], v)} />
        </Field>
      </SettingsCard>

      <SettingsCard
        title="Security"
        description="Protects anything that changes the device. Sensor data and status stay readable without a password."
      >
        <Toggle
          label="Require a password to change settings, update or restart"
          checked={auth.enabled}
          onChange={(v) => update(['auth', 'enabled'], v)}
        />
        {auth.enabled && (
          <div class="grid grid-cols-1 md:grid-cols-2 gap-4 ml-7">
            <Field label="Username" error={error('auth.username')}>
              <TextInput dataField="auth.username" value={auth.username} onInput={(v) => update(['auth', 'username'], v)} />
            </Field>
            <Field
              label="Password"
              error={error('auth.password')}
              hint="Shown masked. Type a new password to change it; leave the mask to keep the current one."
            >
              <TextInput dataField="auth.password" type="password" value={auth.password} onInput={(v) => update(['auth', 'password'], v)} />
            </Field>
          </div>
        )}

        <div class="pt-4 border-t border-gray-700 space-y-3">
          <Toggle
            label="Allow command-line firmware uploads (ArduinoOTA)"
            checked={config.ota.enabled}
            onChange={(v) => update(['ota', 'enabled'], v)}
            hint="For `pio run -t upload --upload-port <ip>`. Updates from the Updates page don't need this."
          />
          {config.ota.enabled && (
            <Field label="Upload password" error={error('ota.password') ?? error('otaPassword')} class="ml-7">
              <TextInput dataField="ota.password" type="password" value={config.ota.password} onInput={(v) => update(['ota', 'password'], v)} />
            </Field>
          )}
        </div>
      </SettingsCard>

      <SettingsCard
        id="ble"
        title="Bluetooth"
        description="Broadcasts safety and rain state, and can wake a paired phone with an alarm that repeats until acknowledged."
        badge={
          ble?.available ? (
            <StatusBadge tone={ble.active ? 'ok' : 'off'} label={ble.active ? `On · ${ble.clients} connected` : 'Off'} />
          ) : undefined
        }
      >
        {hw.bleAvailable === false && (
          <Requires>
            Bluetooth isn't in this firmware. It needs the separate BLE build, installed over USB - see the Bluetooth guide in
            the docs.
          </Requires>
        )}
        {hw.bleAvailable && (
          <>
            <Toggle
              label="Turn on Bluetooth"
              checked={bleConfig.enabled}
              onChange={(v) => update(['ble', 'enabled'], v)}
              hint="Takes effect after a restart."
            />
            <Requires tone="warn">
              WiFi and Bluetooth share one radio. With Bluetooth on, the web UI and Alpaca respond noticeably slower - on a
              weak WiFi link, enough to make the web UI hard to use.
            </Requires>

            {bleConfig.enabled && (
              <Group
                title="Phone alarm"
                aside={
                  alarm?.serviceActive ? (
                    <StatusBadge
                      tone={alarm.active ? 'bad' : alarm.bondedPhones > 0 ? 'ok' : 'warn'}
                      label={alarm.active ? `Alarm #${alarm.sequence} ringing` : `${alarm.bondedPhones} phone${alarm.bondedPhones === 1 ? '' : 's'} paired`}
                    />
                  ) : undefined
                }
              >
                <Field
                  label="Pairing passkey"
                  error={error('ble.passkey')}
                  hint="6 digits. You type it on your phone when pairing; only paired phones receive alarms. Leave empty to turn the alarm off."
                >
                  <div class="flex gap-2 max-w-sm">
                    <div class="flex-1">
                      <TextInput dataField="ble.passkey" type="password" value={bleConfig.passkey} onInput={(v) => update(['ble', 'passkey'], v)} />
                    </div>
                    <ActionButton onClick={() => update(['ble', 'passkey'], randomPasskey())}>Generate</ActionButton>
                  </div>
                  {bleConfig.passkey && /^\d{6}$/.test(bleConfig.passkey) && (
                    <p class="mt-1 text-xs text-gray-400">
                      Passkey: <span class="font-mono text-white tracking-widest">{bleConfig.passkey}</span> - note it before saving; it's hidden afterwards.
                    </p>
                  )}
                </Field>

                {hasPasskey ? (
                  <>
                    <p class="text-sm text-gray-300">Wake me when:</p>
                    <div class="space-y-2 ml-1">
                      <Toggle label="The safety verdict goes unsafe" checked={bleConfig.alarmOnUnsafe} onChange={(v) => update(['ble', 'alarmOnUnsafe'], v)} />
                      <Toggle
                        label="Rain starts"
                        checked={bleConfig.alarmOnRain}
                        onChange={(v) => update(['ble', 'alarmOnRain'], v)}
                        blockedReason={!hw.rain.enabled ? 'Needs the rain sensor, which is turned off.' : null}
                      />
                      {!hw.rain.enabled && (
                        <button type="button" class="ml-7 -mt-1 text-xs text-cyan-300 hover:underline" onClick={() => goTo('sensors', 'rain')}>
                          Set up the rain sensor →
                        </button>
                      )}
                      <Toggle
                        label="A sensor stops responding"
                        checked={bleConfig.alarmOnSensorFault}
                        onChange={(v) => update(['ble', 'alarmOnSensorFault'], v)}
                        hint="Includes the RG-15 lens fault."
                      />
                    </div>
                  </>
                ) : (
                  <Requires>Set a passkey to turn on the phone alarm.</Requires>
                )}

                {alarm?.serviceActive && (
                  <div class="flex flex-wrap items-center gap-3 pt-1">
                    {alarm.active && (
                      <ActionButton onClick={() => act('/api/ble/ack', 'Alarm acknowledged.')}>Acknowledge alarm #{alarm.sequence}</ActionButton>
                    )}
                    {confirmForget ? (
                      <>
                        <span class="text-xs text-amber-300">Unpair every phone?</span>
                        <ActionButton onClick={() => { setConfirmForget(false); act('/api/ble/forget-bonds', 'All phones unpaired.'); }}>
                          Yes, unpair all
                        </ActionButton>
                        <ActionButton onClick={() => setConfirmForget(false)}>Cancel</ActionButton>
                      </>
                    ) : (
                      <ActionButton onClick={() => setConfirmForget(true)} disabled={alarm.bondedPhones === 0}>
                        Unpair all phones
                      </ActionButton>
                    )}
                    <ResultNote result={actionResult} />
                  </div>
                )}

                {ble?.active && hasPasskey && !alarm?.serviceActive && (
                  <Requires>Restart the device to start the phone alarm service.</Requires>
                )}
                {alarm?.serviceActive && passkeyChanged && (
                  <Requires tone="warn">Changing the passkey needs a restart, and paired phones must be unpaired and paired again.</Requires>
                )}
              </Group>
            )}

            {ble && ble.active !== bleConfig.enabled && <Requires>Restart the device to apply.</Requires>}
          </>
        )}
      </SettingsCard>
    </>
  );
};

export default DeviceTab;
