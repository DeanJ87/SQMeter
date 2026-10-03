export type SettingsTabId = 'device' | 'network' | 'time' | 'sensors' | 'safety' | 'alerts';

export const SETTINGS_TABS: { id: SettingsTabId; label: string }[] = [
  { id: 'device', label: 'Device' },
  { id: 'network', label: 'Network' },
  { id: 'time', label: 'Time & Location' },
  { id: 'sensors', label: 'Sensors' },
  { id: 'safety', label: 'Safety' },
  { id: 'alerts', label: 'Alerts' },
];

// Older links (/settings?section=alpaca from the Alpaca setup redirect,
// section anchors used elsewhere in the UI) map onto the tab that now holds
// that section.
const SECTION_TO_TAB: Record<string, SettingsTabId> = {
  alpaca: 'safety',
  safety: 'safety',
  alerts: 'alerts',
  ble: 'device',
  wind: 'sensors',
  rain: 'sensors',
  mqtt: 'network',
  wifi: 'network',
  gps: 'time',
};

export const isSettingsTab = (value: string | null): value is SettingsTabId =>
  SETTINGS_TABS.some((tab) => tab.id === value);

export const tabFromLocation = (search: string): { tab: SettingsTabId; anchor?: string } => {
  const params = new URLSearchParams(search);
  const tab = params.get('tab');
  const section = params.get('section') ?? undefined;
  if (isSettingsTab(tab)) return { tab, anchor: section };
  if (section && SECTION_TO_TAB[section]) return { tab: SECTION_TO_TAB[section], anchor: section };
  return { tab: 'device' };
};

// Which tab owns a validation error path (e.g. "alerts.ntfy.topic").
export const tabForErrorPath = (path: string): SettingsTabId => {
  const root = path.split('.')[0];
  switch (root) {
    case 'wifi':
    case 'mqtt':
    case 'mqttBroker':
    case 'mqttPort':
    case 'mqttTopic':
    case 'mqttInterval':
      return 'network';
    case 'ntp':
    case 'gps':
    case 'timezone':
    case 'primaryTimeSource':
    case 'secondaryTimeSource':
      return 'time';
    case 'sensor':
    case 'cloudDetection':
    case 'rain':
    case 'wind':
    case 'sensorInterval':
    case 'i2cSDA':
    case 'i2cSCL':
    case 'i2cPins':
    case 'i2cFrequency':
      return 'sensors';
    case 'alpaca':
      return 'safety';
    case 'alerts':
      return 'alerts';
    case 'ble':
      return 'device';
    default:
      return 'device';
  }
};
