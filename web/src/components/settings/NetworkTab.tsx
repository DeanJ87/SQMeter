import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import type { MQTTPublishGroups, WiFiNetwork } from '../../types';
import type { Hardware } from './hardware';
import { defaultHomeAssistant, defaultMqttPublish } from './defaults';
import type { SettingsTabProps } from './context';
import { ActionButton, Field, Group, NumberInput, ResultNote, SelectInput, SettingsCard, StatusBadge, TextInput, Toggle } from './controls';

type Result = { type: 'success' | 'error'; text: string } | null;

const PUBLISH_GROUPS: {
  key: keyof MQTTPublishGroups;
  label: string;
  hint?: string;
  needs?: (hw: Hardware) => boolean;
  blocked?: string;
}[] = [
  { key: 'sky', label: 'Sky quality and light' },
  { key: 'environment', label: 'Temperature, humidity, pressure' },
  { key: 'clouds', label: 'IR and cloud cover' },
  { key: 'gps', label: 'GPS', needs: (hw) => hw.gps.enabled, blocked: 'GPS is off.' },
  { key: 'rain', label: 'Rain', needs: (hw) => hw.rain.enabled, blocked: 'Rain sensor is off.' },
  { key: 'wind', label: 'Wind', needs: (hw) => hw.wind.enabled, blocked: 'Anemometer is off.' },
  { key: 'safety', label: 'Safe / unsafe', hint: 'Retained <base>/safe (1/0) and <base>/safety (reasons), on every change.' },
  { key: 'diagnostics', label: 'Diagnostics', hint: 'Light-sensor sample counts and RG-15 serial counters to <base>/diagnostics. For troubleshooting.' },
];

