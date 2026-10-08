import createSqmCore from './core/sqm-core.mjs';
import {
  advanceRamps,
  cloneConditions,
  DEFAULT_CONDITIONS,
  getInput,
  rampRemainingMs,
  toCoreInputs,
  withDifferential,
  withInput,
  type Conditions,
  type NumericInput,
  type Ramp,
  type SensorId,
} from './conditions';
import { formatIsoWithOffset, localClock } from './posixTz';
import { DEFAULT_ELEVATION, LOCATION_PRESETS, presetAt, resolveTimePreset, type TimePresetId, type TimeResult } from './presets';
import { shortcut, type ShortcutId, type ShortcutOptions, type ShortcutResult } from './shortcuts';
import { simulatorLocation, sunLux } from './simulator';

// The demo's emulated SQMeter: the firmware's own logic (device core,
// WebAssembly) fed with the sensor readings the visitor sets
// (./conditions.ts), ticking once a second like the device. State lasts
// until the tab closes (sessionStorage; specs/019-demo-conditions/contracts/demo-state.md).

const STORAGE_KEY = 'sqm.demo.v2';
const OLD_STORAGE_KEY = 'sqm.demo.v1';
const DEMO_VERSION = '0.2.0-beta.3';
const LONDON_TZ = 'GMT0BST,M3.5.0/1,M10.5.0';
// Cloud shortcuts roll in over this long unless another ramp is chosen (FR-010).
export const CLOUD_RAMP_MS = 40_000;

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
  pending(): string;
}

interface SavedV1 {
  version: 1;
  device: string;
  timeMultiplier: number;
  demoMs: number;
  clockMs?: number;
  savedAt: number;
}

interface Saved {
  version: 2;
  device: string;
  conditions: Conditions;
  ramps: Ramp[];
  timeMultiplier: number;
  demoMs: number;
  clockMs: number;
  savedAt: number;
}

// What the device is still waiting on (contracts/core-pending.md).
export interface Pending {
  skyAveraging: { nightMode: boolean; windowSeconds: number; settlingSeconds: number };
  rainClear: { latched: boolean; rainingNow: boolean; remainingSeconds?: number };
  alerts: { condition: string; kind: 'grace' | 'settle' | 'cooldown'; remainingSeconds: number }[];
}

// ?scenario= links from the docs and screenshots (contracts/demo-state.md).
const LINKS: Record<string, { time?: TimePresetId; shortcut?: ShortcutId; rampMs?: number; fault?: SensorId }> = {
  night: { time: 'darkest', shortcut: 'clear' },
  rain: { shortcut: 'rain' },
  cloud: { shortcut: 'overcast', rampMs: CLOUD_RAMP_MS },
  clear: { shortcut: 'clear' },
  dawn: { time: 'dawn' },
  'fail-light': { fault: 'light' },
  'fail-ir': { fault: 'infrared' },
  'fail-environment': { fault: 'environment' },
  'fail-rain': { fault: 'rain' },
};

export interface Reply {
  status: number;
  body: string;
  contentType?: string;
}

type Listener = () => void;

class DemoDevice {
  private core!: Core;
  private demoMs = 1000; // device uptime clock (millis()), runs faster at 10x
  private clockMs = Date.now(); // device date and time (NTP), runs faster at 10x; presets move it
  private lastWall = 0;
  private timer: ReturnType<typeof setInterval> | null = null;
  private listeners = new Set<Listener>();
  private restartingUntil = 0;
  private inputs: Conditions = cloneConditions(DEFAULT_CONDITIONS);
  private activeRamps: Ramp[] = [];
  timeMultiplier = 1;

