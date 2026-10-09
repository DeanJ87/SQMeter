import { FunctionalComponent } from 'preact';
import type { SensorHealth } from '../types';
import { Card, Note, Pill } from '../components/ui';
import { formatAgo } from '../i18n/format';
import { t } from '../i18n';
import { healthWords } from './glance';

// A sensor that is switched on but not working keeps its card (specs/025
// FR-013): what is wrong, how old its last good reading is, and what it
// affects - never a card that vanishes as if the sensor wasn't fitted.

const FaultCard: FunctionalComponent<{ title: string; icon: string; health: SensorHealth; ageMs?: number; effect: string }> = ({
  title,
  icon,
  health,
  ageMs,
  effect,
}) => (
  <div data-inventory="sensor-faults">
    <Card title={title} icon={icon} tone="muted" actions={<Pill tone="pill-red">{healthWords(health)}</Pill>}>
      <Note tone="bad">
        {effect}
        {ageMs ? ` ${t('glance.lastReading', { ago: formatAgo(ageMs) })}` : ''}
      </Note>
    </Card>
  </div>
);

export default FaultCard;