const NetworkTab: FunctionalComponent<SettingsTabProps & { originalWifiSsid: string | null }> = ({
  config,
  update,
  updateMany,
  error,
  hw,
  originalWifiSsid,
}) => {
  const [networks, setNetworks] = useState<WiFiNetwork[]>([]);
  const [scanning, setScanning] = useState(false);
  const [scanResult, setScanResult] = useState<Result>(null);
  const [testingMqtt, setTestingMqtt] = useState(false);
  const [mqttResult, setMqttResult] = useState<Result>(null);

  const scan = async () => {
    setScanning(true);
    setScanResult(null);
    try {
      const response = await fetch('/api/wifi/scan');
      const data = await response.json();
      setNetworks(data.networks || []);
    } catch {
      setScanResult({ type: 'error', text: 'Scan failed' });
    } finally {
      setScanning(false);
    }
  };

  const selectNetwork = (ssid: string) =>
    updateMany([
      [['wifi', 'ssid'], ssid],
      // A different network needs its own password; keep the stored one only
      // for the network the device is already on.
      [['wifi', 'password'], ssid !== originalWifiSsid ? '' : config.wifi.password],
    ]);

  const ssid = config.wifi.ssid;
  const showPassword = originalWifiSsid !== null && ssid !== originalWifiSsid;
  const networkOptions = [
    ...(ssid && !networks.some((n) => n.ssid === ssid) ? [{ value: ssid, label: ssid }] : []),
    ...networks.map((n) => ({ value: n.ssid, label: `${n.ssid}  ${n.rssi} dBm${n.encryption !== 'Open' ? ' 🔒' : ''}` })),
    { value: 'OTHER', label: 'Other...' },
  ];

  const testMqtt = async () => {
    setTestingMqtt(true);
    setMqttResult(null);
    try {
      const response = await fetch('/api/mqtt/test', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          broker: config.mqtt.broker,
          port: config.mqtt.port,
          username: config.mqtt.username,
          password: config.mqtt.password,
          clientId: `SQM-${config.deviceName || 'ESP32'}-Test`,
        }),
      });
      const result = await response.json();
      setMqttResult(result.success
        ? { type: 'success', text: result.message || 'Connected' }
        : { type: 'error', text: result.error || 'Connection failed' });
    } catch {
      setMqttResult({ type: 'error', text: 'Could not reach the device' });
    } finally {
      setTestingMqtt(false);
    }
  };

  const base = config.mqtt.topic || 'sqmeter';
  const publish = { ...defaultMqttPublish, ...config.mqtt.publish };
  const homeAssistant = { ...defaultHomeAssistant, ...config.mqtt.homeAssistant };

  const mqttBadge = !config.mqtt.enabled
    ? undefined
    : hw.mqtt.connected === null
      ? undefined
      : <StatusBadge tone={hw.mqtt.connected ? 'ok' : 'bad'} label={hw.mqtt.connected ? 'Connected' : 'Not connected'} />;

  return (
    <>
      <SettingsCard id="wifi" title="WiFi">
        <div class="form-grid">
          <Field label="Network" error={error('wifi.ssid')}>
            <div class="input-row">
              <SelectInput
                dataField="wifi.ssid"
                value={ssid === '' ? 'OTHER' : ssid}
                options={networkOptions}
                onChange={(v) => selectNetwork(v === 'OTHER' ? '' : v)}
              />
              <ActionButton onClick={scan} busy={scanning} busyLabel="Scanning...">Scan</ActionButton>
            </div>
            {ssid === '' && <TextInput dataField="wifi.ssid" value={ssid} placeholder="Network name" onInput={(v) => selectNetwork(v)} />}
            <ResultNote result={scanResult} />
          </Field>
          {showPassword && (
            <Field label="Password">
              <TextInput dataField="wifi.password" type="password" value={config.wifi.password} onInput={(v) => update(['wifi', 'password'], v)} />
            </Field>
          )}
          <Field label="Hostname" error={error('wifi.hostname')}>
            <TextInput dataField="wifi.hostname" value={config.wifi.hostname} onInput={(v) => update(['wifi', 'hostname'], v)} />
          </Field>
        </div>
        <Toggle label="Reconnect automatically" checked={config.wifi.autoReconnect} onChange={(v) => update(['wifi', 'autoReconnect'], v)} />
      </SettingsCard>

      <SettingsCard id="mqtt" title="MQTT" hint="Publishes readings to a broker, e.g. for Home Assistant." badge={mqttBadge}>
        <Toggle label="Publish to a broker" checked={config.mqtt.enabled} onChange={(v) => update(['mqtt', 'enabled'], v)} />
        {config.mqtt.enabled && (
          <>
            <div class="form-grid">
              <Field label="Broker" error={error('mqttBroker')}>
                <TextInput dataField="mqttBroker" value={config.mqtt.broker} placeholder="192.168.1.100" onInput={(v) => update(['mqtt', 'broker'], v)} />
              </Field>
              <Field label="Port" error={error('mqttPort')}>
                <NumberInput
                  dataField="mqttPort"
                  integer
                  min={1}
                  max={65535}
                  value={config.mqtt.port}
                  onChange={(v) => update(['mqtt', 'port'], Math.max(1, Math.min(65535, v || 1883)))}
                />
              </Field>
              <Field label="Username">
                <TextInput value={config.mqtt.username} placeholder="Optional" onInput={(v) => update(['mqtt', 'username'], v)} />
              </Field>
              <Field label="Password">
                <TextInput type="password" value={config.mqtt.password} placeholder="Optional" onInput={(v) => update(['mqtt', 'password'], v)} />
              </Field>
              <Field
                label="Base topic"
                error={error('mqttTopic')}
                hint={`Readings go to ${base}/state; also ${base}/availability, /safe, /safety, /alerts and /alerts/armed.`}
              >
                <TextInput dataField="mqttTopic" value={config.mqtt.topic} placeholder="sqmeter" onInput={(v) => update(['mqtt', 'topic'], v)} />
              </Field>
              <Field label="Publish every" error={error('mqttInterval')}>
                <NumberInput
                  dataField="mqttInterval"
                  integer
                  min={1}
                  max={3600}
                  unit="s"
                  value={config.mqtt.publishIntervalMs / 1000}
                  onChange={(v) => update(['mqtt', 'publishIntervalMs'], Math.max(1, v || 60) * 1000)}
                />
              </Field>
            </div>
            <div class="btn-row">
              <ActionButton onClick={testMqtt} busy={testingMqtt} busyLabel="Testing..." disabled={!config.mqtt.broker || !config.mqtt.port}>
                Test connection
              </ActionButton>
              <ResultNote result={mqttResult} />
            </div>

            <Group title="Publish">
              {PUBLISH_GROUPS.map((item) => (
                <Toggle
                  key={item.key}
                  label={item.label}
                  checked={publish[item.key]}
                  onChange={(v) => update(['mqtt', 'publish', item.key], v)}
                  hint={item.hint}
                  blockedReason={item.needs && !item.needs(hw) ? item.blocked : null}
                />
              ))}
            </Group>

            <Group title="Home Assistant">
              <Toggle
                label="MQTT discovery"
                checked={homeAssistant.enabled}
                onChange={(v) => update(['mqtt', 'homeAssistant', 'enabled'], v)}
                hint="Announces the readings, the safe flag and an alerts on/off switch, so they appear in Home Assistant without YAML."
              />
              {homeAssistant.enabled && (
                <div class="form-grid indent">
                  <Field label="Discovery prefix" error={error('mqtt.homeAssistant.discoveryPrefix')} hint="Home Assistant's default is homeassistant.">
                    <TextInput
                      value={homeAssistant.discoveryPrefix}
                      placeholder="homeassistant"
                      onInput={(v) => update(['mqtt', 'homeAssistant', 'discoveryPrefix'], v)}
                    />
                  </Field>
                </div>
              )}
            </Group>
          </>
        )}
      </SettingsCard>
    </>
  );
};

export default NetworkTab;
