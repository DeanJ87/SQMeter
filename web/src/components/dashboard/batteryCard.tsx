import { FunctionalComponent } from 'preact';
import type { BatteryReading, ChargeState } from '../../types';
import { Card, InfoTip, MetricTile, Pill } from '../ui';
import { t } from '../../i18n';
import { formatMinutes, formatNumber } from '../../i18n/format';

// The Li-ion pack (spec 032, contracts/ui.md): only on a board with a fuel gauge.

// The device caps its estimates at 99 h (5,940 min).
const ESTIMATE_CAP_MINUTES = 5940;

const STATE: Record<ChargeState, { text: () => string; tone: string }> = {
  discharging: { text: () => t('dashboard.batteryDischarging'), tone: 'pill-dim' },
  charging: { text: () => t('dashboard.batteryCharging'), tone: 'pill-cyan' },
  charged: { text: () => t('dashboard.batteryCharged'), tone: 'pill-green' },
  fault: { text: () => t('dashboard.batteryFault'), tone: 'pill-red' },
  'no-battery': { text: () => t('dashboard.batteryNoBattery'), tone: 'pill-dim' },
};

export const formatEstimate = (minutes: number | null | undefined) => {
  if (typeof minutes !== 'number' || !Number.isFinite(minutes)) return '--';
  return minutes >= ESTIMATE_CAP_MINUTES ? t('dashboard.batteryOver99h') : formatMinutes(minutes);
};

/** The charger's state as a pill (with why charging stopped behind "?"), or nothing without one. */
export const BatteryStatePill: FunctionalComponent<{ state?: ChargeState | null }> = ({ state }) => {
  const view = state ? STATE[state] : undefined;
  if (!view) return null;
  return (
    <>
      <Pill tone={view.tone}>{view.text()}</Pill>
      {state === 'fault' && <InfoTip text={t('dashboard.batteryFaultHint')} />}
    </>
  );
};

// Time left while discharging, time to full while charging; none otherwise.
const timeTile = (battery: BatteryReading) => {
  if (battery.state === 'discharging') return { label: t('dashboard.batteryTimeLeft'), minutes: battery.timeToEmptyMinutes };
  if (battery.state === 'charging') return { label: t('dashboard.batteryTimeToFull'), minutes: battery.timeToFullMinutes };
  return null;
};

const Percent: FunctionalComponent<{ battery: BatteryReading }> = ({ battery }) => {
  if (battery.state === 'no-battery') return null;
  if (battery.estimating || typeof battery.percent !== 'number')
    return <div class="coordinate-line">{t('dashboard.batteryEstimating')}</div>;
  return (
    <div class="sqm-display">
      <div class="sqm-value">{formatNumber(battery.percent, 0)}</div>
      <div class="sqm-unit">%</div>
    </div>
  );
};

export const BatteryCard: FunctionalComponent<{ battery: BatteryReading }> = ({ battery }) => {
  const time = timeTile(battery);
  return (
    <Card
      title={t('dashboard.battery')}
      icon="battery"
      tone="green"
      actions={
        <>
          {battery.powerSaving && <Pill tone="pill-amber">{t('dashboard.batteryPowerSaving')}</Pill>}
          <BatteryStatePill state={battery.state} />
        </>
      }
    >
      <Percent battery={battery} />
      <div class="tile-grid two">
        {typeof battery.voltage === 'number' && (
          <MetricTile label={t('dashboard.batteryVoltage')} value={formatNumber(battery.voltage, 2)} unit="V" />
        )}
        {time && <MetricTile label={time.label} value={battery.estimating ? '--' : formatEstimate(time.minutes)} />}
      </div>
    </Card>
  );
};
