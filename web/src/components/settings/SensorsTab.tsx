import { FunctionalComponent } from 'preact';
import type { SettingsTabProps } from './context';
import { CloudDetectionCard, SkyQualityCard, SkySensorsCard } from './SensorsSkyCards';
import { RainCard, WindCard } from './SensorsRainWindCards';

const SensorsTab: FunctionalComponent<SettingsTabProps> = (props) => (
  <>
    <SkySensorsCard {...props} />
    <SkyQualityCard {...props} />
    <CloudDetectionCard {...props} />
    <RainCard {...props} />
    <WindCard {...props} />
  </>
);

export default SensorsTab;
