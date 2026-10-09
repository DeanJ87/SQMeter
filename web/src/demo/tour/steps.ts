import { demoDevice } from '../device';

// The demo tour's steps (specs/018 FR-001). Action steps finish when the
// emulated device reacts (FR-002), never on a timer, and offer "Do it for me".

export interface TourStep {
  id: string;
  title: string;
  body: () => string;
  /** CSS selector of the control to outline; none = a centred card. */
  target?: string;
  /** Page the step is about (opened when the step starts). */
  route?: string;
  /** Action steps: true once the device has reacted. */
  done?: () => boolean;
  doIt?: () => void;
  doItLabel?: string;
}

interface Safety {
  safe: boolean;
  reasons: string[];
}

const safety = (): Safety => JSON.parse(demoDevice.safety());
const rainOn = () => demoDevice.rawConfig().rain?.enabled === true;
const hasAlerts = () => (JSON.parse(demoDevice.recentAlerts()).alerts as unknown[]).length > 0;

// Settings the "change a rule" step compares against, captured when it starts.
let rulesAtStart = '';
const rules = () => JSON.stringify(demoDevice.rawConfig().alpaca ?? {});
export const captureRules = () => {
  rulesAtStart = rules();
};

export const TOUR_STEPS: TourStep[] = [
  {
    id: 'welcome',
    title: 'This is SQMeter',
    body: () =>
      "A sky-quality and weather monitor for observatories: it measures how dark and clear the sky is, decides whether it's safe to open, and tells you and your imaging software. This demo runs the device's own code in your browser.",
  },
  {
    id: 'readings',
    title: 'Live readings',
    route: '/',
    target: '[data-masonry-id="sky"]',
    body: () => 'Sky quality in mag/arcsec², the Bortle class and naked-eye limit - from a light sensor pointed at the zenith.',
  },
  {
    id: 'verdict',
    title: 'The safety verdict',
    route: '/',
    target: '[data-masonry-id="safety"]',
    body: () => 'Safe or unsafe, with the reasons. Your rules decide: rain, cloud cover, darkness, wind and sensors that stop answering.',
  },
  {
    id: 'make-unsafe',
    title: 'Make it unsafe',
    route: '/',
    target: '[data-masonry-id="safety"]',
    body: () =>
      rainOn()
        ? 'Open Demo (bottom right) and press Rain under Shortcuts - or let me. Watch this card.'
        : 'The rain sensor is off in this demo, so try cloud instead: open Demo and press "Cloud just unsafe" - or let me.',
    done: () => !safety().safe,
    doIt: () => void demoDevice.applyShortcut(rainOn() ? 'rain' : 'cloudUnsafe'),
    doItLabel: 'Do it for me',
  },
  {
    id: 'bell',
    title: 'An alert',
    route: '/',
    target: '.alerts-bell',
    body: () =>
      `${hasAlerts() ? 'The device raised an alert - open the bell to read it.' : "The alert lands under the bell. For its first minute after starting, the device doesn't alert, so a restart doesn't announce the start-up weather - give it a moment."} A real SQMeter sends alerts to your phone (Pushover, ntfy, MQTT); turn on Real notifications in the Demo panel to try that here.`,
  },
  {
    id: 'alpaca',
    title: 'Your imaging software agrees',
    route: '/alpaca',
    body: () =>
      'N.I.N.A. and other ASCOM Alpaca apps ask the device "is it safe?" - and get the same verdict, with the same reasons. Here is what they see.',
  },
  {
    id: 'change-rule',
    title: 'Change a rule',
    route: '/settings?tab=safety',
    body: () =>
      'Change any safety rule and save - for example switch off the Rain rule. The verdict re-evaluates with your rule straight away.',
    done: () => rulesAtStart !== '' && rules() !== rulesAtStart,
    doIt: () => void demoDevice.applyConfig(JSON.stringify({ alpaca: { rainUnsafeEnabled: false } })),
    doItLabel: 'Switch off the rain rule for me',
  },
  {
    id: 'demo-panel',
    title: 'The Demo panel',
    target: '.demo-panel-toggle',
    body: () =>
      'Set what every sensor reports, jump to tonight or dawn, move the device anywhere on Earth, connect a pretend imaging app - and take this tour again.',
  },
  {
    id: 'next',
    title: 'Where next',
    body: () => 'Read the docs at sqmeter.dev, see the parts list to build one, or look at the code on GitHub.',
  },
];

export const LINKS = [
  { label: 'Docs', href: 'https://sqmeter.dev/' },
  { label: 'Build one', href: 'https://sqmeter.dev/getting-started/hardware/' },
  { label: 'GitHub', href: 'https://github.com/DeanJ87/SQMeter' },
];