  async start() {
    const module = await createSqmCore();
    this.core = new module.EmulatedDevice(JSON.stringify({ version: DEMO_VERSION, mac: 'a1b2c3d4e5f6' })) as Core;
    this.restore();
    this.lastWall = Date.now();
    // ?scenario=rain etc. - for links from the docs and for screenshots.
    const requested = typeof location === 'undefined' ? null : new URLSearchParams(location.search).get('scenario');
    if (requested) this.applyLink(requested);
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

  /** The device's time zone (POSIX, from its settings). */
  get timezone(): string {
    return this.config().ntp?.timezone || 'UTC0';
  }

  /** "2026-10-08T23:10:00+0100": the device's local time, as /api/status reports it. */
  get isoTime() {
    return formatIsoWithOffset(this.timezone, this.clockMs);
  }

  /** Where the device is (its saved location, or the demo's default sky). */
  get place() {
    return simulatorLocation(this.config().location);
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
    // A switched-off rain sensor reports nothing; switched back on, it starts dry.
    if (!cfg.rain?.enabled && (this.inputs.rain.rate !== 0 || this.inputs.rain.lensFault)) {
      this.inputs = { ...this.inputs, rain: { rate: 0, lensFault: false } };
      this.activeRamps = this.activeRamps.filter((ramp) => ramp.field !== 'rain.rate');
    }
    const advanced = advanceRamps(this.inputs, this.activeRamps, this.demoMs);
    this.inputs = advanced.conditions;
    this.activeRamps = advanced.ramps;
    const place = simulatorLocation(cfg.location);
    const inputs = toCoreInputs(this.inputs, this.demoMs, {
      sunLux: sunLux(this.now, place),
      gps: { enabled: cfg.gps?.enabled ?? false, ...place, altitude: presetAt(place)?.elevation ?? DEFAULT_ELEVATION },
    });
    const { time, date } = localClock(cfg.ntp?.timezone || 'UTC0', this.clockMs);
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

  pending(): Pending {
    return JSON.parse(this.core.pending());
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
    if (reply.status === 404)
      return { status: 400, body: 'Invalid Alpaca device type, device number, method or HTTP verb', contentType: 'text/plain' };
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

  // --- Sensor inputs (spec 019) -------------------------------------------------

  get conditions(): Readonly<Conditions> {
    return this.inputs;
  }

  get ramps(): readonly Ramp[] {
    return this.activeRamps;
  }

  rampRemainingMs(ramp: Ramp) {
    return rampRemainingMs(ramp, this.demoMs);
  }

  private update(next: Conditions, cancelRampsFor: NumericInput[] = []) {
    this.inputs = next;
    this.activeRamps = this.activeRamps.filter((ramp) => !cancelRampsFor.includes(ramp.field));
    this.step();
  }

  /** Sets one reading; returns true if it was outside the sensor's range and clamped. */
  setInput(field: NumericInput, value: number) {
    const { conditions, clamped } = withInput(this.inputs, field, value);
    this.update(conditions, [field]);
    return clamped;
  }

  setDifferential(value: number) {
    const { conditions, clamped } = withDifferential(this.inputs, value);
    this.update(conditions, ['ir.sky']);
    return clamped;
  }

  setFault(sensor: SensorId, on: boolean) {
    this.update({ ...this.inputs, faults: { ...this.inputs.faults, [sensor]: on } });
  }

  setLensFault(on: boolean) {
    this.update({ ...this.inputs, rain: { ...this.inputs.rain, lensFault: on } });
  }

  setGpsFix(on: boolean) {
    this.update({ ...this.inputs, gps: { fix: on } });
  }

  followSun() {
    this.update({ ...this.inputs, light: { ...this.inputs.light, mode: 'sun' } }, ['light.lux']);
  }

  setSteady(on: boolean) {
    this.update({ ...this.inputs, steady: on });
  }

  /** Works the shortcut out from the current settings and applies it (instantly, or ramped). */
  applyShortcut(id: ShortcutId, rampMs = 0, options: ShortcutOptions = {}): ShortcutResult {
    const result = shortcut(id, this.config(), this.inputs, options);
    if (!result.ok) return result;
    const fields = Object.keys(result.changes) as NumericInput[];
    if (rampMs > 0) {
      const ramps = fields.map((field) => ({
        field,
        from: getInput(this.inputs, field),
        to: result.changes[field] as number,
        startMs: this.demoMs,
        durationMs: rampMs,
      }));
      this.activeRamps = [...this.activeRamps.filter((ramp) => !fields.includes(ramp.field)), ...ramps];
      this.step();
      return result;
    }
    let next = this.inputs;
    for (const field of fields) next = withInput(next, field, result.changes[field] as number).conditions;
    this.update(next, fields);
    return result;
  }

  // --- Clock and place -------------------------------------------------------------

  /** Moves the device's clock (UTC ms); it runs on from there at 1x or 10x. */
  setClock(ms: number) {
    if (!Number.isFinite(ms)) return;
    this.clockMs = ms;
    this.step();
  }

  applyTimePreset(id: TimePresetId): TimeResult {
    const result = resolveTimePreset(id, this.clockMs, this.place, this.timezone);
    if (result.ok) this.setClock(result.at);
    return result;
  }

  /** Saves the place and its time zone, as Settings -> Time & Location would. */
  applyLocationPreset(id: string): Reply {
    const preset = LOCATION_PRESETS.find((p) => p.id === id);
    if (!preset) return { status: 400, body: JSON.stringify({ error: 'Unknown place' }) };
    return this.applyConfig(
      JSON.stringify({
        location: { set: true, latitude: preset.latitude, longitude: preset.longitude },
        ntp: { timezone: preset.timezone },
      }),
    );
  }

  private applyLink(id: string) {
    const link = LINKS[id];
    if (!link) return;
    if (link.time) {
      const result = resolveTimePreset(link.time, this.clockMs, this.place, this.timezone);
      if (result.ok) this.clockMs = result.at;
    }
    if (link.shortcut) this.applyShortcut(link.shortcut, link.rampMs ?? 0);
    if (link.fault) this.inputs = { ...this.inputs, faults: { ...this.inputs.faults, [link.fault]: true } };
  }

  /** The device's switched-on optional sensors. */
  get rainEnabled() {
    return this.config().rain?.enabled === true;
  }

  get windEnabled() {
    return this.config().wind?.enabled === true;
  }

  get gpsEnabled() {
    return this.config().gps?.enabled === true;
  }

  setTimeMultiplier(multiplier: number) {
    this.timeMultiplier = multiplier;
    this.persist();
    this.listeners.forEach((listener) => listener());
  }

  reset() {
    try {
      sessionStorage.removeItem(STORAGE_KEY);
      sessionStorage.removeItem(OLD_STORAGE_KEY);
    } catch {
      // storage unavailable: nothing to clear
    }
    location.reload();
  }

  // --- Persistence --------------------------------------------------------------

  private persist() {
    try {
      const saved: Saved = {
        version: 2,
        device: this.core.saveState(),
        conditions: this.inputs,
        ramps: this.activeRamps,
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
    const read = <T>(key: string): T | null => {
      try {
        return JSON.parse(sessionStorage.getItem(key) ?? 'null');
      } catch {
        return null;
      }
    };
    const saved = read<Saved>(STORAGE_KEY);
    // A session from before spec 019: keep the device and its clocks; its scenario is gone.
    const old = saved ? null : read<SavedV1>(OLD_STORAGE_KEY);
    const state = saved?.version === 2 ? saved : old?.version === 1 ? old : null;
    if (state && this.core.loadState(state.device)) {
      if (saved?.version === 2) {
        this.inputs = { ...cloneConditions(DEFAULT_CONDITIONS), ...saved.conditions };
        this.activeRamps = Array.isArray(saved.ramps) ? saved.ramps : [];
      }
      this.timeMultiplier = state.timeMultiplier === 10 ? 10 : 1;
      const away = Math.max(0, Date.now() - state.savedAt);
      this.demoMs = Math.max(1000, state.demoMs + away);
      this.clockMs = (state.clockMs ?? state.savedAt) + away;
      if (old) {
        try {
          sessionStorage.removeItem(OLD_STORAGE_KEY);
        } catch {
          // storage unavailable: nothing to clear
        }
      }
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
        ntp: { timezone: LONDON_TZ },
        rain: { enabled: true },
        wind: { enabled: true, directionEnabled: true },
        alpaca: { enabled: true, safeDelaySeconds: 0 },
        mqtt: { enabled: true, broker: '192.168.1.10', topic: 'sqmeter' },
        alerts: { enabled: true, ntfy: { enabled: true, topic: 'sqmeter-demo' } },
      }),
    );
    this.core.loadConfig(this.core.getConfig(false)); // what the hardware boots with
  }
}

export const demoDevice = new DemoDevice();
