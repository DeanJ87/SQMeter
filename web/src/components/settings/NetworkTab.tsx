import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import type { MQTTPublishGroups } from '../../types';
import { useWifiScan } from '../../hooks/useWifiScan';
import { defaultHomeAssistant, defaultMqttPublish } from './defaults';
import type { SettingsTabProps } from './context';
import {
  ActionButton,
  DepNote,
  DepToggle,
  Field,
  Group,
  NumberInput,
  ResultNote,
  SelectInput,
  SettingsCard,
  StatusBadge,
  TextInput,
  Toggle,
} from './controls';
import { t } from '../../i18n';
import { deviceError } from '../../i18n/deviceMessage';

type Result = { type: 'success' | 'error'; text: string } | null;

const PUBLISH_GROUPS: {
  key: keyof MQTTPublishGroups;
  label: string;
  hint?: string;
}[] = [
  { key: 'sky', label: t('settings.network.skyQualityAndLight') },
  { key: 'environment', label: t('settings.network.temperatureHumidityPressure') },
  { key: 'clouds', label: t('settings.network.irAndCloudCover') },
  { key: 'gps', label: 'GPS' },
  { key: 'rain', label: t('settings.network.rain') },
  { key: 'wind', label: t('settings.network.wind') },
  { key: 'safety', label: t('settings.network.safeUnsafe'), hint: t('settings.network.retainedBaseSafe10') },
  {
    key: 'diagnostics',
    label: t('settings.network.diagnostics'),
    hint: t('settings.network.lightSensorSampleCountsAnd'),
  },
];

