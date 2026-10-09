import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import { getTimezoneFriendlyName } from '../utils/timezone';
import { useWebSocket } from '../hooks/useWebSocket';
import type { SensorHealth, SystemStatus } from '../types';
import { Button, Card, Note, Pill, ProgressMeter, ReadingRow } from './ui';
import { showToast } from './toast';
import { t } from '../i18n';
import { formatAgeMs, formatUptime } from '../i18n/format';
import { deviceError } from '../i18n/deviceMessage';

const formatBytes = (bytes: number): string => {
  if (bytes < 1024) return `${bytes} B`;
  if (bytes < 1048576) return `${(bytes / 1024).toFixed(2)} KB`;
  return `${(bytes / 1048576).toFixed(2)} MB`;
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
      return { text: t('system.notDetected'), tone: 'pill-red' };
    case 'error':
      return { text: t('system.error'), tone: 'pill-red' };
    case 'stale':
      return { text: t('system.stale'), tone: 'pill-amber' };
    default:
      return { text: t('system.unknown'), tone: 'pill-dim' };
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
        <h2>{t('common.loading')}</h2>
        <p>{t('system.connectingToDevice')}</p>
      </div>
    );
  }

  const handleRestart = async () => {
    setConfirmRestart(false);
    try {
      await fetch('/api/restart', { method: 'POST' });
      showToast({ message: t('common.restarting') });
    } catch {
      showToast({ message: t('system.couldNotReachTheDevice'), tone: 'bad' });
    }
  };

  const runRg15Action = async (path: string, successMessage: string) => {
    setRg15Action({ loading: true, message: null });
    try {
      const response = await fetch(path, { method: 'POST' });
      const data = await response.json().catch(() => ({}));
      setRg15Action({
        loading: false,
        message: response.ok ? successMessage : deviceError(data, t('system.failed')),
      });
    } catch {
      setRg15Action({ loading: false, message: t('system.couldNotReachTheDevice') });
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
        <Card title={t('system.firmware')} icon="cpu" tone="cyan">
          <div class="system-row-grid">
            <InfoRow label={t('system.name')} value={status.firmware.name} />
            <InfoRow label={t('system.version')} value={`v${status.firmware.version}`} tone="tone-cyan" />
            <InfoRow label={t('system.build')} value={`${status.firmware.buildDate} ${status.firmware.buildTime}`} />
          </div>
        </Card>
      )}

      <Card title={t('system.runtimeStatus')} icon="cpu" tone="violet">
        <div class="system-row-grid">
          <InfoRow label={t('system.uptime')} value={formatUptime(status.uptime)} />
          <InfoRow label={t('system.cpuFrequency')} value={t('system.cpufreqmhzMhz', { cpuFreqMHz: status.cpuFreqMHz })} />
          <div class="system-metric">
            <InfoRow label={t('system.freeHeap')} value={`${formatBytes(status.freeHeap)} / ${formatBytes(status.heapSize)}`} />
            <ProgressMeter value={100 - heapUsedPercent} label={t('system.freeHeap2')} />
          </div>
          <div class="system-metric">
            <InfoRow label={t('system.flash')} value={`${formatBytes(status.sketchSize)} / ${formatBytes(status.flashSize)}`} />
            <ProgressMeter value={flashUsedPercent} label={t('system.flashUsed')} />
          </div>
          <div class="system-metric">
            <InfoRow label={t('system.filesystem')} value={`${formatBytes(status.fsUsed)} / ${formatBytes(status.fsTotal)}`} />
            <ProgressMeter value={fsUsedPercent} label={t('system.filesystemUsed')} />
          </div>
          <div>
            <div class="reading-row">
              <span class="reading-label">{t('system.currentTime')}</span>
              {status.ntp && status.ntp.activeSource > 0 && <Pill tone="pill-green">{status.ntp.activeSource === 1 ? 'NTP' : 'GPS'}</Pill>}
            </div>
            <strong class="system-time">{status.time.iso}</strong>
            <p class="system-subtle">{getTimezoneFriendlyName(status.time.timezone)}</p>
          </div>
        </div>
      </Card>

      <Card title={t('system.sensors')} icon="eye" tone="green">
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
        <Card title={t('system.rg15Diagnostics')} icon="rain" tone="cyan">
          <div class="btn-row">
            <Button
              small
              disabled={rg15Action.loading}
              onClick={() => runRg15Action('/api/sensors/rg15/reset-total', t('system.totalReset'))}
            >
              {t('system.resetTotal')}
            </Button>
            <Button
              small
              variant="danger"
              disabled={rg15Action.loading}
              onClick={() => runRg15Action('/api/sensors/rg15/reboot', t('system.rg15Rebooting'))}
            >
              {t('system.rebootRg15')}
            </Button>
            {rg15Action.message && <Note>{rg15Action.message}</Note>}
          </div>

          {rainDiagnostics && (
            <>
              <div class="system-diagnostic-grid">
                <InfoRow label={t('system.state')} value={rainDiagnostics.state} tone={rain.status === 'ok' ? 'tone-green' : 'tone-red'} />
                <InfoRow label={t('system.rxTx')} value={`${rainDiagnostics.rxPin} / ${rainDiagnostics.txPin}`} />
                <InfoRow label={t('system.baudRate')} value={String(rainDiagnostics.baudRate)} />
                <InfoRow label={t('system.successfulReads')} value={String(rainDiagnostics.successfulReads)} />
                <InfoRow label={t('system.timeouts')} value={String(rainDiagnostics.timeouts)} />
                <InfoRow label={t('system.parseErrors')} value={String(rainDiagnostics.parseErrors)} />
                <InfoRow label={t('system.lastResponseAge')} value={formatAgeMs(rainDiagnostics.lastResponseAgeMs)} />
                <InfoRow label={t('system.lastPollAge')} value={formatAgeMs(rainDiagnostics.lastPollAgeMs)} />
              </div>

              <div class="system-log-row">
                <span>{t('system.lastCommandResponseError')}</span>
                <strong>{rainDiagnostics.lastCommand ?? '--'}</strong>
                <em>{rainDiagnostics.lastResponse ?? '--'}</em>
                <em>{rainDiagnostics.lastError ?? '--'}</em>
              </div>
            </>
          )}
        </Card>
      )}

      {status.partitions && (
        <Card title={t('system.flashPartitions')} icon="upload" tone="cyan">
          <div class="system-row-grid">
            <InfoRow label={t('system.currentSlot')} value={status.partitions.runningSlot} />
            <InfoRow label={t('system.nextUpdateSlot')} value={status.partitions.nextSlot} />
            <InfoRow label={t('system.otaSlotSize')} value={formatBytes(status.partitions.runningSize)} />
            {status.partitions.nvs && (
              <>
                <InfoRow label={t('system.nvsUsed')} value={String(status.partitions.nvs.usedEntries)} />
                <InfoRow label={t('system.nvsFree')} value={String(status.partitions.nvs.freeEntries)} />
                <InfoRow label={t('system.namespaces')} value={String(status.partitions.nvs.namespaceCount)} />
              </>
            )}
          </div>
        </Card>
      )}

      {status.ntp && (
        <Card title={t('system.networkTimeNtp')} icon="gps" tone="green">
          {status.ntp.enabled ? (
            <div class="system-list">
              <InfoRow
                label={t('system.status')}
                value={status.ntp.synced ? t('system.synced') : status.ntp.status === 1 ? t('system.syncing') : t('system.notSynced')}
                tone={status.ntp.synced ? 'tone-green' : 'tone-amber'}
              />
              <InfoRow label={t('system.server')} value={status.ntp.server} />
              <InfoRow label={t('system.lastSyncAge')} value={formatShortAgeMs(status.ntp.lastSync)} />
              <InfoRow label={t('system.nextSync')} value={formatShortAgeMs(status.ntp.nextSync)} />
              <InfoRow label={t('system.clockDrift')} value={`${status.ntp.drift}s`} />
            </div>
          ) : (
            <p class="system-subtle">{t('system.ntpIsDisabled')}</p>
          )}
        </Card>
      )}

      {status.ntp && status.ntp.gpsEnabled && (
        <Card title={t('system.gpsTime')} icon="gps" tone="green">
          <div class="system-list">
            <InfoRow
              label={t('system.status')}
              value={status.ntp.gpsHasFix ? t('system.lockAcquired') : t('system.searching')}
              tone={status.ntp.gpsHasFix ? 'tone-green' : 'tone-amber'}
            />
            {status.ntp.gpsHasFix && status.ntp.gpsTimeUTC && (
              <InfoRow label={t('system.gpsTimeUtc')} value={status.ntp.gpsTimeUTC} tone="tone-cyan" />
            )}
            <InfoRow label={t('system.satellites')} value={String(status.ntp.gpsSatellites || 0)} />
          </div>
        </Card>
      )}

      {status.mqtt && (
        <Card title="MQTT" icon="wifi" tone="cyan">
          {status.mqtt.enabled ? (
            <div class="system-list">
              <InfoRow
                label={t('system.status')}
                value={status.mqtt.connected ? t('system.connected') : t('system.disconnected')}
                tone={status.mqtt.connected ? 'tone-green' : 'tone-red'}
              />
              <InfoRow label={t('system.broker')} value={`${status.mqtt.broker}:${status.mqtt.port}`} />
              <InfoRow label={t('system.topic')} value={status.mqtt.topic} />
            </div>
          ) : (
            <p class="system-subtle">{t('system.mqttIsDisabled')}</p>
          )}
        </Card>
      )}

      <Card title="WiFi" icon="wifi" tone="cyan">
        <div class="system-list">
          <InfoRow
            label={t('system.status')}
            value={status.wifi.connected ? t('system.connected') : t('system.disconnected')}
            tone={status.wifi.connected ? 'tone-green' : 'tone-red'}
          />
          {status.wifi.connected && (
            <>
              <InfoRow label="SSID" value={status.wifi.ssid} />
              <InfoRow label={t('system.ipAddress')} value={status.wifi.ip} tone="tone-cyan" />
              <InfoRow label={t('system.signal')} value={`${status.wifi.rssi} dBm`} />
              <InfoRow label={t('system.macAddress')} value={status.wifi.mac} />
            </>
          )}
        </div>
      </Card>

      <Card title={t('system.actions')} icon="cpu" tone="red">
        <div class="btn-row">
          {confirmRestart ? (
            <>
              <Button variant="danger" onClick={handleRestart}>
                {t('system.restartNow')}
              </Button>
              <Button variant="ghost" onClick={() => setConfirmRestart(false)}>
                {t('system.cancel')}
              </Button>
            </>
          ) : (
            <Button variant="danger" onClick={() => setConfirmRestart(true)}>
              {t('system.restartDevice')}
            </Button>
          )}
        </div>
      </Card>
    </div>
  );
};

export default System;
