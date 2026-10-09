import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { useWebSocket } from '../hooks/useWebSocket';
import { useQuiet } from '../hooks/useQuiet';
import type { Config, SensorData, SystemStatus } from '../types';
import { Button, Card, Icon, MetricTile, Note, Pill, ReadingRow, SensorReadingRow } from './ui';
import SafetyCard from './SafetyCard';
import SunMoonCard from './SunMoonCard';
import { deviceTime } from '../lib/deviceTime';
import { summariseSeries, useAnnounceChange } from '../lib/a11y';
import Masonry, { MasonryItem, mergeOrder, moveInOrder } from './Masonry';
import { t } from '../i18n';
import { formatAgeMs, formatNumber, formatUptime } from '../i18n/format';
import { deviceText } from '../i18n/deviceMessage';

const bortleTone = (bortle?: number): string => {
  if (typeof bortle !== 'number') return 'tone-muted';
  if (bortle <= 2) return 'tone-cyan';
  if (bortle <= 4) return 'tone-green';
  if (bortle <= 6) return 'tone-amber';
  return 'tone-red';
};

// The device classifies the sky (with the thresholds in Settings); show its verdict.
const CONDITION_TONE: Record<string, string> = { clear: 'pill-green', cloudy: 'pill-amber', overcast: 'pill-red' };
const conditionTone = (condition?: string): string => (condition && CONDITION_TONE[condition]) || 'pill-dim';

// Highlight a wind reading against its safety limit: red at or above, amber
// within 80% of it; plain when that limit is off.
const windTone = (value: number | undefined, enabled: boolean | undefined, limit: number | undefined, base = '') => {
  if (typeof value !== 'number' || !enabled || typeof limit !== 'number' || limit <= 0) return base;
  if (value >= limit) return 'tone-red';
  if (value >= limit * 0.8) return 'tone-amber';
  return base;
};

const rssiTone = (rssi?: number) => {
  if (typeof rssi !== 'number') return { tone: 'pill-dim', label: t('dashboard.unknown') };
  if (rssi > -60) return { tone: 'pill-green', label: t('dashboard.strong') };
  if (rssi > -75) return { tone: 'pill-cyan', label: t('dashboard.good') };
  if (rssi > -85) return { tone: 'pill-amber', label: t('dashboard.weak') };
  return { tone: 'pill-red', label: t('dashboard.poor') };
};

const compassPoint = (degrees: number) => t('dashboard.compassPoints').split(' ')[Math.round(degrees / 22.5) % 16];

const StatusDot: FunctionalComponent<{ ok: boolean }> = ({ ok }) => (
  <span class={`status-dot ${ok ? 'is-ok' : 'is-bad'}`} aria-hidden="true" />
);

// `label` names the series; screen readers get its trend in words (spec 022).
const MiniSpark: FunctionalComponent<{ values: number[]; tone: string; label: string }> = ({ values, tone, label }) => {
  const summary = summariseSeries(label, values, (value) => value.toFixed(2));
  if (values.length < 2) return <div class="sparkline" role="img" aria-label={summary} />;
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
    <svg class={`sparkline ${tone}`} viewBox="0 0 100 36" preserveAspectRatio="none" role="img" aria-label={summary}>
      <path d={fillPath} class="spark-fill" />
      <path d={linePath} class="spark-line" />
    </svg>
  );
};

const DEFAULT_ORDER = ['safety', 'sky', 'sunmoon', 'cloud', 'environment', 'gps', 'light', 'device', 'ir', 'wind', 'rain'];
const ORDER_KEY = 'sqm.dashboard.order';

// Card order is a per-browser preference.
const loadOrder = (): string[] => {
  try {
    const value = JSON.parse(localStorage.getItem(ORDER_KEY) ?? '[]');
    return Array.isArray(value) ? value.filter((id) => typeof id === 'string') : [];
  } catch {
    return [];
  }
};
const saveOrder = (order: string[]) => {
  try {
    if (order.length) localStorage.setItem(ORDER_KEY, JSON.stringify(order));
    else localStorage.removeItem(ORDER_KEY);
  } catch {
    // storage unavailable: the order just isn't remembered
  }
};

