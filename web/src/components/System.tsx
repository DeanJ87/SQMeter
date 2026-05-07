import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import { getTimezoneFriendlyName } from '../utils/timezone';
import { useWebSocket } from '../hooks/useWebSocket';
import type { SystemStatus } from '../types';
import { Card, Pill, ProgressMeter, ReadingRow } from './ui';

const formatUptime = (seconds: number): string => {
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  return days > 0 ? `${days}d ${hours}h ${minutes}m` : `${hours}h ${minutes}m`;
};

const formatBytes = (bytes: number): string => {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1048576) return `${(bytes / 1024).toFixed(2)} KB`;
  return `${(bytes / 1048576).toFixed(2)} MB`;
};

const formatAgeMs = (value: number | null | undefined): string => {
  if (typeof value !== 'number' || !Number.isFinite(value)) return '--';
  if (value < 1000) return `${value} ms`;
  if (value < 60000) return `${(value / 1000).toFixed(1)} s`;
  return `${Math.floor(value / 60000)}m ${Math.floor((value % 60000) / 1000)}s`;
};

const formatShortAgeMs = (value: number | null | undefined): string => {
  if (typeof value !== 'number' || !Number.isFinite(value) || value <= 0 || value > 86400000) return '--';
  return formatAgeMs(value);
};

const sensorBadge = (status: number): { text: string; tone: string } => {
  switch (status) {
    case 0: return { text: 'OK', tone: 'pill-green' };
    case 1: return { text: 'Not initialized', tone: 'pill-amber' };
    case 2: return { text: 'Error', tone: 'pill-red' };
    case 3: return { text: 'Timeout', tone: 'pill-amber' };
    case 4: return { text: 'Invalid data', tone: 'pill-red' };
    default: return { text: 'Unknown', tone: 'pill-dim' };
  }
};

const InfoRow: FunctionalComponent<{ label: string; value: string; tone?: string }> = ({ label, value, tone = '' }) => (
  <ReadingRow label={label} value={value} valueClass={tone} />
);

const SensorRow: FunctionalComponent<{ name: string; status: number }> = ({ name, status }) => {
  const badge = sensorBadge(status);
  return (
    <div class="reading-row">
      <span class="reading-label">{name}</span>
      <Pill tone={badge.tone}>{badge.text}</Pill>
    </div>
  );
};

