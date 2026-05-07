import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { useWebSocket } from '../hooks/useWebSocket';
import type { Config, SensorData, SystemStatus } from '../types';
import { Card, Icon, MetricTile, Pill, ReadingRow, SensorReadingRow } from './ui';

const formatNumber = (value: number | undefined, digits: number) =>
  typeof value === 'number' && Number.isFinite(value) ? value.toFixed(digits) : '--';

const formatAgeMs = (value: number | null | undefined) => {
  if (typeof value !== 'number' || !Number.isFinite(value)) return '--';
  if (value < 1000) return `${value} ms`;
  if (value < 60000) return `${(value / 1000).toFixed(1)} s`;
  return `${Math.floor(value / 60000)}m ${Math.floor((value % 60000) / 1000)}s`;
};

const formatUptime = (seconds: number | undefined) => {
  if (typeof seconds !== 'number' || !Number.isFinite(seconds)) return '--';
  const days = Math.floor(seconds / 86400);
  const hours = Math.floor((seconds % 86400) / 3600);
  const minutes = Math.floor((seconds % 3600) / 60);
  return days > 0 ? `${days}d ${hours}h ${minutes}m` : `${hours}h ${minutes}m`;
};

const bortleTone = (bortle?: number): string => {
  if (typeof bortle !== 'number') return 'tone-muted';
  if (bortle <= 2) return 'tone-cyan';
  if (bortle <= 4) return 'tone-green';
  if (bortle <= 6) return 'tone-amber';
  return 'tone-red';
};

const conditionTone = (cover?: number): string => {
  if (typeof cover !== 'number') return 'pill-dim';
  if (cover < 15) return 'pill-green';
  if (cover < 40) return 'pill-cyan';
  if (cover < 70) return 'pill-amber';
  return 'pill-red';
};

const conditionLabel = (cover?: number): string => {
  if (typeof cover !== 'number') return 'Unknown';
  if (cover < 15) return 'Clear';
  if (cover < 40) return 'Mostly Clear';
  if (cover < 70) return 'Partly Cloudy';
  return 'Overcast';
};

const rssiTone = (rssi?: number) => {
  if (typeof rssi !== 'number') return { tone: 'pill-dim', label: 'Unknown' };
  if (rssi > -60) return { tone: 'pill-green', label: 'Strong' };
  if (rssi > -75) return { tone: 'pill-cyan', label: 'Good' };
  if (rssi > -85) return { tone: 'pill-amber', label: 'Weak' };
  return { tone: 'pill-red', label: 'Poor' };
};

const StatusDot: FunctionalComponent<{ ok: boolean }> = ({ ok }) => (
  <span class={`status-dot ${ok ? 'is-ok' : 'is-bad'}`} aria-hidden="true" />
);

const MiniSpark: FunctionalComponent<{ values: number[]; tone: string }> = ({ values, tone }) => {
  if (values.length < 2) return <div class="sparkline" />;
  const min = Math.min(...values);
  const max = Math.max(...values);
  const range = max - min || 1;
  const points = values.map((value, index) => ({
    x: (index / (values.length - 1)) * 100,
    y: 34 - ((value - min) / range) * 30,
  }));
  const linePath = points.reduce((path, point, index) => {
    if (index === 0) return `M ${point.x.toFixed(1)} ${point.y.toFixed(1)}`;

    const previous = points[index - 1];
    const beforePrevious = points[index - 2] ?? previous;
    const next = points[index + 1] ?? point;
    const smoothing = 0.18;
    const controlStart = {
      x: previous.x + (point.x - beforePrevious.x) * smoothing,
      y: previous.y + (point.y - beforePrevious.y) * smoothing,
    };
    const controlEnd = {
      x: point.x - (next.x - previous.x) * smoothing,
      y: point.y - (next.y - previous.y) * smoothing,
    };

    return [
      path,
      'C',
      controlStart.x.toFixed(1),
      controlStart.y.toFixed(1),
      controlEnd.x.toFixed(1),
      controlEnd.y.toFixed(1),
      point.x.toFixed(1),
      point.y.toFixed(1),
    ].join(' ');
  }, '');
  const fillPath = `${linePath} L 100 36 L 0 36 Z`;

  return (
    <svg class={`sparkline ${tone}`} viewBox="0 0 100 36" preserveAspectRatio="none" aria-hidden="true">
      <path d={fillPath} class="spark-fill" />
      <path d={linePath} class="spark-line" />
    </svg>
  );
};