const NetworkTab: FunctionalComponent<SettingsTabProps & { originalWifiSsid: string | null }> = ({
  config,
  update,
  updateMany,
  error,
  hw,
  originalWifiSsid,
  deps,
  fix,
}) => {
  const { networks, scanning, error: scanError, scan } = useWifiScan();
  const [testingMqtt, setTestingMqtt] = useState(false);
  const [mqttResult, setMqttResult] = useState<Result>(null);

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
    ...networks.map((n) => ({ value: n.ssid, label: `${n.ssid}  ${n.rssi} dBm${n.encryption === 'secured' ? ' 🔒' : ''}` })),
    { value: 'OTHER', label: t('common.other') },
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
      setMqttResult(
        result.success
          ? { type: 'success', text: result.message || t('settings.network.connected') }
          : { type: 'error', text: deviceError(result, t('settings.network.connectionFailed')) },
      );
    } catch {
      setMqttResult({ type: 'error', text: t('settings.network.couldNotReachTheDevice') });
    } finally {
      setTestingMqtt(false);
    }
  };

  const base = config.mqtt.topic || 'sqmeter';
  const publish = { ...defaultMqttPublish, ...config.mqtt.publish };
  const homeAssistant = { ...defaultHomeAssistant, ...config.mqtt.homeAssistant };

  const mqttBadge = !config.mqtt.enabled ? undefined : hw.mqtt.connected === null ? undefined : (
    <StatusBadge
      tone={hw.mqtt.connected ? 'ok' : 'bad'}
      label={hw.mqtt.connected ? t('settings.network.connected') : t('settings.network.notConnected')}
    />
  );

  return (
    <>
      <SettingsCard id="wifi" title="WiFi">
        <div class="form-grid">
          <Field label={t('settings.network.network')} error={error('wifi.ssid')}>
            <div class="input-row">
              <SelectInput
                dataField="wifi.ssid"
                value={ssid === '' ? 'OTHER' : ssid}
                options={networkOptions}
                onChange={(v) => selectNetwork(v === 'OTHER' ? '' : v)}
              />
              <ActionButton onClick={scan} busy={scanning} busyLabel={t('common.scanning')}>
                {t('settings.network.scan')}
              </ActionButton>
            </div>
            {ssid === '' && (
              <TextInput
                dataField="wifi.ssid"
                value={ssid}
                placeholder={t('settings.network.networkName')}
                onInput={(v) => selectNetwork(v)}
              />
            )}
            <ResultNote result={scanError ? { type: 'error', text: scanError } : null} />
          </Field>
          {showPassword && (
            <Field label={t('settings.network.password')}>
              <TextInput
                dataField="wifi.password"
                type="password"
                value={config.wifi.password}
                onInput={(v) => update(['wifi', 'password'], v)}
              />
            </Field>
          )}
          <Field label={t('settings.network.hostname')} error={error('wifi.hostname')}>
            <TextInput dataField="wifi.hostname" value={config.wifi.hostname} onInput={(v) => update(['wifi', 'hostname'], v)} />
          </Field>
        </div>
        <Toggle
          label={t('settings.network.reconnectAutomatically')}
          checked={config.wifi.autoReconnect}
          onChange={(v) => update(['wifi', 'autoReconnect'], v)}
        />
        <DepToggle
          entry={deps.get('wifi.mdns')}
          onFix={fix}
          label={t('settings.network.advertiseOnTheNetworkMdns')}
          hint={t('settings.network.reachableAtHttpValueLocal', { value: config.wifi.hostname || 'sqmeter' })}
          checked={config.wifi.mdns ?? true}
          onChange={(v) => update(['wifi', 'mdns'], v)}
        />
        <DepToggle
          entry={deps.get('wifi.ipv6')}
          onFix={fix}
          label="IPv6"
          hint={t('settings.network.alsoReachableOverIpv6')}
          checked={config.wifi.ipv6 ?? true}
          onChange={(v) => update(['wifi', 'ipv6'], v)}
        />
      </SettingsCard>

      <SettingsCard id="mqtt" title="MQTT" hint={t('settings.network.publishesReadingsToABroker')} badge={mqttBadge}>
        <Toggle
          label={t('settings.network.publishToABroker')}
          checked={config.mqtt.enabled}
          onChange={(v) => update(['mqtt', 'enabled'], v)}
        />
        {config.mqtt.enabled && (
          <>
            <div class="form-grid">
              <Field label={t('settings.network.broker')} error={error('mqttBroker')}>
                <TextInput
                  dataField="mqttBroker"
                  value={config.mqtt.broker}
                  placeholder="192.168.1.100 or fd00::10"
                  onInput={(v) => update(['mqtt', 'broker'], v)}
                />
              </Field>
              <Field label={t('settings.network.port')} error={error('mqttPort')}>
                <NumberInput
                  dataField="mqttPort"
                  integer
                  min={1}
                  max={65535}
                  value={config.mqtt.port}
                  onChange={(v) => update(['mqtt', 'port'], Math.max(1, Math.min(65535, v || 1883)))}
                />
              </Field>
              <Field label={t('settings.network.username')}>
                <TextInput
                  value={config.mqtt.username}
                  placeholder={t('settings.network.optional')}
                  onInput={(v) => update(['mqtt', 'username'], v)}
                />
              </Field>
              <Field label={t('settings.network.password')}>
                <TextInput
                  type="password"
                  value={config.mqtt.password}
                  placeholder={t('settings.network.optional')}
                  onInput={(v) => update(['mqtt', 'password'], v)}
                />
              </Field>
              <Field
                label={t('settings.network.baseTopic')}
                error={error('mqttTopic')}
                hint={t('settings.network.readingsGoToBaseState', { base, base2: base })}
              >
                <TextInput
                  dataField="mqttTopic"
                  value={config.mqtt.topic}
                  placeholder="sqmeter"
                  onInput={(v) => update(['mqtt', 'topic'], v)}
                />
              </Field>
              <Field label={t('settings.network.publishEvery')} error={error('mqttInterval')}>
                <NumberInput
                  dataField="mqttInterval"
                  integer
                  min={1}
                  max={86400}
                  unit="s"
                  value={config.mqtt.publishIntervalMs / 1000}
                  onChange={(v) => update(['mqtt', 'publishIntervalMs'], Math.max(1, v || 60) * 1000)}
                />
              </Field>
            </div>
            <div class="btn-row">
              <ActionButton
                onClick={testMqtt}
                busy={testingMqtt}
                busyLabel={t('common.testing')}
                disabled={!config.mqtt.broker || !config.mqtt.port}
              >
                {t('settings.network.testConnection')}
              </ActionButton>
              <ResultNote result={mqttResult} />
            </div>

            <Group title={t('settings.network.publish')}>
              {PUBLISH_GROUPS.map((item) => (
                <DepToggle
                  key={item.key}
                  entry={deps.get(`mqtt.publish.${item.key}`)}
                  onFix={fix}
                  prefix={t('settings.network.nothingToPublish')}
                  label={item.label}
                  checked={publish[item.key]}
                  onChange={(v) => update(['mqtt', 'publish', item.key], v)}
                  hint={item.hint}
                />
              ))}
            </Group>

            <Group title="Home Assistant">
              <DepToggle
                entry={deps.get('mqtt.homeAssistant.enabled')}
                onFix={fix}
                label={t('settings.network.mqttDiscovery')}
                checked={homeAssistant.enabled}
                onChange={(v) => update(['mqtt', 'homeAssistant', 'enabled'], v)}
                hint={t('settings.network.announcesTheReadingsTheSafe')}
              />
              {homeAssistant.enabled && deps.get('mqtt.homeAssistant.alertsSwitch').state === 'inactive' && (
                <div class="indent">
                  <DepNote entry={deps.get('mqtt.homeAssistant.alertsSwitch')} onFix={fix} prefix={t('settings.network.noAlertsSwitch')} />
                </div>
              )}
              {homeAssistant.enabled && (
                <div class="form-grid indent">
                  <Field
                    label={t('settings.network.discoveryPrefix')}
                    error={error('mqtt.homeAssistant.discoveryPrefix')}
                    hint={t('settings.network.homeAssistantSDefaultIs')}
                  >
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