const System: FunctionalComponent = () => {
  const { data: status, connected } = useWebSocket<SystemStatus>('/ws/status');
  const [rg15Action, setRg15Action] = useState<{ loading: boolean; message: string | null }>({ loading: false, message: null });

  if (!connected || !status) {
    return (
      <div class="empty-state">
        <h2>Loading...</h2>
        <p>Connecting to device...</p>
      </div>
    );
  }

  const handleRestart = async () => {
    if (!confirm('Are you sure you want to restart the device?')) return;

    try {
      await fetch('/api/restart', { method: 'POST' });
      alert('Device is restarting...');
    } catch {
      alert('Failed to restart device');
    }
  };

  const runRg15Action = async (path: string, successMessage: string) => {
    setRg15Action({ loading: true, message: null });
    try {
      const response = await fetch(path, { method: 'POST' });
      const data = await response.json().catch(() => ({}));
      setRg15Action({
        loading: false,
        message: response.ok ? successMessage : (data.message || 'RG-15 action failed'),
      });
    } catch {
      setRg15Action({ loading: false, message: 'RG-15 action failed' });
    }
  };

  const heapUsedPercent = status.heapSize > 0 ? ((status.heapSize - status.freeHeap) / status.heapSize) * 100 : 0;
  const flashUsedPercent = status.flashSize > 0 ? (status.sketchSize / status.flashSize) * 100 : 0;
  const fsUsedPercent = status.fsTotal > 0 ? (status.fsUsed / status.fsTotal) * 100 : 0;
  const rg15 = status.sensors.rg15;

  return (
    <div class="panel-page system-page page-enter">
      {status.firmware && (
        <Card title="Firmware" icon="cpu" tone="cyan">
          <div class="system-row-grid">
            <InfoRow label="Name" value={status.firmware.name} />
            <InfoRow label="Version" value={`v${status.firmware.version}`} tone="tone-cyan" />
            <InfoRow label="Build" value={`${status.firmware.buildDate} ${status.firmware.buildTime}`} />
          </div>
        </Card>
      )}

      <Card title="Runtime Status" icon="cpu" tone="violet">
        <div class="system-row-grid">
          <InfoRow label="Uptime" value={formatUptime(status.uptime)} />
          <InfoRow label="CPU Frequency" value={`${status.cpuFreqMHz} MHz`} />
          <div class="system-metric">
            <InfoRow label="Free Heap" value={`${formatBytes(status.freeHeap)} / ${formatBytes(status.heapSize)}`} />
            <ProgressMeter value={100 - heapUsedPercent} />
          </div>
          <div class="system-metric">
            <InfoRow label="Flash" value={`${formatBytes(status.sketchSize)} / ${formatBytes(status.flashSize)}`} />
            <ProgressMeter value={flashUsedPercent} />
          </div>
          <div class="system-metric">
            <InfoRow label="Filesystem" value={`${formatBytes(status.fsUsed)} / ${formatBytes(status.fsTotal)}`} />
            <ProgressMeter value={fsUsedPercent} />
          </div>
          <div>
            <div class="reading-row">
              <span class="reading-label">Current Time</span>
              {status.ntp && status.ntp.activeSource > 0 && (
                <Pill tone="pill-green">{status.ntp.activeSource === 1 ? 'NTP' : 'GPS'}</Pill>
              )}
            </div>
            <strong class="system-time">{status.time.iso}</strong>
            <p class="system-subtle">{getTimezoneFriendlyName(status.time.timezone)}</p>
          </div>
        </div>
      </Card>

      <Card title="Sensors" icon="eye" tone="green">
        <div class="system-list">
          <SensorRow name="TSL2591 Light Sensor" status={status.sensors.tsl2591.status} />
          <SensorRow name="BME280 Environment" status={status.sensors.bme280.status} />
          <SensorRow name="MLX90614 IR Temperature" status={status.sensors.mlx90614.status} />
          <SensorRow name="GPS Module" status={status.sensors.gps.status} />
          {rg15 && <SensorRow name="RG-15 Rain Sensor" status={rg15.status} />}
        </div>
      </Card>

      {rg15 && (
        <Card title="RG-15 Diagnostics" icon="rain" tone="cyan">
          <div class="system-actions">
            <button
              type="button"
              disabled={rg15Action.loading}
              onClick={() => runRg15Action('/api/sensors/rg15/reset-total', 'RG-15 total reset command sent')}
              class="bg-blue-600"
            >
              Reset total
            </button>
            <button
              type="button"
              disabled={rg15Action.loading}
              onClick={() => runRg15Action('/api/sensors/rg15/reboot', 'RG-15 reboot command sent')}
              class="bg-amber-600"
            >
              Reboot RG-15
            </button>
          </div>
          {rg15Action.message && <p class="system-message">{rg15Action.message}</p>}

          <div class="system-diagnostic-grid">
            <InfoRow label="Online" value={rg15.online ? 'Yes' : 'No'} tone={rg15.online ? 'tone-green' : 'tone-red'} />
            <InfoRow label="Raining" value={(rg15.raining ?? rg15.isRaining) ? 'Yes' : 'No'} />
            <InfoRow label="Rain intensity" value={String(rg15.rain_intensity ?? rg15.rInt ?? '--')} />
            <InfoRow label="Since last read" value={String(rg15.accumulation_since_last_read ?? rg15.acc ?? '--')} />
            <InfoRow label="Event total" value={String(rg15.event_accumulation ?? rg15.eventAcc ?? '--')} />
            <InfoRow label="RX / TX" value={`${rg15.uart?.rx_pin ?? '--'} / ${rg15.uart?.tx_pin ?? '--'}`} />
            <InfoRow label="Baud rate" value={String(rg15.uart?.baud_rate ?? '--')} />
            <InfoRow label="Successful reads" value={String(rg15.uart?.successful_reads ?? 0)} />
            <InfoRow label="Timeouts" value={String(rg15.uart?.timeouts ?? 0)} />
            <InfoRow label="Parse errors" value={String(rg15.uart?.parse_errors ?? 0)} />
            <InfoRow label="Last response age" value={formatAgeMs(rg15.uart?.last_response_age_ms)} />
            <InfoRow label="Health probe age" value={formatAgeMs(rg15.uart?.last_health_check_age_ms)} />
          </div>

          <div class="system-log-row">
            <span>Last command / response / error</span>
            <strong>{rg15.uart?.last_command ?? '--'}</strong>
            <em>{rg15.uart?.last_raw_response ?? '--'}</em>
            <em>{rg15.uart?.last_error ?? '--'}</em>
          </div>
        </Card>
      )}

      {status.partitions && (
        <Card title="Flash Partitions" icon="upload" tone="cyan">
          <div class="system-row-grid">
            <InfoRow label="Current Slot" value={status.partitions.runningSlot} />
            <InfoRow label="Next Update Slot" value={status.partitions.nextSlot} />
            <InfoRow label="OTA Slot Size" value={formatBytes(status.partitions.runningSize)} />
            {status.partitions.nvs && (
              <>
                <InfoRow label="NVS Used" value={String(status.partitions.nvs.usedEntries)} />
                <InfoRow label="NVS Free" value={String(status.partitions.nvs.freeEntries)} />
                <InfoRow label="Namespaces" value={String(status.partitions.nvs.namespaceCount)} />
              </>
            )}
          </div>
        </Card>
      )}

      {status.ntp && (
        <Card title="Network Time (NTP)" icon="gps" tone="green">
          {status.ntp.enabled ? (
            <div class="system-list">
              <InfoRow label="Status" value={status.ntp.synced ? 'Synced' : status.ntp.status === 1 ? 'Syncing' : 'Not synced'} tone={status.ntp.synced ? 'tone-green' : 'tone-amber'} />
              <InfoRow label="Server" value={status.ntp.server} />
              <InfoRow label="Last Sync Age" value={formatShortAgeMs(status.ntp.lastSync)} />
              <InfoRow label="Next Sync" value={formatShortAgeMs(status.ntp.nextSync)} />
              <InfoRow label="Clock Drift" value={`${status.ntp.drift}s`} />
            </div>
          ) : (
            <p class="system-subtle">NTP is disabled.</p>
          )}
        </Card>
      )}

      {status.ntp && status.ntp.gpsEnabled && (
        <Card title="GPS Time" icon="gps" tone="green">
          <div class="system-list">
            <InfoRow label="Status" value={status.ntp.gpsHasFix ? 'Lock acquired' : 'Searching'} tone={status.ntp.gpsHasFix ? 'tone-green' : 'tone-amber'} />
            {status.ntp.gpsHasFix && status.ntp.gpsTimeUTC && (
              <InfoRow label="GPS Time (UTC)" value={status.ntp.gpsTimeUTC} tone="tone-cyan" />
            )}
            <InfoRow label="Satellites" value={String(status.ntp.gpsSatellites || 0)} />
            {status.gpsData && (
              <InfoRow label="HDOP" value={status.gpsData.hdop.toFixed(1)} />
            )}
          </div>
        </Card>
      )}

      {status.mqtt && (
        <Card title="MQTT" icon="wifi" tone="cyan">
          {status.mqtt.enabled ? (
            <div class="system-list">
              <InfoRow label="Status" value={status.mqtt.connected ? 'Connected' : 'Disconnected'} tone={status.mqtt.connected ? 'tone-green' : 'tone-red'} />
              <InfoRow label="Broker" value={`${status.mqtt.broker}:${status.mqtt.port}`} />
              <InfoRow label="Topic" value={status.mqtt.topic} />
            </div>
          ) : (
            <p class="system-subtle">MQTT is disabled.</p>
          )}
        </Card>
      )}

      {status.gpsData && status.sensors.gps.initialized && (
        <Card title="GPS Location" icon="gps" tone="green">
          <div class="system-list">
            <InfoRow label="Fix" value={status.gpsData.hasFix ? 'Lock acquired' : 'Searching'} tone={status.gpsData.hasFix ? 'tone-green' : 'tone-amber'} />
            <InfoRow label="Satellites" value={String(status.gpsData.satellites)} />
            {status.gpsData.hasFix && (
              <>
                <InfoRow label="Latitude" value={`${status.gpsData.latitude.toFixed(6)} deg`} />
                <InfoRow label="Longitude" value={`${status.gpsData.longitude.toFixed(6)} deg`} />
                <InfoRow label="Altitude" value={`${status.gpsData.altitude.toFixed(1)} m`} />
                <InfoRow label="Fix Age" value={formatAgeMs(status.gpsData.age)} />
              </>
            )}
          </div>
        </Card>
      )}

      <Card title="WiFi" icon="wifi" tone="cyan">
        <div class="system-list">
          <InfoRow label="Status" value={status.wifi.connected ? 'Connected' : 'Disconnected'} tone={status.wifi.connected ? 'tone-green' : 'tone-red'} />
          {status.wifi.connected && (
            <>
              <InfoRow label="SSID" value={status.wifi.ssid} />
              <InfoRow label="IP Address" value={status.wifi.ip} tone="tone-cyan" />
              <InfoRow label="Signal" value={`${status.wifi.rssi} dBm`} />
              <InfoRow label="MAC Address" value={status.wifi.mac} />
            </>
          )}
        </div>
      </Card>

      <Card title="Actions" icon="cpu" tone="red">
        <button onClick={handleRestart} class="bg-red-600 system-wide-button">
          Restart Device
        </button>
      </Card>
    </div>
  );
};

export default System;