const Dashboard: FunctionalComponent = () => {
  const { data: sensors, connected, lastMessageAt } = useWebSocket<SensorData>('/ws/sensors');
  const { data: status } = useWebSocket<SystemStatus>('/ws/status');
  const [config, setConfig] = useState<Config | null>(null);
  const [sqmHistory, setSqmHistory] = useState<number[]>([]);

  useEffect(() => {
    fetch('/api/config')
      .then((response) => (response.ok ? response.json() : null))
      .then((data) => setConfig(data))
      .catch(() => setConfig(null));
  }, []);

  useEffect(() => {
    const sqm = sensors?.skyQuality?.sqm;
    if (typeof sqm !== 'number' || !Number.isFinite(sqm)) return;
    setSqmHistory((history) => {
      if (history.length === 0) {
        return Array.from({ length: 24 }, (_, index) => sqm + Math.sin(index / 3) * 0.06);
      }
      return [...history.slice(-23), sqm];
    });
  }, [sensors?.skyQuality?.sqm]);

  const rainUnits = config?.rain?.units === 'imperial'
    ? { depth: 'in', intensity: 'in/hr' }
    : { depth: 'mm', intensity: 'mm/hr' };
  const rain = sensors?.rainSensor;
  const payloadTimestamp = sensors?.dataTimestamp;
  const dataTimestamp = payloadTimestamp && payloadTimestamp > 1000000000000
    ? payloadTimestamp
    : lastMessageAt;
  const dataAgeSeconds = dataTimestamp ? Math.max(0, Math.floor((Date.now() - dataTimestamp) / 1000)) : null;
  const isStale = dataAgeSeconds !== null && config?.sensor?.readIntervalMs
    ? dataAgeSeconds * 1000 > config.sensor.readIntervalMs * 2
    : false;
  const live = connected && Boolean(sensors) && !isStale;
  const skyTone = bortleTone(sensors?.skyQuality?.bortle);
  const cloudCover = sensors?.cloudConditions?.cloudCoverPercent;
  const rssi = rssiTone(status?.wifi?.rssi);

  const rainStatus = !rain?.enabled
    ? { text: 'Disabled', tone: 'pill-dim' }
    : rain.stale
      ? { text: 'Stale', tone: 'pill-amber' }
      : rain.online
        ? { text: 'Online', tone: 'pill-green' }
        : { text: 'Offline', tone: 'pill-red' };

  if (!connected || !sensors) {
    return (
      <div class="empty-state">
        <StatusDot ok={false} />
        <h2>{connected ? 'Waiting for sensor data' : 'Connecting to SQMeter'}</h2>
        <p>{connected ? 'The dashboard will populate when the next reading arrives.' : 'Opening the live WebSocket stream.'}</p>
      </div>
    );
  }

  return (
    <div class="dashboard page-enter">
      <div class="dashboard-layout">
        <div class="dashboard-left">
          <section class={`hero-card ${skyTone}`}>
            <div class="hero-topline">
              <div class="card-title flat">
                <Icon name="star" tone="cyan" />
                <h2>Sky Quality</h2>
              </div>
              <div class="hero-pills">
                <Pill tone={live ? 'pill-green' : isStale ? 'pill-amber' : 'pill-dim'}>
                  <StatusDot ok={live} /> {live ? 'Live' : isStale ? 'Stale' : 'Connected'}
                </Pill>
                <Pill tone={skyTone.replace('tone-', 'pill-')}>
                  Bortle {formatNumber(sensors.skyQuality?.bortle, 0)}
                </Pill>
              </div>
            </div>

            <div class="sqm-display">
              <div class="sqm-value">{formatNumber(sensors.skyQuality?.sqm, 2)}</div>
              <div class="sqm-unit">mag / arcsec²</div>
              <p>{sensors.skyQuality?.description ?? 'Sky quality data unavailable'}</p>
            </div>

            <MiniSpark values={sqmHistory} tone={skyTone} />

            <div class="metric-grid compact">
              <MetricTile label="Bortle" value={formatNumber(sensors.skyQuality?.bortle, 0)} tone={skyTone} />
              <MetricTile label="NELM" value={formatNumber(sensors.skyQuality?.nelm, 1)} unit="mag" tone="tone-cyan" />
              <MetricTile label="Illuminance" value={formatNumber(sensors.lightSensor?.lux, 5)} unit="lux" />
            </div>
          </section>

          {sensors.cloudConditions && (
            <Card
              title="Cloud Conditions"
              icon="cloud"
              tone="violet"
              actions={
                <>
                  <Pill tone={conditionTone(cloudCover)}>{conditionLabel(cloudCover)}</Pill>
                </>
              }
            >
              <div class="metric-grid">
                <MetricTile label="Cloud Cover" value={formatNumber(cloudCover, 0)} unit="%" tone="tone-violet" />
                <MetricTile label="Temp Delta" value={formatNumber(sensors.cloudConditions.temperatureDelta, 1)} unit="C" />
                <MetricTile label="Corrected" value={formatNumber(sensors.cloudConditions.correctedDelta, 1)} unit="C" />
              </div>
            </Card>
          )}
        </div>

        <div class="dashboard-right">
          {sensors.environment && sensors.environment.status === 0 && (
            <Card title="Environment" icon="therm" tone="amber">
              <div class="tile-grid two">
                <MetricTile label="Temperature" value={formatNumber(sensors.environment.temperature, 1)} unit="C" tone="tone-amber" />
                <MetricTile label="Humidity" value={formatNumber(sensors.environment.humidity, 1)} unit="%" tone={sensors.environment.humidity > 80 ? 'tone-amber' : 'tone-cyan'} />
                <MetricTile label="Pressure" value={formatNumber(sensors.environment.pressure, 1)} unit="hPa" />
                <MetricTile label="Dew Point" value={formatNumber(sensors.environment.dewpoint, 1)} unit="C" tone="tone-violet" />
              </div>
            </Card>
          )}

          {sensors.gps && (
            <Card
              title="GPS Location"
              icon="gps"
              tone={sensors.gps.hasFix ? 'green' : 'muted'}
              actions={
                <>
                  <Pill tone={sensors.gps.hasFix ? 'pill-green' : 'pill-dim'}>
                    {sensors.gps.hasFix ? 'Lock acquired' : 'No fix'}
                  </Pill>
                </>
              }
            >
              <div class="coordinate-line mono">
                {sensors.gps.hasFix
                  ? `${Math.abs(sensors.gps.latitude).toFixed(6)} ${sensors.gps.latitude >= 0 ? 'N' : 'S'}, ${Math.abs(sensors.gps.longitude).toFixed(6)} ${sensors.gps.longitude >= 0 ? 'E' : 'W'}`
                  : '--'}
              </div>
              <div class="tile-grid gps-metrics">
                <MetricTile label="Satellites" value={String(sensors.gps.satellites)} tone="tone-green" />
                <MetricTile label="Altitude" value={sensors.gps.hasFix ? formatNumber(sensors.gps.altitude, 0) : '--'} unit="m" />
                <MetricTile label="HDOP" value={sensors.gps.hasFix ? formatNumber(sensors.gps.hdop, 1) : '--'} />
                <MetricTile label="Fix Age" value={formatAgeMs(sensors.gps.age)} />
              </div>
            </Card>
          )}

          {sensors.lightSensor && (
            <Card title="Light Sensor" icon="eye" tone="cyan">
              <SensorReadingRow label="Illuminance" value={sensors.lightSensor.status === 0 ? formatNumber(sensors.lightSensor.lux, 5) : '--'} unit="lux" />
              <SensorReadingRow label="Visible" value={sensors.lightSensor.status === 0 ? String(sensors.lightSensor.visible) : '--'} unit="raw" />
              <SensorReadingRow label="Infrared" value={sensors.lightSensor.status === 0 ? String(sensors.lightSensor.infrared) : '--'} unit="raw" />
              <SensorReadingRow label="Full spectrum" value={sensors.lightSensor.status === 0 ? String(sensors.lightSensor.full) : '--'} unit="raw" />
            </Card>
          )}

          <Card title="Device & Network" icon="wifi" tone="cyan">
            <div class="tile-grid two">
              <div class="metric-tile left">
                <div class="metric-label">Wi-Fi</div>
                <Pill tone={rssi.tone}>{rssi.label}</Pill>
                <div class="metric-sub mono">{status?.wifi?.rssi ?? '--'} dBm</div>
                <div class="metric-sub">{status?.wifi?.ssid ?? '--'}</div>
              </div>
              <div class="metric-tile left">
                <div class="metric-label">IP Address</div>
                <div class="metric-value tone-cyan small">{status?.wifi?.ip ?? '--'}</div>
                <div class="metric-label pushed">Uptime</div>
                <div class="metric-sub mono">{formatUptime(status?.uptime)}</div>
              </div>
            </div>
            {status?.firmware?.version && (
              <div class="firmware-row">
                <span>Firmware</span>
                <Pill>v{status.firmware.version}</Pill>
              </div>
            )}
          </Card>

          {sensors.irTemperature && (
            <Card title="IR Temperature" icon="therm" tone="violet">
              <ReadingRow
                label="Sky temperature"
                value={`${sensors.irTemperature.status === 0 ? formatNumber(sensors.irTemperature.objectTemp, 1) : '--'} C`}
              />
              <ReadingRow
                label="Ambient"
                value={`${sensors.irTemperature.status === 0 ? formatNumber(sensors.irTemperature.ambientTemp, 1) : '--'} C`}
              />
            </Card>
          )}

          {rain && (
            <Card
              title="Rain Sensor"
              icon="rain"
              tone="cyan"
              actions={
                <>
                  <Pill tone={rainStatus.tone}>{rainStatus.text}</Pill>
                </>
              }
            >
              <div class="metric-grid">
                <MetricTile label="Raining" value={(rain.raining ?? rain.isRaining) ? 'Yes' : 'No'} tone={(rain.raining ?? rain.isRaining) ? 'tone-amber' : 'tone-green'} />
                <MetricTile label="Intensity" value={rain.enabled ? formatNumber(rain.rain_intensity ?? rain.rInt, 1) : '--'} unit={rainUnits.intensity} />
                <MetricTile label="Event" value={rain.enabled ? formatNumber(rain.event_accumulation ?? rain.eventAcc, 2) : '--'} unit={rainUnits.depth} />
                <MetricTile label="Daily" value={rain.enabled ? formatNumber(rain.total_accumulation ?? rain.totalAcc, 2) : '--'} unit={rainUnits.depth} />
              </div>
              {(rain.lensBad || rain.emSat) && (
                <div class="warning-list">
                  {rain.lensBad && <span>LensBad: clean or inspect the lens.</span>}
                  {rain.emSat && <span>EmSat: emitter saturation detected.</span>}
                </div>
              )}
            </Card>
          )}
        </div>
      </div>
    </div>
  );
};

export default Dashboard;
