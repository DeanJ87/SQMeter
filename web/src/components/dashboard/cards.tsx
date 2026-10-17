import type { Config, SensorData, SensorHealth, SystemStatus } from '../../types';
import type { MasonryItem } from '../Masonry';
import SafetyCard from '../SafetyCard';
import SunMoonCard from '../SunMoonCard';
import DeviceCard from '../../dashboard/DeviceCard';
import FaultCard from '../../dashboard/FaultCard';
import { expectedSensors, type Sensor } from '../../dashboard/glance';
import { authoritativeMissing, otherReading, otherSkyFault, skyChoice } from '../../dashboard/skySensors';
import { skySensorName } from '../../lib/sensorNames';
import { deviceTime } from '../../lib/deviceTime';
import { t } from '../../i18n';
import { CloudCard, IrCard, LightCard, SkyHero, SkyUnavailableCard, type Freshness } from './skyCards';
import { EnvironmentCard, GpsCard, RainCard, WindCard } from './weatherCards';
import { BatteryCard, BatteryStatePill } from './batteryCard';

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
  return f && <FaultCard title={title} icon={icon} health={f.health} />;
};

// A GPS fix wins over the location typed into Settings.
const locationOf = ({ sensors, config }: CardContext) => {
  if (sensors?.gps?.fix && sensors.gps.latitude !== undefined && sensors.gps.longitude !== undefined) {
    return { latitude: sensors.gps.latitude, longitude: sensors.gps.longitude };
  }
  return config?.location?.set ? config.location : null;
};

// With the SQM-L in play a missing authoritative sensor is named, with a
// switch to the other one (spec 030 FR-010); otherwise as before.
const skyUnavailable = (ctx: CardContext, fault: Fault) => {
  const missing = authoritativeMissing(ctx.sensors, ctx.config);
  if (missing) return <SkyUnavailableCard missing={missing} />;
  return faultCard(fault, 'light', t('dashboard.skyQuality'), 'star') ?? <SkyUnavailableCard />;
};

const skyCard = (ctx: CardContext, fault: Fault): Entry => {
  const { sensors, config } = ctx;
  // Stale: the device says its data is old, or the stream has gone quiet.
  const freshness: Freshness = !ctx.connected ? 'offline' : ctx.quiet ? 'quiet' : sensors?.dataStale ? 'stale' : 'live';
  const choice = skyChoice(config);
  return {
    id: 'sky',
    title: t('dashboard.skyQuality'),
    node:
      sensors?.light?.status !== 'ok' ? (
        skyUnavailable(ctx, fault)
      ) : (
        <SkyHero
          sensors={sensors}
          freshness={freshness}
          sqmHistory={ctx.sqmHistory}
          name={choice.both ? (sensors.sky.source ?? choice.authoritative) : undefined}
          other={otherReading(sensors, config)}
        />
      ),
  };
};

// The other sky sensor, once it has failed, keeps a card titled with its
// name (spec 025 D6); while it reports, its reading is on the Sky card.
const otherSkyCard = ({ sensors, status, config }: CardContext): Entry => {
  const failed = otherSkyFault(sensors, status, config);
  return (
    failed && {
      id: 'sky-other',
      title: skySensorName(failed.role),
      node: <FaultCard title={skySensorName(failed.role)} icon="star" health={failed.health} />,
    }
  );
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
      title: t('dashboard.irSkySensor'),
      node: faultCard(fault, 'infrared', t('dashboard.irSkySensor'), 'therm') || <IrCard sensors={sensors} />,
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

// Only on a board with a fuel gauge (spec 032). A gauge that stops answering
// keeps its card in the fault state, with the charger's state when it is wired.
const batteryCard = ({ sensors }: CardContext): Entry => {
  const battery = sensors?.battery;
  if (!battery?.present) return null;
  const title = t('dashboard.battery');
  const node =
    battery.status && battery.status !== 'ok' ? (
      <FaultCard title={title} icon="battery" health={battery.status} extra={<BatteryStatePill state={battery.state} />} />
    ) : (
      <BatteryCard battery={battery} />
    );
  return { id: 'battery', title, node };
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
    otherSkyCard(ctx),
    sunMoonCard(ctx),
    cloud,
    environmentCard(ctx, fault),
    gpsCard(ctx, fault),
    lightCard(ctx, fault),
    { id: 'device', title: t('dashboard.deviceNetwork'), node: <DeviceCard status={ctx.status} /> },
    ir,
    windCard(ctx, fault),
    rainCard(ctx, fault),
    batteryCard(ctx),
  ];
  return cards.filter((card): card is MasonryItem => Boolean(card));
};
