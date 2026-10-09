import { FunctionalComponent } from 'preact';
import type { Config, SensorData } from '../../types';
import { Card, MetricTile, Pill } from '../ui';
import { t } from '../../i18n';
import { formatAgeMs, formatCoordinates, formatNumber } from '../../i18n/format';
import { compassPoint, windTone } from './tones';

type Gps = NonNullable<SensorData['gps']>;
type Wind = NonNullable<SensorData['wind']>;
type Rain = NonNullable<SensorData['rain']>;

export const EnvironmentCard: FunctionalComponent<{ environment: SensorData['environment'] }> = ({ environment }) => (
  <Card title={t('dashboard.environment')} icon="therm" tone="amber">
    <div class="tile-grid two">
      <MetricTile label={t('dashboard.temperature')} value={formatNumber(environment.temperature, 1)} unit="°C" tone="tone-amber" />
      <MetricTile
        label={t('dashboard.humidity')}
        value={formatNumber(environment.humidity, 1)}
        unit="%"
        tone={(environment.humidity ?? 0) > 80 ? 'tone-amber' : 'tone-cyan'}
      />
      <MetricTile label={t('dashboard.pressure')} value={formatNumber(environment.pressure, 1)} unit="hPa" />
      <MetricTile label={t('dashboard.dewPoint')} value={formatNumber(environment.dewpoint, 1)} unit="°C" tone="tone-violet" />
    </div>
  </Card>
);

export const GpsCard: FunctionalComponent<{ gps: Gps }> = ({ gps }) => (
  <Card
    title={t('dashboard.gpsLocation')}
    icon="gps"
    tone={gps.fix ? 'green' : 'muted'}
    actions={
      <>
        <Pill tone={gps.fix ? 'pill-green' : 'pill-dim'}>{gps.fix ? t('dashboard.lockAcquired') : t('dashboard.noFix')}</Pill>
      </>
    }
  >
    <div class="coordinate-line mono">{gps.fix ? formatCoordinates(gps.latitude ?? 0, gps.longitude ?? 0, 6) : '--'}</div>
    <div class="tile-grid gps-metrics">
      <MetricTile label={t('dashboard.satellites')} value={String(gps.satellites ?? 0)} tone="tone-green" />
      <MetricTile label={t('dashboard.altitude')} value={gps.fix ? formatNumber(gps.altitude, 0) : '--'} unit="m" />
      <MetricTile label="HDOP" value={gps.fix ? formatNumber(gps.hdop, 1) : '--'} />
      <MetricTile label={t('dashboard.fixAge')} value={formatAgeMs(gps.ageMs)} />
    </div>
  </Card>
);

const directionUnit = (wind: Wind) => {
  if (wind.direction !== undefined) return `${formatNumber(wind.direction, 0)}°`;
  return wind.vaneFault ? t('dashboard.vaneFault') : t('dashboard.calm');
};

export const WindCard: FunctionalComponent<{ wind: Wind; config: Config | null }> = ({ wind, config }) => {
  const alpaca = config?.alpaca;
  return (
    <Card
      title={t('dashboard.wind')}
      icon="cloud"
      tone="cyan"
      actions={
        <Pill tone={wind.status === 'ok' ? 'pill-green' : 'pill-red'}>
          {wind.status === 'ok' ? t('dashboard.online') : t('dashboard.offline')}
        </Pill>
      }
    >
      <div class="metric-grid">
        <MetricTile
          label={t('dashboard.speed')}
          value={formatNumber(wind.speed, 1)}
          unit="m/s"
          alt={t('dashboard.numberKmH', { number: formatNumber((wind.speed ?? 0) * 3.6, 0) })}
          tone={windTone(wind.speed, alpaca?.windSpeedUnsafeEnabled, alpaca?.windSpeedUnsafeMs, 'tone-cyan')}
        />
        <MetricTile
          label={t('dashboard.gust')}
          value={formatNumber(wind.gust, 1)}
          unit="m/s"
          alt={t('dashboard.numberKmH', { number: formatNumber((wind.gust ?? 0) * 3.6, 0) })}
          tone={windTone(wind.gust, alpaca?.windGustUnsafeEnabled, alpaca?.windGustUnsafeMs)}
        />
        <MetricTile
          label={t('dashboard.direction')}
          value={wind.direction !== undefined ? compassPoint(wind.direction) : '--'}
          unit={directionUnit(wind)}
        />
      </div>
    </Card>
  );
};

const rainStatusOf = (rain: Rain) => {
  if (rain.status === 'ok') return { text: t('dashboard.online'), tone: 'pill-green' };
  if (rain.status === 'stale') return { text: t('dashboard.stale'), tone: 'pill-amber' };
  return { text: t('dashboard.offline'), tone: 'pill-red' };
};

const rainedText = (rain: Rain) => (rain.raining ? t('dashboard.yes') : rain.status === 'ok' ? t('dashboard.no') : '--');

export const RainCard: FunctionalComponent<{ rain: Rain; config: Config | null }> = ({ rain, config }) => {
  // The device reports rain in mm; show it in the units the RG-15 is set to.
  const imperialRain = config?.rain?.units === 'imperial';
  const rainUnits = imperialRain ? { depth: 'in', intensity: 'in/hr' } : { depth: 'mm', intensity: 'mm/hr' };
  const rainValue = (mm: number | undefined) => (typeof mm === 'number' && imperialRain ? mm / 25.4 : mm);
  const rainStatus = rainStatusOf(rain);
  return (
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
        <MetricTile label={t('dashboard.raining')} value={rainedText(rain)} tone={rain.raining ? 'tone-amber' : 'tone-green'} />
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
  );
};
