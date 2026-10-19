import { ComponentChildren, FunctionalComponent } from 'preact';
import type { SensorHealth } from '../types';
import { Card, Pill } from '../components/ui';
import { healthWords } from './glance';

// A sensor that is switched on but not working keeps its card (specs/025
// FR-013) with its state in the pill - never a card that vanishes as if the
// sensor wasn't fitted. What it affects and how old its last reading is are
// said once, in the Status card's row for the sensor (specs/026 DS-08).

// `extra`: a second pill that still holds while the sensor is down (the charger's state, spec 032).
const FaultCard: FunctionalComponent<{ title: string; icon: string; health: SensorHealth; extra?: ComponentChildren }> = ({
  title,
  icon,
  health,
  extra,
}) => (
  <div data-inventory="sensor-faults">
    <Card
      title={title}
      icon={icon}
      tone="muted"
      actions={
        <>
          {extra}
          <Pill tone="pill-red">{healthWords(health)}</Pill>
        </>
      }
    />
  </div>
);

export default FaultCard;
