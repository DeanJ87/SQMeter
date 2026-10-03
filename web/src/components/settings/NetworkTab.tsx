import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import type { WiFiNetwork } from '../../types';
import type { SettingsTabProps } from './context';
import { ActionButton, Field, NumberInput, ResultNote, SelectInput, SettingsCard, StatusBadge, TextInput, Toggle } from './controls';

type Result = { type: 'success' | 'error'; text: string } | null;

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
  const [scanError, setScanError] = useState<string | null>(null);
  const [testingMqtt, setTestingMqtt] = useState(false);
  const [mqttResult, setMqttResult] = useState<Result>(null);

  const scan = async () => {
    setScanning(true);
    setScanError(null);
    try {
      const response = await fetch('/api/wifi/scan');
      const data = await response.json();
      setNetworks(data.networks || []);
    } catch {
      setScanError('Failed to scan WiFi networks');
    } finally {
      setScanning(false);
    }
  };

  const selectNetwork = (ssid: string) =>
    updateMany([
      [['wifi', 'ssid'], ssid],
      // Switching networks needs that network's password; keep the stored
      // (masked) one only for the network the device is already on.
      [['wifi', 'password'], ssid !== originalWifiSsid ? '' : config.wifi.password],
    ]);

  const ssid = config.wifi.ssid;
  const knownInScan = networks.some((n) => n.ssid === ssid);
  const showPassword = originalWifiSsid !== null && ssid !== originalWifiSsid;
  const networkOptions = [
    ...(ssid && !knownInScan ? [{ value: ssid, label: `${ssid} (current)` }] : []),
    ...networks.map((n) => ({ value: n.ssid, label: `${n.ssid} (${n.rssi} dBm)${n.encryption !== 'Open' ? ' 🔒' : ''}` })),
    { value: 'OTHER', label: 'Other (type a name)...' },
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
        ? { type: 'success', text: result.message || 'Connection successful' }
        : { type: 'error', text: result.error || 'Connection failed' });
    } catch {
      setMqttResult({ type: 'error', text: 'Network error' });
    } finally {
      setTestingMqtt(false);
    }
  };

  const mqttBadge = !config.mqtt.enabled
    ? <StatusBadge tone="off" label="Off" />
    : hw.mqtt.connected === null
      ? undefined
      : <StatusBadge tone={hw.mqtt.connected ? 'ok' : 'bad'} label={hw.mqtt.connected ? 'Connected' : 'Not connected'} />;

  return (
    <>
      <SettingsCard id="wifi" title="WiFi">
        <Field label="Network" error={error('wifi.ssid')} hint={scanError ?? undefined}>
          <div class="flex gap-2">
            <div class="flex-1">
              <SelectInput
                dataField="wifi.ssid"
                value={ssid === '' ? 'OTHER' : ssid}
                options={networkOptions}
                onChange={(v) => selectNetwork(v === 'OTHER' ? '' : v)}
              />
            </div>
            <ActionButton onClick={scan} busy={scanning} busyLabel="Scanning...">Scan</ActionButton>
          </div>
          {ssid === '' && (
            <div class="mt-2">
              <TextInput dataField="wifi.ssid" value={ssid} placeholder="Network name (SSID)" onInput={(v) => selectNetwork(v)} />
            </div>
          )}
        </Field>
        {showPassword && (
          <Field label="Password">
            <TextInput dataField="wifi.password" type="password" value={config.wifi.password} onInput={(v) => update(['wifi', 'password'], v)} />
          </Field>
        )}
        <div class="grid grid-cols-1 md:grid-cols-2 gap-4">
          <Field label="Hostname" error={error('wifi.hostname')}>
            <TextInput dataField="wifi.hostname" value={config.wifi.hostname} onInput={(v) => update(['wifi', 'hostname'], v)} />
          </Field>
        </div>
        <Toggle label="Reconnect automatically" checked={config.wifi.autoReconnect} onChange={(v) => update(['wifi', 'autoReconnect'], v)} />
      </SettingsCard>

      <SettingsCard
        id="mqtt"
        title="MQTT"
        description="Publishes sensor readings to a broker, e.g. for Home Assistant. Alerts can also be sent over MQTT."
        badge={mqttBadge}
      >
        <Toggle label="Publish to an MQTT broker" checked={config.mqtt.enabled} onChange={(v) => update(['mqtt', 'enabled'], v)} />
        {config.mqtt.enabled && (
          <>
            <div class="grid grid-cols-1 md:grid-cols-3 gap-4">
              <Field label="Broker" error={error('mqttBroker')} class="md:col-span-2">
                <TextInput dataField="mqttBroker" value={config.mqtt.broker} placeholder="192.168.1.100" onInput={(v) => update(['mqtt', 'broker'], v)} />
              </Field>
              <Field label="Port" error={error('mqttPort')} hint="1883, or 8883 for TLS">
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
              <div />
              <Field label="Topic" error={error('mqttTopic')} hint="Letters, digits, / _ -">
                <TextInput dataField="mqttTopic" value={config.mqtt.topic} placeholder="sqm/data" onInput={(v) => update(['mqtt', 'topic'], v)} />
              </Field>
              <Field label="Publish every (seconds)" error={error('mqttInterval')}>
                <NumberInput
                  dataField="mqttInterval"
                  integer
                  min={1}
                  max={3600}
                  value={config.mqtt.publishIntervalMs / 1000}
                  onChange={(v) => update(['mqtt', 'publishIntervalMs'], Math.max(1, v || 60) * 1000)}
                />
              </Field>
            </div>
            <div class="flex flex-wrap items-center gap-3">
              <ActionButton onClick={testMqtt} busy={testingMqtt} busyLabel="Testing..." disabled={!config.mqtt.broker || !config.mqtt.port}>
                Test connection
              </ActionButton>
              <ResultNote result={mqttResult} />
            </div>
          </>
        )}
      </SettingsCard>
    </>
  );
};

export default NetworkTab;
