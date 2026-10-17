import { FunctionalComponent } from 'preact';
import type { SystemStatus } from '../types';
import { Pill, ReadingRow } from './ui';
import { t } from '../i18n';
import { formatNumber } from '../i18n/format';

// System -> Sensors: the fuel gauge and charger (spec 032). Only on a board
// with a gauge or with the charger pins set; the Battery card on the
// dashboard holds the charge itself.

const GAUGE_MODEL = 'MAX17048';
const level = (high: boolean) => (high ? 'H' : 'L');

const gaugeBadge = (status: SystemStatus) => {
  const diagnostics = status.diagnostics?.battery;
  const health = status.sensors.battery?.status;
  if (diagnostics && (!health || health === 'ok'))
    return { text: t('system.batteryDetected', { version: diagnostics.version }), tone: 'pill-green' };
  if (health === 'stale') return { text: t('system.stale'), tone: 'pill-amber' };
  if (health === 'error') return { text: t('system.error'), tone: 'pill-red' };
  return { text: t('system.notDetected'), tone: 'pill-red' };
};

const BatteryRows: FunctionalComponent<{ status: SystemStatus }> = ({ status }) => {
  if (!status.battery?.present) return null;
  const badge = gaugeBadge(status);
  const charger = status.diagnostics?.battery?.charger;
  const voltage = status.battery.voltage;
  return (
    <>
      <div class="reading-row">
        <span class="reading-label">
          {t('system.batteryGauge')} <span class="system-subtle mono">{GAUGE_MODEL}</span>
        </span>
        <Pill tone={badge.tone}>{badge.text}</Pill>
      </div>
      {typeof voltage === 'number' && <ReadingRow label={t('system.batteryVoltage')} value={`${formatNumber(voltage, 2)} V`} />}
      {charger && (
        <ReadingRow
          label={t('system.chargerPins')}
          value={`STAT1 ${level(charger.stat1)} · STAT2 ${level(charger.stat2)} · PG ${level(charger.pg)}`}
          valueClass="mono"
        />
      )}
    </>
  );
};

export default BatteryRows;
