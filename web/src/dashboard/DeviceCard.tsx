import { FunctionalComponent } from 'preact';
import type { SystemStatus } from '../types';
import { Card, Pill } from '../components/ui';
import { compareVersions } from '../utils/versionCompare';
import { lastUpdateCheck } from '../lib/lastUpdateCheck';
import { formatUptime } from '../i18n/format';
import { t, type MessageKey } from '../i18n';

// Device & Network (specs/010, 025 FR-015, FR-018): WiFi, addresses (IPv6
// too), the .local name, MQTT and the firmware, with "Update available"
// from the Updates page's last check - never a check of its own.

const rssiTone = (rssi?: number) => {
  if (typeof rssi !== 'number') return { tone: 'pill-dim', label: t('dashboard.unknown') };
  if (rssi > -60) return { tone: 'pill-green', label: t('dashboard.strong') };
  if (rssi > -75) return { tone: 'pill-cyan', label: t('dashboard.good') };
  if (rssi > -85) return { tone: 'pill-amber', label: t('dashboard.weak') };
  return { tone: 'pill-red', label: t('dashboard.poor') };
};

// PubSubClient's state codes, in the device's own words (device.api.*).
const MQTT_STATE: Record<number, MessageKey> = {
  [-4]: 'device.api.connectionTimeout',
  [-3]: 'device.api.connectionLost',
  [-2]: 'device.api.connectFailed',
  1: 'device.api.badProtocol',
  2: 'device.api.badClientId',
  3: 'device.api.unavailable',
  4: 'device.api.badCredentialsCheckUsernamePassword',
  5: 'device.api.unauthorized',
};

export const mqttStateText = (mqtt: NonNullable<SystemStatus['mqtt']>) => {
  if (mqtt.connected) return { text: t('dashboard.connected'), tone: 'pill-green' };
  const reason = MQTT_STATE[mqtt.state];
  return reason
    ? { text: t('glance.mqttCantConnect', { reason: t(reason) }), tone: 'pill-red' }
    : { text: t('glance.mqttRetrying'), tone: 'pill-amber' };
};

const SCOPE_ORDER = { global: 0, 'unique-local': 1, 'link-local': 2 } as const;

const Ipv6Rows: FunctionalComponent<{ ipv6: NonNullable<SystemStatus['wifi']['ipv6']> }> = ({ ipv6 }) => {
  const addresses = [...ipv6.addresses].sort((a, b) => SCOPE_ORDER[a.scope] - SCOPE_ORDER[b.scope]);
  return (
    <div class="device-row" data-inventory="ipv6">
      <span>{t('glance.ipv6')}</span>
      {addresses.length ? (
        <span class="device-addresses ltr mono">
          {addresses.map((entry) => (
            <span key={entry.address}>{entry.address}</span>
          ))}
        </span>
      ) : (
        <span class="system-subtle">{t('glance.ipv6None')}</span>
      )}
    </div>
  );
};

type Wifi = SystemStatus['wifi'] | undefined;

const WifiTiles: FunctionalComponent<{ wifi: Wifi; uptime?: number }> = ({ wifi, uptime }) => {
  const rssi = rssiTone(wifi?.rssi);
  return (
    <div class="tile-grid two">
      <div class="metric-tile left">
        <div class="metric-label">{t('dashboard.wifi')}</div>
        <Pill tone={rssi.tone}>{rssi.label}</Pill>
        <div class="metric-sub mono">{wifi?.rssi ?? '--'} dBm</div>
        <div class="metric-sub">{wifi?.ssid ?? '--'}</div>
      </div>
      <div class="metric-tile left">
        <div class="metric-label">{t('dashboard.ipAddress')}</div>
        <div class="metric-value tone-cyan small">{wifi?.ip ?? '--'}</div>
        <div class="metric-label pushed">{t('dashboard.uptime')}</div>
        <div class="metric-sub mono">{formatUptime(uptime)}</div>
      </div>
    </div>
  );
};

const LocalName: FunctionalComponent<{ wifi: Wifi }> = ({ wifi }) =>
  wifi?.mdns && wifi.hostname ? (
    <div class="device-row" data-inventory="mdns">
      <span>{t('glance.localName')}</span>
      <span class="mono ltr">{`${wifi.hostname}.local`}</span>
    </div>
  ) : null;

const MqttRow: FunctionalComponent<{ mqtt: SystemStatus['mqtt'] }> = ({ mqtt }) => {
  if (!mqtt?.enabled) return null;
  const state = mqttStateText(mqtt);
  return (
    <div class="device-row" data-inventory="mqtt-state">
      <span>{t('glance.mqtt')}</span>
      <Pill tone={state.tone}>{state.text}</Pill>
    </div>
  );
};

const FirmwareRow: FunctionalComponent<{ version?: string }> = ({ version }) => {
  if (!version) return null;
  const latest = lastUpdateCheck();
  const newer = latest ? (compareVersions(latest, version) ?? 0) > 0 : false;
  return (
    <div class="firmware-row">
      <span>{t('dashboard.firmware')}</span>
      <Pill>v{version}</Pill>
      {newer && (
        <a class="glance-fix" href="#/updates" data-inventory="update-available">
          {t('glance.updateAvailable', { version: latest ?? '' })}
        </a>
      )}
    </div>
  );
};

const DeviceCard: FunctionalComponent<{ status: SystemStatus | null }> = ({ status }) => (
  <Card title={t('dashboard.deviceNetwork')} icon="wifi" tone="cyan">
    <WifiTiles wifi={status?.wifi} uptime={status?.uptime} />
    {status?.wifi?.ipv6?.enabled && <Ipv6Rows ipv6={status.wifi.ipv6} />}
    <LocalName wifi={status?.wifi} />
    <MqttRow mqtt={status?.mqtt} />
    <FirmwareRow version={status?.firmware?.version} />
  </Card>
);

export default DeviceCard;
