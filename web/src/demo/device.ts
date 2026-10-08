import createSqmCore from './core/sqm-core.mjs';
import { SCENARIOS, darkestTime, nextSunRising, simulate, simulatorLocation, type Scenario, type ScenarioId } from './simulator';

// The demo's emulated SQMeter: the firmware's own logic (device core,
// WebAssembly) fed by the sky simulator, ticking once a second like the
// device. State lasts until the tab closes (sessionStorage).

const STORAGE_KEY = 'sqm.demo.v1';
const DEMO_VERSION = '0.2.0-beta.3';

interface Core {
  getConfig(redacted: boolean): string;
  applyConfig(json: string): string;
  loadConfig(json: string): boolean;
  restart(nowMs: number): void;
  tick(nowMs: number, epoch: number, inputs: string, localTime: string, localDate: string): void;
  readings(): string;
  statusParts(): string;
  safety(): string;
  safetyHistory(): string;
  recentAlerts(): string;
  clearAlerts(): void;
  isArmed(): boolean;
  armedDocument(): string;
  setArmed(on: boolean, source: string): void;
  testAlert(params: string): string;
  calibrateDark(): string;
  alpaca(method: string, path: string, params: string): string;
  saveState(): string;
  loadState(json: string): boolean;
}

interface Saved {
  version: 1;
  device: string;
  scenario: Scenario | null;
  timeMultiplier: number;
  demoMs: number;
  clockMs?: number;
  savedAt: number;
}

export interface Reply {
  status: number;
  body: string;
  contentType?: string;
}

type Listener = () => void;

class DemoDevice {
  private core!: Core;
  private demoMs = 1000; // device uptime clock (millis()), runs faster at 10x
  private clockMs = Date.now(); // device date and time (NTP), runs faster at 10x and moves for "Night sky"
  private lastWall = 0;
  private timer: ReturnType<typeof setInterval> | null = null;
  private listeners = new Set<Listener>();
  private restartingUntil = 0;
  scenario: Scenario | null = null;
  timeMultiplier = 1;

  async start() {
    const module = await createSqmCore();
    this.core = new module.EmulatedDevice(JSON.stringify({ version: DEMO_VERSION, mac: 'a1b2c3d4e5f6' })) as Core;
    this.restore();
    this.lastWall = Date.now();
    // ?scenario=rain etc. starts a scenario - for links from the docs and
    // for screenshots.
    const requested = typeof location === 'undefined' ? null : new URLSearchParams(location.search).get('scenario');
    const known = SCENARIOS.find((s) => s.id === requested);
    if (known) this.beginScenario(known.id);
    this.step();
    this.timer = setInterval(() => this.step(), 1000);
  }

  stop() {
    if (this.timer) clearInterval(this.timer);
  }

  /** Device clock, ms since the emulated device was first started. */
  get nowMs() {
    return this.demoMs;
  }

  /** The device's date and time. */
  get now() {
    return new Date(this.clockMs);
  }

  get restarting() {
    return Date.now() < this.restartingUntil;
  }

