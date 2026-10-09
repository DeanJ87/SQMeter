import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import { getTimezoneFriendlyName } from '../utils/timezone';
import { useWebSocket } from '../hooks/useWebSocket';
import type { SensorHealth, SystemStatus } from '../types';
import { Button, Card, Note, Pill, ProgressMeter, ReadingRow } from './ui';
import { showToast } from './toast';
import { t } from '../i18n';
import { formatAgeMs, formatBytes, formatUptime } from '../i18n/format';
import { deviceError } from '../i18n/deviceMessage';
import { bodyOf, post } from '../lib/api';

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

const IPV6_SCOPE_KEYS = { 'link-local': 'system.ipv6LinkLocal', 'unique-local': 'system.ipv6Local', global: 'system.ipv6Global' } as const;

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

type Status = SystemStatus;
type Rg15Action = { loading: boolean; message: string | null };

const restartDevice = async () => {
  try {
    await post('/api/restart');
    showToast({ message: t('common.restarting') });
  } catch {
    showToast({ message: t('system.couldNotReachTheDevice'), tone: 'bad' });
  }
};

// The RG-15's reset-total and reboot buttons, and what the device said.
const useRg15Actions = () => {
  const [rg15Action, setRg15Action] = useState<Rg15Action>({ loading: false, message: null });
  const runRg15Action = async (path: string, successMessage: string) => {
    setRg15Action({ loading: true, message: null });
    try {
      const response = await post(path);
      const data = await bodyOf(response);
      setRg15Action({
        loading: false,
        message: response.ok ? successMessage : deviceError(data, t('system.failed')),
      });
    } catch {
      setRg15Action({ loading: false, message: t('system.couldNotReachTheDevice') });
    }
  };
  return { rg15Action, runRg15Action };
};

const usedPercent = (used: number, total: number) => (total > 0 ? (used / total) * 100 : 0);

const FirmwareCard: FunctionalComponent<{ firmware: NonNullable<Status['firmware']> }> = ({ firmware }) => (
  <Card title={t('system.firmware')} icon="cpu" tone="cyan">
    <div class="system-row-grid">
      <InfoRow label={t('system.name')} value={firmware.name} />
      <InfoRow label={t('system.version')} value={`v${firmware.version}`} tone="tone-cyan" />
      <InfoRow label={t('system.build')} value={`${firmware.buildDate} ${firmware.buildTime}`} />
    </div>
  </Card>
);

const RuntimeCard: FunctionalComponent<{ status: Status }> = ({ status }) => {
  const heapUsedPercent = usedPercent(status.heapSize - status.freeHeap, status.heapSize);
  const flashUsedPercent = usedPercent(status.sketchSize, status.flashSize);
  const fsUsedPercent = usedPercent(status.fsUsed, status.fsTotal);
  return (
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
  );
};

const SensorsCard: FunctionalComponent<{ sensors: Status['sensors'] }> = ({ sensors }) => (
  <Card title={t('system.sensors')} icon="eye" tone="green">
    <div class="system-list">
      <SensorRow name="TSL2591 Light Sensor" status={sensors.light.status} />
      <SensorRow name="BME280 Environment" status={sensors.environment.status} />
      <SensorRow name="MLX90614 IR Temperature" status={sensors.infrared.status} />
      {sensors.gps && <SensorRow name="GPS Module" status={sensors.gps.status} />}
      {sensors.rain && <SensorRow name="RG-15 Rain Sensor" status={sensors.rain.status} />}
      {sensors.wind && <SensorRow name="Anemometer" status={sensors.wind.status} />}
    </div>
  </Card>
);

type RainDiagnostics = NonNullable<NonNullable<Status['diagnostics']>['rain']>;

