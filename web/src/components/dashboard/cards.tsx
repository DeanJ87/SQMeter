import type { Config, SensorData, SensorHealth, SystemStatus } from '../../types';
import type { MasonryItem } from '../Masonry';
import SafetyCard from '../SafetyCard';
import SunMoonCard from '../SunMoonCard';
import DeviceCard from '../../dashboard/DeviceCard';
import FaultCard from '../../dashboard/FaultCard';
import { expectedSensors, sensorEffect, type Sensor } from '../../dashboard/glance';
import { deviceTime } from '../../lib/deviceTime';
import { t } from '../../i18n';
import { CloudCard, IrCard, LightCard, SkyHero, SkyUnavailableCard } from './skyCards';
import { EnvironmentCard, GpsCard, RainCard, WindCard } from './weatherCards';

// What the dashboard's cards are drawn from, once readings have arrived.
export interface CardContext {
  sensors: SensorData;
  status: SystemStatus | null;
  config: Config | null;
  quiet: boolean;
  connected: boolean;
  sqmHistory: number[];
}

type Entry = MasonryItem | false | null | undefined;

// An expected sensor that isn't ok keeps its card, in a fault state (specs/025 FR-013).
const faultOf = ({ sensors, status, config }: CardContext) => {
  const expected = expectedSensors(status, config);
  return (sensor: Sensor) => {
    if (!expected.includes(sensor)) return null;
    const entry = status?.sensors[sensor] ?? (sensors?.[sensor] as { status?: SensorHealth; ageMs?: number } | undefined);
    return entry?.status && entry.status !== 'ok' ? { health: entry.status, ageMs: entry.ageMs } : null;
  };
};

type Fault = ReturnType<typeof faultOf>;

const faultCard = (fault: Fault, sensor: Sensor, title: string, icon: string) => {
  const f = fault(sensor);
  return f && <FaultCard title={title} icon={icon} health={f.health} ageMs={f.ageMs} effect={sensorEffect(sensor)} />;
};

// A GPS fix wins over the location typed into Settings.
const locationOf = ({ sensors, config }: CardContext) => {
  if (sensors?.gps?.fix && sensors.gps.latitude !== undefined && sensors.gps.longitude !== undefined) {
    return { latitude: sensors.gps.latitude, longitude: sensors.gps.longitude };
  }
  return config?.location?.set ? config.location : null;
};

const skyCard = (ctx: CardContext, fault: Fault): Entry => {
  const { sensors } = ctx;
  // Stale: the device says its data is old, or the stream has gone quiet.
  const isStale = Boolean(sensors?.dataStale) || ctx.quiet;
  const live = ctx.connected && Boolean(sensors) && !isStale;
  return {
    id: 'sky',
    title: t('dashboard.skyQuality'),
    node:
      sensors?.light?.status !== 'ok' ? (
        (faultCard(fault, 'light', t('dashboard.skyQuality'), 'star') ?? <SkyUnavailableCard />)
      ) : (
        <SkyHero sensors={sensors} live={live} isStale={isStale} sqmHistory={ctx.sqmHistory} />
      ),
  };
};

const sunMoonCard = (ctx: CardContext): Entry => {
  const location = locationOf(ctx);
  const showSunMoon = location !== null && ctx.config?.location?.showSunMoon !== false;
  return (
    showSunMoon &&
    location && {
      id: 'sunmoon',
      title: t('dashboard.sunMoon'),
      node: <SunMoonCard latitude={location.latitude} longitude={location.longitude} deviceNow={deviceTime(ctx.status)} />,
    }
  );
};

const infraredCards = ({ sensors }: CardContext, fault: Fault): Entry[] => {
  const shown = sensors?.infrared?.status === 'ok' || fault('infrared');
  return [
    shown && {
      id: 'cloud',
      title: t('dashboard.cloudConditions'),
      node: faultCard(fault, 'infrared', t('dashboard.cloudConditions'), 'cloud') || <CloudCard sensors={sensors} />,
    },
    shown && {
      id: 'ir',
      title: t('dashboard.irTemperature'),
      node: faultCard(fault, 'infrared', t('dashboard.irTemperature'), 'therm') || <IrCard sensors={sensors} />,
    },
  ];
};

const environmentCard = ({ sensors }: CardContext, fault: Fault): Entry =>
  (sensors.environment.status === 'ok' || fault('environment')) && {
    id: 'environment',
    title: t('dashboard.environment'),
    node: faultCard(fault, 'environment', t('dashboard.environment'), 'therm') || <EnvironmentCard environment={sensors.environment} />,
  };

const gpsCard = ({ sensors }: CardContext, fault: Fault): Entry =>
  (sensors.gps || fault('gps')) && {
    id: 'gps',
    title: 'GPS',
    node: faultCard(fault, 'gps', t('dashboard.gpsLocation'), 'gps') || (sensors.gps && <GpsCard gps={sensors.gps} />),
  };

const lightCard = ({ sensors }: CardContext, fault: Fault): Entry =>
  (sensors?.light?.status === 'ok' || fault('light')) && {
    id: 'light',
    title: t('dashboard.lightSensor'),
    node: faultCard(fault, 'light', t('dashboard.lightSensor'), 'eye') || <LightCard sensors={sensors} />,
  };

const windCard = ({ sensors, config }: CardContext, fault: Fault): Entry =>
  (sensors.wind || fault('wind')) && {
    id: 'wind',
    title: t('dashboard.wind'),
    node: faultCard(fault, 'wind', t('dashboard.wind'), 'cloud') || (sensors.wind && <WindCard wind={sensors.wind} config={config} />),
  };

const rainCard = ({ sensors, config }: CardContext, fault: Fault): Entry => {
  const rain = sensors?.rain;
  return (
    (rain || fault('rain')) && {
      id: 'rain',
      title: t('dashboard.rainSensor'),
      node: faultCard(fault, 'rain', t('dashboard.rainSensor'), 'rain') || (rain && <RainCard rain={rain} config={config} />),
    }
  );
};

// Every card the readings allow, in no particular order (the order is the user's).
export const dashboardCards = (ctx: CardContext): MasonryItem[] => {
  const fault = faultOf(ctx);
  const [cloud, ir] = infraredCards(ctx, fault);
  const cards: Entry[] = [
    {
      id: 'safety',
      title: t('dashboard.safety'),
      node: <SafetyCard safety={ctx.sensors.safety} rainClearInSeconds={ctx.sensors?.rain?.clearInSeconds} />,
    },
    skyCard(ctx, fault),
    sunMoonCard(ctx),
    cloud,
    environmentCard(ctx, fault),
    gpsCard(ctx, fault),
    lightCard(ctx, fault),
    { id: 'device', title: t('dashboard.deviceNetwork'), node: <DeviceCard status={ctx.status} /> },
    ir,
    windCard(ctx, fault),
    rainCard(ctx, fault),
  ];
  return cards.filter((card): card is MasonryItem => Boolean(card));
};
