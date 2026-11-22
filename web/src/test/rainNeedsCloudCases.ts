import { mockConfig } from '../mocks/data';
import type { Case } from './settingsDependencyCases';

// D-52, Rain needs cloud (specs/036-rain-needs-cloud): needs the rain sensor and the IR sky sensor.
export const RAIN_NEEDS_CLOUD_CASES: Case[] = [
  {
    id: 'D-52',
    tab: 'safety',
    config: { rain: { ...mockConfig.rain, enabled: false }, alpaca: { ...mockConfig.alpaca, rainNeedsCloud: true } },
    note: 'Not in effect: Rain sensor is off',
    control: 'Rain needs cloud',
    control_state: 'unlocked',
    fixLabel: 'Turn on',
    fixTab: 'Sensors',
  },
  {
    id: 'D-52',
    tab: 'safety',
    config: { rain: { ...mockConfig.rain, enabled: true }, alpaca: { ...mockConfig.alpaca, rainNeedsCloud: true } },
    facts: { infraredDetected: false },
    note: 'Not in effect: IR sky sensor not detected',
    control: 'Rain needs cloud',
    control_state: 'unlocked',
  },
  {
    id: 'D-52',
    tab: 'safety',
    config: { rain: { ...mockConfig.rain, enabled: true }, alpaca: { ...mockConfig.alpaca, rainNeedsCloud: false } },
    facts: { infraredDetected: false },
    note: 'IR sky sensor not detected.',
    control: 'Rain needs cloud',
    control_state: 'locked',
  },
];