const Rg15DiagnosticsDetail: FunctionalComponent<{ diagnostics: RainDiagnostics; ok: boolean }> = ({ diagnostics, ok }) => (
  <>
    <div class="system-diagnostic-grid">
      <InfoRow label={t('system.state')} value={diagnostics.state} tone={ok ? 'tone-green' : 'tone-red'} />
      <InfoRow label={t('system.rxTx')} value={`${diagnostics.rxPin} / ${diagnostics.txPin}`} />
      <InfoRow label={t('system.baudRate')} value={String(diagnostics.baudRate)} />
      <InfoRow label={t('system.successfulReads')} value={String(diagnostics.successfulReads)} />
      <InfoRow label={t('system.timeouts')} value={String(diagnostics.timeouts)} />
      <InfoRow label={t('system.parseErrors')} value={String(diagnostics.parseErrors)} />
      <InfoRow label={t('system.lastResponseAge')} value={formatAgeMs(diagnostics.lastResponseAgeMs)} />
      <InfoRow label={t('system.lastPollAge')} value={formatAgeMs(diagnostics.lastPollAgeMs)} />
    </div>

    <div class="system-log-row">
      <span>{t('system.lastCommandResponseError')}</span>
      <strong>{diagnostics.lastCommand ?? '--'}</strong>
      <em>{diagnostics.lastResponse ?? '--'}</em>
      <em>{diagnostics.lastError ?? '--'}</em>
    </div>
  </>
);

const Rg15Card: FunctionalComponent<{
  ok: boolean;
  diagnostics: RainDiagnostics | undefined;
  action: Rg15Action;
  run: (path: string, successMessage: string) => void;
}> = ({ ok, diagnostics, action, run }) => (
  <Card title={t('system.rg15Diagnostics')} icon="rain" tone="cyan">
    <div class="btn-row">
      <Button small disabled={action.loading} onClick={() => run('/api/sensors/rg15/reset-total', t('system.totalReset'))}>
        {t('system.resetTotal')}
      </Button>
      <Button small variant="danger" disabled={action.loading} onClick={() => run('/api/sensors/rg15/reboot', t('system.rg15Rebooting'))}>
        {t('system.rebootRg15')}
      </Button>
      {action.message && <Note>{action.message}</Note>}
    </div>

    {diagnostics && <Rg15DiagnosticsDetail diagnostics={diagnostics} ok={ok} />}
  </Card>
);

const PartitionsCard: FunctionalComponent<{ partitions: NonNullable<Status['partitions']> }> = ({ partitions }) => (
  <Card title={t('system.flashPartitions')} icon="upload" tone="cyan">
    <div class="system-row-grid">
      <InfoRow label={t('system.currentSlot')} value={partitions.runningSlot} />
      <InfoRow label={t('system.nextUpdateSlot')} value={partitions.nextSlot} />
      <InfoRow label={t('system.otaSlotSize')} value={formatBytes(partitions.runningSize)} />
      {partitions.nvs && (
        <>
          <InfoRow label={t('system.nvsUsed')} value={String(partitions.nvs.usedEntries)} />
          <InfoRow label={t('system.nvsFree')} value={String(partitions.nvs.freeEntries)} />
          <InfoRow label={t('system.namespaces')} value={String(partitions.nvs.namespaceCount)} />
        </>
      )}
    </div>
  </Card>
);

type Ntp = NonNullable<Status['ntp']>;

const ntpStatusText = (ntp: Ntp) => (ntp.synced ? t('system.synced') : ntp.status === 1 ? t('system.syncing') : t('system.notSynced'));

const NtpCard: FunctionalComponent<{ ntp: Ntp }> = ({ ntp }) => (
  <Card title={t('system.networkTimeNtp')} icon="gps" tone="green">
    {ntp.enabled ? (
      <div class="system-list">
        <InfoRow label={t('system.status')} value={ntpStatusText(ntp)} tone={ntp.synced ? 'tone-green' : 'tone-amber'} />
        <InfoRow label={t('system.server')} value={ntp.server} />
        <InfoRow label={t('system.lastSyncAge')} value={formatShortAgeMs(ntp.lastSync)} />
        <InfoRow label={t('system.nextSync')} value={formatShortAgeMs(ntp.nextSync)} />
        <InfoRow label={t('system.clockDrift')} value={`${ntp.drift}s`} />
      </div>
    ) : (
      <p class="system-subtle">{t('system.ntpIsDisabled')}</p>
    )}
  </Card>
);

const GpsTimeCard: FunctionalComponent<{ ntp: Ntp }> = ({ ntp }) => (
  <Card title={t('system.gpsTime')} icon="gps" tone="green">
    <div class="system-list">
      <InfoRow
        label={t('system.status')}
        value={ntp.gpsHasFix ? t('system.lockAcquired') : t('system.searching')}
        tone={ntp.gpsHasFix ? 'tone-green' : 'tone-amber'}
      />
      {ntp.gpsHasFix && ntp.gpsTimeUTC && <InfoRow label={t('system.gpsTimeUtc')} value={ntp.gpsTimeUTC} tone="tone-cyan" />}
      <InfoRow label={t('system.satellites')} value={String(ntp.gpsSatellites || 0)} />
    </div>
  </Card>
);

