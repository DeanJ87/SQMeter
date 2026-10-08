import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import { getTimezoneFriendlyName } from '../utils/timezone';
import { useWebSocket } from '../hooks/useWebSocket';
import type { SensorHealth, SystemStatus } from '../types';
import { Button, Card, Note, Pill, ProgressMeter, ReadingRow } from './ui';
import { showToast } from './toast';

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

const sensorBadge = (status: SensorHealth): { text: string; tone: string } => {
  switch (status) {
    case 'ok':
      return { text: 'OK', tone: 'pill-green' };
    case 'missing':
      return { text: 'Not detected', tone: 'pill-red' };
    case 'error':
      return { text: 'Error', tone: 'pill-red' };
    case 'stale':
      return { text: 'Stale', tone: 'pill-amber' };
    default:
      return { text: 'Unknown', tone: 'pill-dim' };
  }
};

const InfoRow: FunctionalComponent<{ label: string; value: string; tone?: string }> = ({ label, value, tone = '' }) => (
  <ReadingRow label={label} value={value} valueClass={tone} />
);

const SensorRow: FunctionalComponent<{ name: string; status: SensorHealth }> = ({ name, status }) => {
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
  const [confirmRestart, setConfirmRestart] = useState(false);

  if (!connected || !status) {
    return (
      <div class="empty-state">
        <h2>Loading...</h2>
        <p>Connecting to device...</p>
      </div>
    );
  }

  const handleRestart = async () => {
    setConfirmRestart(false);
    try {
      await fetch('/api/restart', { method: 'POST' });
      showToast({ message: 'Restarting...' });
    } catch {
      showToast({ message: 'Could not reach the device', tone: 'bad' });
    }
  };

  const runRg15Action = async (path: string, successMessage: string) => {
    setRg15Action({ loading: true, message: null });
    try {
      const response = await fetch(path, { method: 'POST' });
      const data = await response.json().catch(() => ({}));
      setRg15Action({
        loading: false,
        message: response.ok ? successMessage : data.error || 'Failed',
      });
    } catch {
      setRg15Action({ loading: false, message: 'Could not reach the device' });
    }
  };

  const heapUsedPercent = status.heapSize > 0 ? ((status.heapSize - status.freeHeap) / status.heapSize) * 100 : 0;
  const flashUsedPercent = status.flashSize > 0 ? (status.sketchSize / status.flashSize) * 100 : 0;
  const fsUsedPercent = status.fsTotal > 0 ? (status.fsUsed / status.fsTotal) * 100 : 0;
  const rain = status.sensors.rain;
  const rainDiagnostics = status.diagnostics?.rain;

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
              {status.ntp && status.ntp.activeSource > 0 && <Pill tone="pill-green">{status.ntp.activeSource === 1 ? 'NTP' : 'GPS'}</Pill>}
            </div>
            <strong class="system-time">{status.time.iso}</strong>
            <p class="system-subtle">{getTimezoneFriendlyName(status.time.timezone)}</p>
          </div>
        </div>
      </Card>

      <Card title="Sensors" icon="eye" tone="green">
        <div class="system-list">
          <SensorRow name="TSL2591 Light Sensor" status={status.sensors.light.status} />
          <SensorRow name="BME280 Environment" status={status.sensors.environment.status} />
          <SensorRow name="MLX90614 IR Temperature" status={status.sensors.infrared.status} />
          {status.sensors.gps && <SensorRow name="GPS Module" status={status.sensors.gps.status} />}
          {rain && <SensorRow name="RG-15 Rain Sensor" status={rain.status} />}
          {status.sensors.wind && <SensorRow name="Anemometer" status={status.sensors.wind.status} />}
        </div>
      </Card>

      {rain && (
        <Card title="RG-15 Diagnostics" icon="rain" tone="cyan">
          <div class="btn-row">
            <Button small disabled={rg15Action.loading} onClick={() => runRg15Action('/api/sensors/rg15/reset-total', 'Total reset.')}>
              Reset total
            </Button>
            <Button
              small
              variant="danger"
              disabled={rg15Action.loading}
              onClick={() => runRg15Action('/api/sensors/rg15/reboot', 'RG-15 rebooting.')}
            >
              Reboot RG-15
            </Button>
            {rg15Action.message && <Note>{rg15Action.message}</Note>}
          </div>

          {rainDiagnostics && (
            <>
              <div class="system-diagnostic-grid">
                <InfoRow label="State" value={rainDiagnostics.state} tone={rain.status === 'ok' ? 'tone-green' : 'tone-red'} />
                <InfoRow label="RX / TX" value={`${rainDiagnostics.rxPin} / ${rainDiagnostics.txPin}`} />
                <InfoRow label="Baud rate" value={String(rainDiagnostics.baudRate)} />
                <InfoRow label="Successful reads" value={String(rainDiagnostics.successfulReads)} />
                <InfoRow label="Timeouts" value={String(rainDiagnostics.timeouts)} />
                <InfoRow label="Parse errors" value={String(rainDiagnostics.parseErrors)} />
                <InfoRow label="Last response age" value={formatAgeMs(rainDiagnostics.lastResponseAgeMs)} />
                <InfoRow label="Last poll age" value={formatAgeMs(rainDiagnostics.lastPollAgeMs)} />
              </div>

              <div class="system-log-row">
                <span>Last command / response / error</span>
                <strong>{rainDiagnostics.lastCommand ?? '--'}</strong>
                <em>{rainDiagnostics.lastResponse ?? '--'}</em>
                <em>{rainDiagnostics.lastError ?? '--'}</em>
              </div>
            </>
          )}
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
              <InfoRow
                label="Status"
                value={status.ntp.synced ? 'Synced' : status.ntp.status === 1 ? 'Syncing' : 'Not synced'}
                tone={status.ntp.synced ? 'tone-green' : 'tone-amber'}
              />
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
            <InfoRow
              label="Status"
              value={status.ntp.gpsHasFix ? 'Lock acquired' : 'Searching'}
              tone={status.ntp.gpsHasFix ? 'tone-green' : 'tone-amber'}
            />
            {status.ntp.gpsHasFix && status.ntp.gpsTimeUTC && (
              <InfoRow label="GPS Time (UTC)" value={status.ntp.gpsTimeUTC} tone="tone-cyan" />
            )}
            <InfoRow label="Satellites" value={String(status.ntp.gpsSatellites || 0)} />
          </div>
        </Card>
      )}

      {status.mqtt && (
        <Card title="MQTT" icon="wifi" tone="cyan">
          {status.mqtt.enabled ? (
            <div class="system-list">
              <InfoRow
                label="Status"
                value={status.mqtt.connected ? 'Connected' : 'Disconnected'}
                tone={status.mqtt.connected ? 'tone-green' : 'tone-red'}
              />
              <InfoRow label="Broker" value={`${status.mqtt.broker}:${status.mqtt.port}`} />
              <InfoRow label="Topic" value={status.mqtt.topic} />
            </div>
          ) : (
            <p class="system-subtle">MQTT is disabled.</p>
          )}
        </Card>
      )}

      <Card title="WiFi" icon="wifi" tone="cyan">
        <div class="system-list">
          <InfoRow
            label="Status"
            value={status.wifi.connected ? 'Connected' : 'Disconnected'}
            tone={status.wifi.connected ? 'tone-green' : 'tone-red'}
          />
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
        <div class="btn-row">
          {confirmRestart ? (
            <>
              <Button variant="danger" onClick={handleRestart}>
                Restart now
              </Button>
              <Button variant="ghost" onClick={() => setConfirmRestart(false)}>
                Cancel
              </Button>
            </>
          ) : (
            <Button variant="danger" onClick={() => setConfirmRestart(true)}>
              Restart device
            </Button>
          )}
        </div>
      </Card>
    </div>
  );
};

export default System;
