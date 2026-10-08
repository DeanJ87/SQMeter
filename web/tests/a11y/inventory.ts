// Every page and state the accessibility checks cover (spec 022 FR-001,
// FR-003). The demo build runs the device's own components, so a result here
// holds for the device; the captive-portal setup page is the /wifi route.
//
// Add new routes and dialogs here: tests/a11y.spec.ts checks each entry in
// each of its states, at both viewports, against tests/a11y/baseline.json.

export type A11yState = 'default' | 'dialog' | 'demo-panel' | 'error' | 'unsafe';

export interface InventoryEntry {
  id: string; // stable key used in the baseline
  route: string; // hash route
  component: string; // owner, for findings
  states: A11yState[];
}

export const VIEWPORTS = [
  { name: 'desktop', width: 1280, height: 800 },
  { name: 'phone', width: 320, height: 640 }, // the 400% reflow case
] as const;

const settingsTab = (tab: string, component: string, extra: A11yState[] = []): InventoryEntry => ({
  id: `settings-${tab}`,
  route: `#/settings?tab=${tab}`,
  component,
  states: ['default', ...extra],
});

export const INVENTORY: InventoryEntry[] = [
  { id: 'dashboard', route: '#/', component: 'Dashboard', states: ['default', 'dialog', 'demo-panel', 'unsafe'] },
  { id: 'alpaca', route: '#/alpaca', component: 'Alpaca', states: ['default', 'unsafe'] },
  { id: 'system', route: '#/system', component: 'System', states: ['default'] },
  settingsTab('device', 'settings/DeviceTab', ['error']),
  settingsTab('network', 'settings/NetworkTab'),
  settingsTab('time', 'settings/TimeTab'),
  settingsTab('sensors', 'settings/SensorsTab'),
  settingsTab('safety', 'settings/SafetyTab'),
  settingsTab('alerts', 'settings/AlertsTab'),
  { id: 'updates', route: '#/updates', component: 'Updates', states: ['default'] },
  { id: 'wifi', route: '#/wifi', component: 'WifiSetup (captive portal)', states: ['default'] },
  { id: 'not-found', route: '#/no-such-page', component: 'NotFound', states: ['default'] },
];