const MqttCard: FunctionalComponent<{ mqtt: NonNullable<Status['mqtt']> }> = ({ mqtt }) => (
  <Card title="MQTT" icon="wifi" tone="cyan">
    {mqtt.enabled ? (
      <div class="system-list">
        <InfoRow
          label={t('system.status')}
          value={mqtt.connected ? t('system.connected') : t('system.disconnected')}
          tone={mqtt.connected ? 'tone-green' : 'tone-red'}
        />
        <InfoRow label={t('system.broker')} value={`${mqtt.broker}:${mqtt.port}`} />
        <InfoRow label={t('system.topic')} value={mqtt.topic} />
      </div>
    ) : (
      <p class="system-subtle">{t('system.mqttIsDisabled')}</p>
    )}
  </Card>
);

const WifiCard: FunctionalComponent<{ wifi: Status['wifi'] }> = ({ wifi }) => (
  <Card title="WiFi" icon="wifi" tone="cyan">
    <div class="system-list">
      <InfoRow
        label={t('system.status')}
        value={wifi.connected ? t('system.connected') : t('system.disconnected')}
        tone={wifi.connected ? 'tone-green' : 'tone-red'}
      />
      {wifi.connected && (
        <>
          <InfoRow label="SSID" value={wifi.ssid} />
          <InfoRow label={t('system.ipAddress')} value={wifi.ip} tone="tone-cyan" />
          {wifi.ipv6?.addresses.map((entry) => (
            <InfoRow
              key={entry.address}
              label={t('system.ipv6Scope', { scope: t(IPV6_SCOPE_KEYS[entry.scope]) })}
              value={entry.address}
              tone="tone-cyan"
            />
          ))}
          <InfoRow label={t('system.signal')} value={`${wifi.rssi} dBm`} />
          <InfoRow label={t('system.macAddress')} value={wifi.mac} />
        </>
      )}
    </div>
  </Card>
);

const ActionsCard: FunctionalComponent<{ confirming: boolean; setConfirming: (confirming: boolean) => void }> = ({
  confirming,
  setConfirming,
}) => (
  <Card title={t('system.actions')} icon="cpu" tone="red">
    <div class="btn-row">
      {confirming ? (
        <>
          <Button
            variant="danger"
            onClick={() => {
              setConfirming(false);
              void restartDevice();
            }}
          >
            {t('system.restartNow')}
          </Button>
          <Button variant="ghost" onClick={() => setConfirming(false)}>
            {t('system.cancel')}
          </Button>
        </>
      ) : (
        <Button variant="danger" onClick={() => setConfirming(true)}>
          {t('system.restartDevice')}
        </Button>
      )}
    </div>
  </Card>
);

const System: FunctionalComponent = () => {
  const { data: status, connected } = useWebSocket<SystemStatus>('/ws/status');
  const { rg15Action, runRg15Action } = useRg15Actions();
  const [confirmRestart, setConfirmRestart] = useState(false);

  if (!connected || !status) {
    return (
      <div class="empty-state">
        <h2>{t('common.loading')}</h2>
        <p>{t('system.connectingToDevice')}</p>
      </div>
    );
  }

  const rain = status.sensors.rain;

  return (
    <div class="panel-page system-page page-enter">
      {status.firmware && <FirmwareCard firmware={status.firmware} />}
      <RuntimeCard status={status} />
      <SensorsCard sensors={status.sensors} />
      {rain && <Rg15Card ok={rain.status === 'ok'} diagnostics={status.diagnostics?.rain} action={rg15Action} run={runRg15Action} />}
      {status.partitions && <PartitionsCard partitions={status.partitions} />}
      {status.ntp && <NtpCard ntp={status.ntp} />}
      {status.ntp && status.ntp.gpsEnabled && <GpsTimeCard ntp={status.ntp} />}
      {status.mqtt && <MqttCard mqtt={status.mqtt} />}
      <WifiCard wifi={status.wifi} />
      <ActionsCard confirming={confirmRestart} setConfirming={setConfirmRestart} />
    </div>
  );
};

export default System;
