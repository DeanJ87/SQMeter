import { FunctionalComponent } from 'preact';
import type { SettingsTabProps } from './context';
import { CloudDetectionCard, SkyQualityCards, SkySensorsCard } from './SensorsSkyCards';
import { RainCard, WindCard } from './SensorsRainWindCards';

const SensorsTab: FunctionalComponent<SettingsTabProps> = (props) => (
  <>
    <SkySensorsCard {...props} />
    <SkyQualityCards {...props} />
    <CloudDetectionCard {...props} />
    <RainCard {...props} />
    <WindCard {...props} />
  </>
);

export default SensorsTab;
