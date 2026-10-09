// What the dashboard needs to know about the demo (specs/025 FR-022), set by
// main.tsx in the demo build. Components never import demo/ (STRUCT-05).

export interface DemoInfo {
  clockShifted: boolean; // the demo device's clock isn't the real time (spec 019)
  openPanel: (event: Event) => void; // opens the Demo panel
}

let provider: (() => DemoInfo) | null = null;

export const setDemoInfo = (next: () => DemoInfo) => {
  provider = next;
};

/** The demo's state, or null on a real device. */
export const demoInfo = (): DemoInfo | null => (provider ? provider() : null);