  onChange(listener: Listener) {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  private config() {
    return JSON.parse(this.core.getConfig(false));
  }

  private step() {
    const wall = Date.now();
    const elapsed = Math.max(0, wall - this.lastWall) * this.timeMultiplier;
    this.demoMs += elapsed;
    this.clockMs += elapsed;
    this.lastWall = wall;
    if (this.restarting) return;
    const cfg = this.config();
    const now = this.now;
    const inputs = simulate(this.demoMs, now, { location: cfg.location, gpsEnabled: cfg.gps?.enabled ?? false, cloudDetection: cfg.cloudDetection }, this.scenario);
    const pad = (n: number) => String(n).padStart(2, '0');
    const time = `${pad(now.getHours())}:${pad(now.getMinutes())}`;
    const date = `${now.getFullYear()}-${pad(now.getMonth() + 1)}-${pad(now.getDate())}`;
    this.core.tick(this.demoMs, Math.floor(this.clockMs / 1000), JSON.stringify(inputs), time, date);
    this.persist();
    this.listeners.forEach((listener) => listener());
  }

  // --- Requests -------------------------------------------------------------

  readings = () => this.core.readings();
  safety = () => this.core.safety();
  safetyHistory = () => this.core.safetyHistory();
  recentAlerts = () => this.core.recentAlerts();
  armedDocument = () => this.core.armedDocument();
  getConfig = () => this.core.getConfig(true);
  isArmed = () => this.core.isArmed();

  statusParts() {
    return JSON.parse(this.core.statusParts());
  }

  rawConfig() {
    return this.config();
  }

  applyConfig(json: string): Reply {
    const result = this.core.applyConfig(json);
    const ok = !JSON.parse(result).error;
    if (ok) this.step();
    return { status: ok ? 200 : 400, body: result };
  }

  /** Pause or resume alerts; `source` is "ui", "rest" or "mqtt". */
  setArmed(on: boolean, source: string) {
    this.core.setArmed(on, source);
    this.step();
  }

  clearAlerts() {
    this.core.clearAlerts();
    this.persist();
  }

  testAlert(params: Record<string, string>): Reply {
    const reply = JSON.parse(this.core.testAlert(JSON.stringify(params)));
    this.persist();
    return { status: reply.status, body: JSON.stringify(reply.body) };
  }

  calibrateDark(): Reply {
    const reply = JSON.parse(this.core.calibrateDark());
    this.persist();
    return { status: reply.status, body: JSON.stringify(reply.body) };
  }

  alpaca(method: string, path: string, params: [string, string][]): Reply {
    const reply = JSON.parse(this.core.alpaca(method, path, JSON.stringify(params)));
    if (reply.status === 404) return { status: 400, body: 'Invalid Alpaca device type, device number, method or HTTP verb', contentType: 'text/plain' };
    const body = typeof reply.body === 'string' ? reply.body : JSON.stringify(reply.body);
    return { status: reply.status, body, contentType: reply.contentType };
  }

  /** Restart the emulated device: a few seconds offline, then a new boot. */
  restart() {
    this.restartingUntil = Date.now() + 4000;
    setTimeout(() => {
      this.core.restart(this.demoMs);
      this.step();
    }, 4000);
  }

  // --- Demo controls ----------------------------------------------------------

  startScenario(id: ScenarioId) {
    this.beginScenario(id);
    this.step();
  }

  private beginScenario(id: ScenarioId) {
    this.scenario = { id, startedAtMs: this.demoMs };
    // Night and dawn move the device's clock, so the sun, moon, darkness
    // and the sky readings all agree.
    const where = simulatorLocation({ location: this.config().location, gpsEnabled: false });
    if (id === 'night') this.clockMs = darkestTime(this.now, where.latitude, where.longitude).getTime();
    if (id === 'dawn') {
      const dawn = nextSunRising(this.now, where.latitude, where.longitude, -12);
      if (dawn) this.clockMs = dawn.getTime();
    }
  }

  /** The rain sensor is switched on in the device's settings. */
  get rainEnabled() {
    return this.config().rain?.enabled === true;
  }

  setTimeMultiplier(multiplier: number) {
    this.timeMultiplier = multiplier;
    this.persist();
    this.listeners.forEach((listener) => listener());
  }

  reset() {
    try {
      sessionStorage.removeItem(STORAGE_KEY);
    } catch {
      // storage unavailable: nothing to clear
    }
    location.reload();
  }

  // --- Persistence --------------------------------------------------------------

  private persist() {
    try {
      const saved: Saved = {
        version: 1,
        device: this.core.saveState(),
        scenario: this.scenario,
        timeMultiplier: this.timeMultiplier,
        demoMs: this.demoMs,
        clockMs: this.clockMs,
        savedAt: Date.now(),
      };
      sessionStorage.setItem(STORAGE_KEY, JSON.stringify(saved));
    } catch {
      // Private mode or storage full: the demo keeps working without it.
    }
  }

  private restore() {
    let saved: Saved | null = null;
    try {
      saved = JSON.parse(sessionStorage.getItem(STORAGE_KEY) ?? 'null');
    } catch {
      saved = null;
    }
    if (saved?.version === 1 && this.core.loadState(saved.device)) {
      this.scenario = saved.scenario;
      this.timeMultiplier = saved.timeMultiplier === 10 ? 10 : 1;
      const away = Math.max(0, Date.now() - saved.savedAt);
      this.demoMs = Math.max(1000, saved.demoMs + away);
      this.clockMs = (saved.clockMs ?? saved.savedAt) + away;
      return;
    }
    this.applyDefaults();
  }

  // A demo that looks like a working observatory from the first second.
  private applyDefaults() {
    this.core.applyConfig(
      JSON.stringify({
        deviceName: 'SQMeter Demo',
        wifi: { ssid: 'DarkSkyLab', hostname: 'sqmeter' },
        location: { set: true, latitude: 51.5074, longitude: -0.1278, showSunMoon: true },
        rain: { enabled: true },
        wind: { enabled: true, directionEnabled: true },
        alpaca: { enabled: true, safeDelaySeconds: 0 },
        mqtt: { enabled: true, broker: '192.168.1.10', topic: 'sqmeter' },
        alerts: { enabled: true, ntfy: { enabled: true, topic: 'sqmeter-demo' } },
      })
    );
    this.core.loadConfig(this.core.getConfig(false)); // what the hardware boots with
  }
}

export const demoDevice = new DemoDevice();
