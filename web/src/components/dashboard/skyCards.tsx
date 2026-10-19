import { FunctionalComponent } from 'preact';
import { useState } from 'preact/hooks';
import type { SensorData, SkyRole } from '../../types';
import { Card, Icon, MetricTile, Note, Pill, ReadingRow, SensorReadingRow } from '../ui';
import { t } from '../../i18n';
import { formatIlluminance, formatNumber } from '../../i18n/format';
import { deviceError, deviceText } from '../../i18n/deviceMessage';
import { skySensorName } from '../../lib/sensorNames';
import { bodyOf, postJson } from '../../lib/api';
import type { OtherReading, SkyMissing } from '../../dashboard/skySensors';
import { healthWords } from '../../dashboard/glance';
import { MiniSpark, StatusDot } from './MiniSpark';
import { bortleTone, conditionTone } from './tones';

type Readings = { sensors: SensorData };

// Data freshness lives here, on the Sky Quality card's pill (DS-08): the
// Status card doesn't repeat it.
export type Freshness = 'live' | 'stale' | 'quiet' | 'offline';
const LIVE_TONE: Record<Freshness, string> = { live: 'pill-green', stale: 'pill-amber', quiet: 'pill-amber', offline: 'pill-red' };
const liveText = (state: Freshness) =>
  ({ live: t('dashboard.live'), stale: t('dashboard.stale'), quiet: t('status.noUpdates'), offline: t('status.offline') })[state];

// The difference is SQM-L - SQM, signed: the narrow lens usually reads darker.
const signed = (value: number) => `${value > 0 ? '+' : ''}${formatNumber(value, 2)}`;

// With both sky sensors on (spec 030): `name` is the authoritative sensor's
// name pill; `other` the other reading and the difference, when shown.
export const SkyHero: FunctionalComponent<
  Readings & { freshness: Freshness; sqmHistory: number[]; name?: SkyRole; other?: OtherReading | null }
> = ({ sensors, freshness, sqmHistory, name, other }) => {
  const skyTone = bortleTone(sensors?.sky?.bortle);
  return (
    <section class={`hero-card ${skyTone}`}>
      <div class="hero-topline">
        <div class="card-title flat">
          <Icon name="star" tone="cyan" />
          <h2>{t('dashboard.skyQuality')}</h2>
        </div>
        <div class="hero-pills">
          {name && <Pill>{skySensorName(name)}</Pill>}
          <span data-inventory="freshness">
            <Pill tone={LIVE_TONE[freshness]}>
              <StatusDot ok={freshness === 'live'} /> {liveText(freshness)}
            </Pill>
          </span>
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
        {other && (
          <MetricTile label={skySensorName(other.role)} value={formatNumber(other.sqm, 2)} unit={t('dashboard.magArcsec')} tone={skyTone} />
        )}
        {other?.difference !== undefined && (
          <MetricTile
            label={t('dashboard.skyDifference')}
            value={signed(other.difference)}
            unit={t('dashboard.magArcsec')}
            tone={other.warn ? 'tone-amber' : ''}
          />
        )}
      </div>
    </section>
  );
};

// Saves the other sensor as the authoritative one (FR-010): the device
// never switches on its own.
const useSwitchSky = () => {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);
  const switchTo = async (role: SkyRole) => {
    setBusy(true);
    setError(null);
    try {
      const response = await postJson('/api/config', { sky: { authoritative: role } });
      if (!response.ok) setError(deviceError(await bodyOf(response), t('dashboard.couldNotSwitch')));
    } catch {
      setError(t('settings.couldNotReachTheDevice'));
    } finally {
      setBusy(false);
    }
  };
  return { busy, error, switchTo };
};

const missingText = (missing: SkyMissing) => {
  const name = skySensorName(missing.role);
  return missing.health === 'off' ? t('dashboard.skySensorOff', { name }) : t('dashboard.skySensorNotResponding', { name });
};

// No sky reading. With the SQM-L in play, `missing` names the authoritative
// sensor and offers the other one (spec 030 FR-010).
export const SkyUnavailableCard: FunctionalComponent<{ missing?: SkyMissing | null }> = ({ missing }) => {
  const { busy, error, switchTo } = useSwitchSky();
  const target = missing?.switchTo;
  const pill = missing && missing.health !== 'off' ? healthWords(missing.health) : missing ? t('status.off') : null;
  return (
    <Card
      title={t('dashboard.skyQuality')}
      icon="star"
      tone="muted"
      actions={<Pill tone="pill-red">{pill ?? t('settings.sensors.notResponding')}</Pill>}
    >
      {missing && (
        <Note
          action={
            target && !busy
              ? { label: t('dashboard.useSkySensor', { name: skySensorName(target) }), onClick: () => void switchTo(target) }
              : undefined
          }
        >
          {missingText(missing)}
        </Note>
      )}
      {error && <Note tone="bad">{error}</Note>}
    </Card>
  );
};

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