const Dashboard: FunctionalComponent = () => {
  const [savedOrder, setSavedOrder] = useState<string[]>(loadOrder);
  const [arranging, setArranging] = useState(false);
  const { data: sensors, connected, lastMessageAt } = useWebSocket<SensorData>('/ws/sensors');
  const quiet = useQuiet(lastMessageAt);
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
    const sqm = sensors?.sky?.sqm;
    if (typeof sqm !== 'number' || !Number.isFinite(sqm)) return;
    // Real readings only: the trend draws once there are two.
    setSqmHistory((history) => [...history.slice(-23), sqm]);
  }, [sensors?.sky?.sqm]);

  // The verdict is the one live value announced on its own, once per change
  // (spec 022 FR-009); readings are read on demand.
  useAnnounceChange(sensors?.safety?.safe, (safe) =>
    safe
      ? t('dashboard.observatorySafe')
      : t('dashboard.observatoryUnsafeValue', { value: sensors?.safety?.reasons?.length ? `: ${sensors.safety.reasons.join(', ')}` : '' }),
  );

  // The device reports rain in mm; show it in the units the RG-15 is set to.
  const imperialRain = config?.rain?.units === 'imperial';
  const rainUnits = imperialRain ? { depth: 'in', intensity: 'in/hr' } : { depth: 'mm', intensity: 'mm/hr' };
  const rainValue = (mm: number | undefined) => (typeof mm === 'number' && imperialRain ? mm / 25.4 : mm);
  const rain = sensors?.rain;
  // Stale: the device says its data is old, or the stream has gone quiet.
  const isStale = Boolean(sensors?.dataStale) || quiet;
  const live = connected && Boolean(sensors) && !isStale;
  const skyTone = bortleTone(sensors?.sky?.bortle);
  // Cards only show for sensors that are switched on and responding.
  const lightOk = sensors?.light?.status === 'ok';
  const irOk = sensors?.infrared?.status === 'ok';
  const cloudCover = sensors?.clouds?.coverPercent;
  const rssi = rssiTone(status?.wifi?.rssi);
  // A GPS fix wins over the location typed into Settings.
  const location =
    sensors?.gps?.fix && sensors.gps.latitude !== undefined && sensors.gps.longitude !== undefined
      ? { latitude: sensors.gps.latitude, longitude: sensors.gps.longitude }
      : config?.location?.set
        ? config.location
        : null;
  const showSunMoon = location !== null && config?.location?.showSunMoon !== false;

  const rainStatus =
    rain?.status === 'ok'
      ? { text: t('dashboard.online'), tone: 'pill-green' }
      : rain?.status === 'stale'
        ? { text: t('dashboard.stale'), tone: 'pill-amber' }
        : { text: t('dashboard.offline'), tone: 'pill-red' };

  if (!connected || !sensors) {
    return (
      <div class="empty-state">
        <StatusDot ok={false} />
        <h2>{connected ? t('dashboard.waitingForSensorData') : t('dashboard.connectingToSqmeter')}</h2>
        <p>{connected ? t('dashboard.theDashboardWillPopulateWhen') : t('dashboard.openingTheLiveWebsocketStream')}</p>
      </div>
    );
  }

  const cards: (MasonryItem | false | null | undefined)[] = [
    { id: 'safety', title: t('dashboard.safety'), node: <SafetyCard safety={sensors.safety} /> },
    {
      id: 'sky',
      title: t('dashboard.skyQuality'),
      node: !lightOk ? (
        <Card
          title={t('dashboard.skyQuality')}
          icon="star"
          tone="muted"
          actions={<Pill tone="pill-red">{t('dashboard.notDetected')}</Pill>}
        >
          <Note>{t('dashboard.theTsl2591LightSensorIsn')}</Note>
        </Card>
      ) : (
        <section class={`hero-card ${skyTone}`}>
          <div class="hero-topline">
            <div class="card-title flat">
              <Icon name="star" tone="cyan" />
              <h2>{t('dashboard.skyQuality')}</h2>
            </div>
            <div class="hero-pills">
              <Pill tone={live ? 'pill-green' : isStale ? 'pill-amber' : 'pill-dim'}>
                <StatusDot ok={live} /> {live ? t('dashboard.live') : isStale ? t('dashboard.stale') : t('dashboard.connected')}
              </Pill>
              <Pill tone={skyTone.replace('tone-', 'pill-')}>
                {t('dashboard.bortleNumber', { number: formatNumber(sensors.sky.bortle, 0) })}
              </Pill>
            </div>
          </div>

          <div class="sqm-display">
            <div class="sqm-value">{formatNumber(sensors.sky.sqm, 2)}</div>
            <div class="sqm-unit">{t('dashboard.magArcsec')}</div>
            <p>{sensors.sky.description ? deviceText(sensors.sky.description) : t('dashboard.skyQualityDataUnavailable')}</p>
          </div>

          <MiniSpark values={sqmHistory} tone={skyTone} label={t('dashboard.skyQuality2')} />

          <div class="metric-grid compact">
            <MetricTile label={t('dashboard.bortle')} value={formatNumber(sensors.sky.bortle, 0)} tone={skyTone} />
            <MetricTile label="NELM" value={formatNumber(sensors.sky.nelm, 1)} unit="mag" tone="tone-cyan" />
            <MetricTile label={t('dashboard.illuminance')} value={formatNumber(sensors.light.lux, 5)} unit="lux" />
          </div>
        </section>
      ),
    },
    showSunMoon &&
      location && {
        id: 'sunmoon',
        title: t('dashboard.sunMoon'),
        node: <SunMoonCard latitude={location.latitude} longitude={location.longitude} deviceNow={deviceTime(status)} />,
      },
    irOk && {
      id: 'cloud',
      title: t('dashboard.cloudConditions'),
      node: (
        <Card
          title={t('dashboard.cloudConditions')}
          icon="cloud"
          tone="violet"
          actions={
            <>
              <Pill tone={conditionTone(sensors.clouds.condition)}>
                {sensors.clouds.description ? deviceText(sensors.clouds.description) : t('dashboard.unknown')}
              </Pill>
            </>
          }
        >
          <div class="metric-grid">
            <MetricTile label={t('dashboard.cloudCover')} value={formatNumber(cloudCover, 0)} unit="%" tone="tone-violet" />
            <MetricTile label={t('dashboard.tempDelta')} value={formatNumber(sensors.clouds.temperatureDelta, 1)} unit="°C" />
            <MetricTile label={t('dashboard.corrected')} value={formatNumber(sensors.clouds.correctedDelta, 1)} unit="°C" />
          </div>
          {sensors.clouds.humiditySource === 'assumed' && (
            <Note>{t('dashboard.humidityAssumedNumberNoHumidity', { number: formatNumber(sensors.clouds.humidity, 0) })}</Note>
          )}
        </Card>
      ),
    },
    sensors.environment.status === 'ok' && {
      id: 'environment',
      title: t('dashboard.environment'),
      node: (
        <Card title={t('dashboard.environment')} icon="therm" tone="amber">
          <div class="tile-grid two">
            <MetricTile
              label={t('dashboard.temperature')}
              value={formatNumber(sensors.environment.temperature, 1)}
              unit="°C"
              tone="tone-amber"
            />
            <MetricTile
              label={t('dashboard.humidity')}
              value={formatNumber(sensors.environment.humidity, 1)}
              unit="%"
              tone={(sensors.environment.humidity ?? 0) > 80 ? 'tone-amber' : 'tone-cyan'}
            />
            <MetricTile label={t('dashboard.pressure')} value={formatNumber(sensors.environment.pressure, 1)} unit="hPa" />
            <MetricTile
              label={t('dashboard.dewPoint')}
              value={formatNumber(sensors.environment.dewpoint, 1)}
              unit="°C"
              tone="tone-violet"
            />
          </div>
        </Card>
      ),
    },
    sensors.gps && {
      id: 'gps',
      title: 'GPS',
      node: (
        <Card
          title={t('dashboard.gpsLocation')}
          icon="gps"
          tone={sensors.gps.fix ? 'green' : 'muted'}
          actions={
            <>
              <Pill tone={sensors.gps.fix ? 'pill-green' : 'pill-dim'}>
                {sensors.gps.fix ? t('dashboard.lockAcquired') : t('dashboard.noFix')}
              </Pill>
            </>
          }
        >
          <div class="coordinate-line mono">
            {sensors.gps.fix
              ? `${Math.abs(sensors.gps.latitude ?? 0).toFixed(6)} ${(sensors.gps.latitude ?? 0) >= 0 ? 'N' : 'S'}, ${Math.abs(sensors.gps.longitude ?? 0).toFixed(6)} ${(sensors.gps.longitude ?? 0) >= 0 ? 'E' : 'W'}`
              : '--'}
          </div>
          <div class="tile-grid gps-metrics">
            <MetricTile label={t('dashboard.satellites')} value={String(sensors.gps.satellites ?? 0)} tone="tone-green" />
            <MetricTile label={t('dashboard.altitude')} value={sensors.gps.fix ? formatNumber(sensors.gps.altitude, 0) : '--'} unit="m" />
            <MetricTile label="HDOP" value={sensors.gps.fix ? formatNumber(sensors.gps.hdop, 1) : '--'} />
            <MetricTile label={t('dashboard.fixAge')} value={formatAgeMs(sensors.gps.ageMs)} />
          </div>
        </Card>
      ),
    },
    lightOk && {
      id: 'light',
      title: t('dashboard.lightSensor'),
      node: (
        <Card title={t('dashboard.lightSensor')} icon="eye" tone="cyan">
          <SensorReadingRow label={t('dashboard.illuminance')} value={formatNumber(sensors.light.lux, 5)} unit="lux" />
          <SensorReadingRow label={t('dashboard.visible')} value={String(sensors.light.visible)} unit={t('dashboard.rawUnit')} />
          <SensorReadingRow label={t('dashboard.infrared')} value={String(sensors.light.infrared)} unit={t('dashboard.rawUnit')} />
          <SensorReadingRow label={t('dashboard.fullSpectrum')} value={String(sensors.light.full)} unit={t('dashboard.rawUnit')} />
        </Card>
      ),
    },
    {
      id: 'device',
      title: t('dashboard.deviceNetwork'),
      node: (
        <Card title={t('dashboard.deviceNetwork')} icon="wifi" tone="cyan">
          <div class="tile-grid two">
            <div class="metric-tile left">
              <div class="metric-label">{t('dashboard.wifi')}</div>
              <Pill tone={rssi.tone}>{rssi.label}</Pill>
              <div class="metric-sub mono">{status?.wifi?.rssi ?? '--'} dBm</div>
              <div class="metric-sub">{status?.wifi?.ssid ?? '--'}</div>
            </div>
            <div class="metric-tile left">
              <div class="metric-label">{t('dashboard.ipAddress')}</div>
              <div class="metric-value tone-cyan small">{status?.wifi?.ip ?? '--'}</div>
              <div class="metric-label pushed">{t('dashboard.uptime')}</div>
              <div class="metric-sub mono">{formatUptime(status?.uptime)}</div>
            </div>
          </div>
          {status?.firmware?.version && (
            <div class="firmware-row">
              <span>{t('dashboard.firmware')}</span>
              <Pill>v{status.firmware.version}</Pill>
            </div>
          )}
        </Card>
      ),
    },
    irOk && {
      id: 'ir',
      title: t('dashboard.irTemperature'),
      node: (
        <Card title={t('dashboard.irTemperature')} icon="therm" tone="violet">
          <ReadingRow label={t('dashboard.skyTemperature')} value={`${formatNumber(sensors.infrared.skyTemperature, 1)} °C`} />
          <ReadingRow label={t('dashboard.ambient')} value={`${formatNumber(sensors.infrared.ambientTemperature, 1)} °C`} />
        </Card>
      ),
    },
    sensors.wind && {
      id: 'wind',
      title: t('dashboard.wind'),
      node: (
        <Card
          title={t('dashboard.wind')}
          icon="cloud"
          tone="cyan"
          actions={
            <Pill tone={sensors.wind.status === 'ok' ? 'pill-green' : 'pill-red'}>
              {sensors.wind.status === 'ok' ? t('dashboard.online') : t('dashboard.offline')}
            </Pill>
          }
        >
          <div class="metric-grid">
            <MetricTile
              label={t('dashboard.speed')}
              value={formatNumber(sensors.wind.speed, 1)}
              unit={t('dashboard.mSNumberKmH', { number: formatNumber((sensors.wind.speed ?? 0) * 3.6, 0) })}
              tone={windTone(sensors.wind.speed, config?.alpaca?.windSpeedUnsafeEnabled, config?.alpaca?.windSpeedUnsafeMs, 'tone-cyan')}
            />
            <MetricTile
              label={t('dashboard.gust')}
              value={formatNumber(sensors.wind.gust, 1)}
              unit={t('dashboard.mSNumberKmH', { number: formatNumber((sensors.wind.gust ?? 0) * 3.6, 0) })}
              tone={windTone(sensors.wind.gust, config?.alpaca?.windGustUnsafeEnabled, config?.alpaca?.windGustUnsafeMs)}
            />
            <MetricTile
              label={t('dashboard.direction')}
              value={sensors.wind.direction !== undefined ? compassPoint(sensors.wind.direction) : '--'}
              unit={
                sensors.wind.direction !== undefined
                  ? `${formatNumber(sensors.wind.direction, 0)}°`
                  : sensors.wind.vaneFault
                    ? t('dashboard.vaneFault')
                    : t('dashboard.calm')
              }
            />
          </div>
        </Card>
      ),
    },
    rain && {
      id: 'rain',
      title: t('dashboard.rainSensor'),
      node: (
        <Card
          title={t('dashboard.rainSensor')}
          icon="rain"
          tone="cyan"
          actions={
            <>
              <Pill tone={rainStatus.tone}>{rainStatus.text}</Pill>
            </>
          }
        >
          <div class="metric-grid">
            <MetricTile
              label={t('dashboard.raining')}
              value={rain.raining ? t('dashboard.yes') : rain.status === 'ok' ? t('dashboard.no') : '--'}
              tone={rain.raining ? 'tone-amber' : 'tone-green'}
            />
            <MetricTile label={t('dashboard.intensity')} value={formatNumber(rainValue(rain.intensity), 1)} unit={rainUnits.intensity} />
            <MetricTile label={t('dashboard.event')} value={formatNumber(rainValue(rain.eventAccumulation), 2)} unit={rainUnits.depth} />
            <MetricTile label={t('dashboard.daily')} value={formatNumber(rainValue(rain.totalAccumulation), 2)} unit={rainUnits.depth} />
          </div>
          {(rain.lensFault || rain.emitterSaturated) && (
            <div class="warning-list">
              {rain.lensFault && <span>{t('dashboard.lensFaultCleanOrInspect')}</span>}
              {rain.emitterSaturated && <span>{t('dashboard.emitterSaturationDetected')}</span>}
            </div>
          )}
        </Card>
      ),
    },
  ];
  const visible = cards.filter((card): card is MasonryItem => Boolean(card));
  const fullOrder = mergeOrder(savedOrder, DEFAULT_ORDER);
  const ordered = fullOrder.map((id) => visible.find((card) => card.id === id)).filter((card): card is MasonryItem => Boolean(card));

  const move = (id: string, toIndex: number) => {
    const next = moveInOrder(
      fullOrder,
      ordered.map((card) => card.id),
      id,
      toIndex,
    );
    setSavedOrder(next);
    saveOrder(next);
  };

  return (
    <div class="dashboard page-enter">
      <div class="dashboard-toolbar">
        {arranging && (
          <Button
            small
            variant="ghost"
            onClick={() => {
              setSavedOrder([]);
              saveOrder([]);
            }}
          >
            {t('dashboard.resetOrder')}
          </Button>
        )}
        <Button small variant={arranging ? 'primary' : 'ghost'} onClick={() => setArranging(!arranging)}>
          {arranging ? t('dashboard.done') : t('dashboard.arrange')}
        </Button>
      </div>
      <Masonry items={ordered} editing={arranging} onMove={move} />
    </div>
  );
};

export default Dashboard;
