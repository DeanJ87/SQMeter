import { FunctionalComponent } from 'preact';
import type { SensorData } from '../../types';
import { Card, Icon, MetricTile, Note, Pill, ReadingRow, SensorReadingRow } from '../ui';
import { t } from '../../i18n';
import { formatIlluminance, formatNumber } from '../../i18n/format';
import { deviceText } from '../../i18n/deviceMessage';
import { MiniSpark, StatusDot } from './MiniSpark';
import { bortleTone, conditionTone } from './tones';

type Readings = { sensors: SensorData };

const liveTone = (live: boolean, isStale: boolean) => (live ? 'pill-green' : isStale ? 'pill-amber' : 'pill-dim');
const liveText = (live: boolean, isStale: boolean) =>
  live ? t('dashboard.live') : isStale ? t('dashboard.stale') : t('dashboard.connected');

export const SkyHero: FunctionalComponent<Readings & { live: boolean; isStale: boolean; sqmHistory: number[] }> = ({
  sensors,
  live,
  isStale,
  sqmHistory,
}) => {
  const skyTone = bortleTone(sensors?.sky?.bortle);
  return (
    <section class={`hero-card ${skyTone}`}>
      <div class="hero-topline">
        <div class="card-title flat">
          <Icon name="star" tone="cyan" />
          <h2>{t('dashboard.skyQuality')}</h2>
        </div>
        <div class="hero-pills">
          <Pill tone={liveTone(live, isStale)}>
            <StatusDot ok={live} /> {liveText(live, isStale)}
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
        <MetricTile label={t('dashboard.illuminance')} value={formatIlluminance(sensors.light.lux)} unit="lux" />
      </div>
    </section>
  );
};

export const SkyUnavailableCard: FunctionalComponent = () => (
  <Card
    title={t('dashboard.skyQuality')}
    icon="star"
    tone="muted"
    actions={<Pill tone="pill-red">{t('settings.sensors.notResponding')}</Pill>}
  />
);

export const CloudCard: FunctionalComponent<Readings> = ({ sensors }) => (
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
      <MetricTile label={t('dashboard.cloudCover')} value={formatNumber(sensors?.clouds?.coverPercent, 0)} unit="%" tone="tone-violet" />
      <MetricTile label={t('dashboard.tempDelta')} value={formatNumber(sensors.clouds.temperatureDelta, 1)} unit="°C" />
      <MetricTile label={t('dashboard.corrected')} value={formatNumber(sensors.clouds.correctedDelta, 1)} unit="°C" />
    </div>
    {sensors.clouds.humiditySource === 'assumed' && (
      <Note>{t('dashboard.humidityAssumedNumberNoHumidity', { number: formatNumber(sensors.clouds.humidity, 0) })}</Note>
    )}
  </Card>
);

export const LightCard: FunctionalComponent<Readings> = ({ sensors }) => (
  <Card title={t('dashboard.lightSensor')} icon="eye" tone="cyan">
    <SensorReadingRow label={t('dashboard.illuminance')} value={formatIlluminance(sensors.light.lux)} unit="lux" />
    <SensorReadingRow label={t('dashboard.visible')} value={String(sensors.light.visible)} unit={t('dashboard.rawUnit')} />
    <SensorReadingRow label={t('dashboard.infrared')} value={String(sensors.light.infrared)} unit={t('dashboard.rawUnit')} />
    <SensorReadingRow label={t('dashboard.fullSpectrum')} value={String(sensors.light.full)} unit={t('dashboard.rawUnit')} />
  </Card>
);

export const IrCard: FunctionalComponent<Readings> = ({ sensors }) => (
  <Card title={t('dashboard.irSkySensor')} icon="therm" tone="violet">
    <ReadingRow label={t('dashboard.skyTemperature')} value={`${formatNumber(sensors.infrared.skyTemperature, 1)} °C`} />
    <ReadingRow label={t('dashboard.ambient')} value={`${formatNumber(sensors.infrared.ambientTemperature, 1)} °C`} />
  </Card>
);
