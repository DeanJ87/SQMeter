import { FunctionalComponent } from 'preact';
import { useEffect, useState } from 'preact/hooks';
import { useWebSocket } from '../hooks/useWebSocket';
import { useQuiet } from '../hooks/useQuiet';
import type { Config, SensorData, SystemStatus } from '../types';
import { Button } from './ui';
import { useAnnounceChange } from '../lib/a11y';
import { getJson } from '../lib/api';
import Masonry, { MasonryItem, mergeOrder, moveInOrder } from './Masonry';
import { t } from '../i18n';
import StatusCard from '../dashboard/StatusCard';
import { glanceItems } from '../dashboard/glance';
import { useEffectiveReport } from '../dashboard/useGlanceData';
import { useAlertSchedule } from '../hooks/useAlertSchedule';
import { acknowledgePhoneAlarm } from '../lib/phoneAlarm';
import { StatusDot } from './dashboard/MiniSpark';
import { dashboardCards } from './dashboard/cards';
import { DEFAULT_ORDER, loadOrder, saveOrder } from './dashboard/order';

// The verdict is the one live value announced on its own, once per change
// (spec 022 FR-009); readings are read on demand.
const verdictText = (safe: boolean | undefined, reasons: string[] | undefined) =>
  safe
    ? t('dashboard.observatorySafe')
    : t('dashboard.observatoryUnsafeValue', { value: reasons?.length ? `: ${reasons.join(', ')}` : '' });

// Nothing received yet; once data has arrived, a lost connection keeps the
// last values on screen, greyed, with the Status card saying so (US2-4).
const Waiting: FunctionalComponent<{ connected: boolean }> = ({ connected }) => (
  <div class="empty-state">
    <StatusDot ok={false} />
    <h2>{connected ? t('dashboard.waitingForSensorData') : t('dashboard.connectingToSqmeter')}</h2>
    <p>{connected ? t('dashboard.theDashboardWillPopulateWhen') : t('dashboard.openingTheLiveWebsocketStream')}</p>
  </div>
);

const Toolbar: FunctionalComponent<{ arranging: boolean; setArranging: (on: boolean) => void; reset: () => void }> = ({
  arranging,
  setArranging,
  reset,
}) => (
  <div class="dashboard-toolbar">
    {arranging && (
      <Button small variant="ghost" onClick={reset}>
        {t('dashboard.resetOrder')}
      </Button>
    )}
    <Button small variant={arranging ? 'primary' : 'ghost'} onClick={() => setArranging(!arranging)}>
      {arranging ? t('dashboard.done') : t('dashboard.arrange')}
    </Button>
  </div>
);

// The last 24 real SQM readings: the trend draws once there are two.
const useSqmHistory = (sqm: number | undefined) => {
  const [sqmHistory, setSqmHistory] = useState<number[]>([]);
  useEffect(() => {
    if (typeof sqm !== 'number' || !Number.isFinite(sqm)) return;
    setSqmHistory((history) => [...history.slice(-23), sqm]);
  }, [sqm]);
  return sqmHistory;
};

const Dashboard: FunctionalComponent = () => {
  const [savedOrder, setSavedOrder] = useState<string[]>(loadOrder);
  const [arranging, setArranging] = useState(false);
  const { data: sensors, connected, lastMessageAt } = useWebSocket<SensorData>('/ws/sensors');
  const quiet = useQuiet(lastMessageAt);
  const { data: status } = useWebSocket<SystemStatus>('/ws/status');
  const [config, setConfig] = useState<Config | null>(null);
  const configRevision = status?.configRevision;
  const effective = useEffectiveReport(configRevision);
  const { schedule, pauseOrResume } = useAlertSchedule(status?.alerts);

  // Read again whenever the device's settings change, so a new location
  // reaches Sun & Moon without a reload. A failed read keeps what's shown.
  useEffect(() => {
    getJson<Config>('/api/config')
      .then((data) => data && setConfig(data))
      .catch(() => undefined);
  }, [configRevision]);

  const sqmHistory = useSqmHistory(sensors?.sky?.sqm);

  useAnnounceChange(sensors?.safety?.safe, (safe) => verdictText(safe, sensors?.safety?.reasons));

  const items = glanceItems({ sensors, status, config, effective, connected, quiet, schedule });
  const onAction = (action: 'resume' | 'acknowledge') => void (action === 'resume' ? pauseOrResume(true) : acknowledgePhoneAlarm());

  if (!sensors) return <Waiting connected={connected} />;

  const statusCard: MasonryItem = { id: 'status', title: t('status.title'), node: <StatusCard items={items} onAction={onAction} /> };
  const visible = [statusCard, ...dashboardCards({ sensors, status, config, quiet, connected, sqmHistory })];
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
  const reset = () => {
    setSavedOrder([]);
    saveOrder([]);
  };

  return (
    <div class={`dashboard page-enter${connected ? '' : ' is-disconnected'}`}>
      <Toolbar arranging={arranging} setArranging={setArranging} reset={reset} />
      <Masonry items={ordered} editing={arranging} onMove={move} />
    </div>
  );
};

export default Dashboard;
