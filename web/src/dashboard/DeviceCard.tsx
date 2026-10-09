import { FunctionalComponent } from 'preact';
import type { SystemStatus } from '../types';
import { Card, InfoTip, Pill } from '../components/ui';
import { compareVersions } from '../utils/versionCompare';
import { lastUpdateCheck } from '../lib/lastUpdateCheck';
import { formatUptime } from '../i18n/format';
import { t, type MessageKey } from '../i18n';

// Device & Network (specs/010, 025 FR-015, FR-018, 026 FR-005): Wi-Fi and
// uptime as tiles; every address, the .local name, MQTT and the firmware as
// one row each, with "Update available" from the Updates page's last check.

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

export const mqttStateText = (mqtt: NonNullable<SystemStatus['mqtt']>): { text: string; tone: string; reason?: string } => {
  if (mqtt.connected) return { text: t('dashboard.connected'), tone: 'pill-green' };
  const reason = MQTT_STATE[mqtt.state];
  return reason
    ? { text: t('glance.mqttCantConnect'), tone: 'pill-red', reason: t(reason) }
    : { text: t('glance.mqttRetrying'), tone: 'pill-amber' };
};

// One labelled value per row (specs/026 DS-05): long values truncate, the
// full value is in the tooltip.
const Row: FunctionalComponent<{ label: string; hint?: string; inventory?: string; value?: string; mono?: boolean }> = ({
  label,
  hint,
  inventory,
  value,
  mono,
  children,
}) => (
  <div class="reading-row" data-inventory={inventory}>
    <span class="reading-label">
      {label} {hint && <InfoTip text={hint} />}
    </span>
    {value !== undefined ? (
      <strong class={`reading-value${mono ? ' mono ltr address-value' : ''}`} title={value}>
        {value}
      </strong>
    ) : (
      children
    )}
  </div>
);

const SCOPE_ORDER = { global: 0, 'unique-local': 1, 'link-local': 2 } as const;
const SCOPE: Record<keyof typeof SCOPE_ORDER, { label: () => string; hint?: () => string }> = {
  global: { label: () => t('glance.ipv6') },
  'unique-local': { label: () => t('glance.ipv6Local'), hint: () => t('glance.ipv6LocalHint') },
  'link-local': { label: () => t('glance.ipv6Link'), hint: () => t('glance.ipv6LinkHint') },
};

const Ipv6Rows: FunctionalComponent<{ ipv6: NonNullable<SystemStatus['wifi']['ipv6']> }> = ({ ipv6 }) => {
  const addresses = [...ipv6.addresses].sort((a, b) => SCOPE_ORDER[a.scope] - SCOPE_ORDER[b.scope]);
  if (!addresses.length)
    return (
      <Row label={t('glance.ipv6')} inventory="ipv6">
        <span class="reading-value system-subtle">{t('glance.ipv6None')}</span>
      </Row>
    );
  return (
    <div data-inventory="ipv6">
      {addresses.map((entry) => (
        <Row key={entry.address} label={SCOPE[entry.scope].label()} hint={SCOPE[entry.scope].hint?.()} value={entry.address} mono />
      ))}
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
        <div class="metric-label">{t('dashboard.uptime')}</div>
        <div class="metric-value small">{formatUptime(uptime)}</div>
      </div>
    </div>
  );
};

const LocalName: FunctionalComponent<{ wifi: Wifi }> = ({ wifi }) =>
  wifi?.mdns && wifi.hostname ? <Row label={t('glance.localName')} inventory="mdns" value={`${wifi.hostname}.local`} mono /> : null;

const MqttRow: FunctionalComponent<{ mqtt: SystemStatus['mqtt'] }> = ({ mqtt }) => {
  if (!mqtt?.enabled) return null;
  const state = mqttStateText(mqtt);
  return (
    <Row label={t('glance.mqtt')} hint={state.reason} inventory="mqtt-state">
      <Pill tone={state.tone}>{state.text}</Pill>
    </Row>
  );
};

const FirmwareRow: FunctionalComponent<{ version?: string }> = ({ version }) => {
  if (!version) return null;
  const latest = lastUpdateCheck();
  const newer = latest ? (compareVersions(latest, version) ?? 0) > 0 : false;
  return (
    <Row label={t('dashboard.firmware')}>
      <span class="device-firmware">
        {newer && (
          <a class="status-tile-fix" href="#/updates" data-inventory="update-available">
            {t('glance.updateAvailable', { version: latest ?? '' })}
          </a>
        )}
        <Pill>v{version}</Pill>
      </span>
    </Row>
  );
};

const DeviceCard: FunctionalComponent<{ status: SystemStatus | null }> = ({ status }) => (
  <Card title={t('dashboard.deviceNetwork')} icon="wifi" tone="cyan">
    <WifiTiles wifi={status?.wifi} uptime={status?.uptime} />
    <div class="device-rows">
      <Row label={t('glance.ipv4')} value={status?.wifi?.ip ?? '--'} mono />
      {status?.wifi?.ipv6?.enabled && <Ipv6Rows ipv6={status.wifi.ipv6} />}
      <LocalName wifi={status?.wifi} />
      <MqttRow mqtt={status?.mqtt} />
      <FirmwareRow version={status?.firmware?.version} />
    </div>
  </Card>
);

export default DeviceCard;
