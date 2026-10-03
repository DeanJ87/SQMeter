import { FunctionalComponent } from 'preact';
import type { SettingsTabProps } from './context';
import { Field, Requires, SettingsCard, StatusBadge, TextInput, Toggle } from './controls';

const DeviceTab: FunctionalComponent<SettingsTabProps> = ({ config, update, error, hw, status }) => {
  const auth = config.auth ?? { enabled: false, username: 'admin', password: '' };
  const ble = status?.ble;

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
              label="Broadcast safety and rain state over Bluetooth"
              checked={config.ble?.enabled ?? false}
              onChange={(v) => update(['ble', 'enabled'], v)}
              hint="Read-only. Takes effect after a restart."
            />
            <Requires tone="warn">
              WiFi and Bluetooth share one radio. With Bluetooth on, the web UI and Alpaca respond noticeably slower -
              on a weak WiFi link, enough to make the web UI hard to use.
            </Requires>
            {ble && ble.active !== (config.ble?.enabled ?? false) && <Requires>Restart the device to apply.</Requires>}
          </>
        )}
      </SettingsCard>
    </>
  );
};

export default DeviceTab;
