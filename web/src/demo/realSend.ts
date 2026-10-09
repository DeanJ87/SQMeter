import { mqttPublish, type MqttTarget } from './mqttPublish';

// Opt-in real notifications from the demo (specs/018 US2/US3). Off by
// default; the visitor's keys live only in this object - never in the demo's
// saved state, URLs or logs (FR-010) - and this is the only demo code that
// talks to the outside world, only while turned on (FR-011, research R1).
// The requests themselves are built by the device's own code (FR-007).

export type RealChannel = 'ntfy' | 'pushover' | 'mqtt';
export const REAL_CHANNELS: RealChannel[] = ['ntfy', 'pushover', 'mqtt'];

export interface RealCredentials {
  ntfy?: { topic: string; token?: string };
  pushover?: { userKey: string; appToken: string };
  mqtt?: MqttTarget & { topic: string };
}

export interface DeliveryResult {
  status: 'pending' | 'sent' | 'failed' | 'skipped';
  detail: string;
}

interface HttpRequestSpec {
  url: string;
  contentType: string;
  headers: [string, string][];
  body: string;
}

interface Requests {
  ntfy?: HttpRequestSpec;
  pushover?: HttpRequestSpec;
  mqtt?: { topic: string; payload: string };
}

/** What the sender needs from the emulated device. */
export interface AlertSource {
  recentAlerts(): string;
  deliveryRequests(id: number, credentials: string): string;
}

export interface Transport {
  fetch: typeof fetch;
  mqtt: (target: MqttTarget, topic: string, payload: string) => Promise<string>;
  now: () => number;
}

export const MIN_INTERVAL_MS = 30_000;
export const MAX_PER_TAB = 10;

/** Plain-words problem with a channel's details, or null when usable. */
export function validateChannel<C extends RealChannel>(channel: C, creds: NonNullable<RealCredentials[C]>): string | null {
  if (channel === 'ntfy') {
    const { topic } = creds as NonNullable<RealCredentials['ntfy']>;
    return /^[A-Za-z0-9_-]{1,64}$/.test(topic) ? null : 'Topic: 1-64 letters, digits, - or _';
  }
  if (channel === 'pushover') {
    const { userKey, appToken } = creds as NonNullable<RealCredentials['pushover']>;
    const key = /^[A-Za-z0-9]{30}$/;
    if (!key.test(userKey)) return 'User key: 30 letters and digits';
    return key.test(appToken) ? null : 'App token: 30 letters and digits';
  }
  const { url, topic } = creds as NonNullable<RealCredentials['mqtt']>;
  if (!/^wss:\/\/[^\s/]+/.test(url)) return 'Broker: a wss:// address (MQTT over secure WebSockets)';
  return /^[A-Za-z0-9_\-/]{1,64}$/.test(topic) ? null : 'Base topic: letters, digits, -, _ and /';
}

// The service's own explanation (Pushover `errors[0]`, ntfy `error`), as the firmware shows it.
async function failureText(response: Response) {
  try {
    const body = await response.json();
    const message = body?.errors?.[0] ?? body?.error;
    if (typeof message === 'string') return message;
  } catch {
    // not JSON
  }
  return `HTTP ${response.status}`;
}

export class RealSender {
  private on = false;
  private credentials: RealCredentials = {};
  private seenId = 0;
  private limits = new Map<RealChannel, { lastAt: number; count: number }>();
  private results = new Map<number, Map<RealChannel, DeliveryResult>>();
  private listeners = new Set<() => void>();

  constructor(
    private source: AlertSource,
    private transport: Transport,
  ) {}

  get enabled() {
    return this.on;
  }

  /** Turn real sending on (after the visitor confirmed) or off. */
  setEnabled(on: boolean) {
    this.on = on;
    // Only alerts from now on: never resend what happened before opting in.
    if (on) this.seenId = this.latestId();
    this.changed();
  }

  channel<C extends RealChannel>(channel: C): RealCredentials[C] {
    return this.credentials[channel];
  }

  /** Set up (valid details) or clear a channel; returns the validation problem, if any. */
  setChannel<C extends RealChannel>(channel: C, creds: RealCredentials[C] | null): string | null {
    const problem = creds ? validateChannel(channel, creds) : null;
    if (creds && !problem) this.credentials = { ...this.credentials, [channel]: creds };
    else this.credentials = { ...this.credentials, [channel]: undefined };
    this.changed();
    return problem;
  }

  configured(): RealChannel[] {
    return REAL_CHANNELS.filter((channel) => this.credentials[channel] !== undefined);
  }

  resultsFor(id: number): Map<RealChannel, DeliveryResult> | undefined {
    return this.results.get(id);
  }

  /** Forget everything (Reset demo). */
  reset() {
    this.on = false;
    this.credentials = {};
    this.limits.clear();
    this.results.clear();
    this.changed();
  }

  onChange(listener: () => void) {
    this.listeners.add(listener);
    return () => this.listeners.delete(listener);
  }

  /** Send any alerts recorded since the last check. Call after every device step or test. */
  check() {
    if (!this.on || this.configured().length === 0) return;
    const fresh = this.recordIds().filter((id) => id > this.seenId);
    for (const id of fresh.sort((a, b) => a - b)) {
      this.seenId = id;
      void this.deliver(id);
    }
  }

  private recordIds(): number[] {
    try {
      return (JSON.parse(this.source.recentAlerts()).alerts as { id: number }[]).map((a) => a.id);
    } catch {
      return [];
    }
  }

  private latestId() {
    return Math.max(0, ...this.recordIds());
  }

  private async deliver(id: number) {
    const requests: Requests = JSON.parse(this.source.deliveryRequests(id, JSON.stringify(this.credentials)));
    const sends = (Object.keys(requests) as RealChannel[]).map((channel) => this.deliverOne(id, channel, requests));
    await Promise.all(sends);
  }

  private async deliverOne(id: number, channel: RealChannel, requests: Requests) {
    const limited = this.rateLimited(channel);
    if (limited) {
      this.setResult(id, channel, { status: 'skipped', detail: limited });
      return;
    }
    this.setResult(id, channel, { status: 'pending', detail: 'Sending...' });
    try {
      const detail = channel === 'mqtt' ? await this.sendMqtt(requests.mqtt!) : await this.sendHttp(requests[channel]!);
      this.setResult(id, channel, { status: 'sent', detail });
    } catch (error) {
      this.setResult(id, channel, { status: 'failed', detail: (error as Error).message });
    }
  }

  private rateLimited(channel: RealChannel): string | null {
    const now = this.transport.now();
    const used = this.limits.get(channel) ?? { lastAt: -Infinity, count: 0 };
    if (used.count >= MAX_PER_TAB) return `Skipped: the demo sends at most ${MAX_PER_TAB} per channel per visit`;
    if (now - used.lastAt < MIN_INTERVAL_MS) return 'Skipped: the demo sends at most one per channel every 30 s';
    this.limits.set(channel, { lastAt: now, count: used.count + 1 });
    return null;
  }

  private async sendHttp(request: HttpRequestSpec): Promise<string> {
    let response: Response;
    try {
      response = await this.transport.fetch(request.url, {
        method: 'POST',
        headers: [['Content-Type', request.contentType], ...request.headers],
        body: request.body,
      });
    } catch {
      throw new Error(`Couldn't reach ${new URL(request.url).host} from this browser`);
    }
    if (!response.ok) throw new Error(await failureText(response));
    return 'Delivered';
  }

  private sendMqtt(request: { topic: string; payload: string }) {
    const target = this.credentials.mqtt;
    if (!target) return Promise.reject(new Error('MQTT: not set up'));
    return this.transport.mqtt(target, request.topic, request.payload);
  }

  private setResult(id: number, channel: RealChannel, result: DeliveryResult) {
    const forRecord = this.results.get(id) ?? new Map<RealChannel, DeliveryResult>();
    forRecord.set(channel, result);
    this.results.set(id, forRecord);
    this.changed();
  }

  private changed() {
    this.listeners.forEach((listener) => listener());
  }
}

let shared: RealSender | null = null;

/** The sender, if the visitor has opened Real notifications this visit. */
export const existingRealSender = () => shared;

/** `/api/alerts/recent` with the real deliveries' outcomes in place of "Demo: nothing was sent". */
export function withRealResults(recentJson: string, sender: RealSender | null = shared): string {
  if (!sender) return recentJson;
  const doc = JSON.parse(recentJson) as { alerts?: { id: number; channels?: Record<string, DeliveryResult> }[] };
  for (const alert of doc.alerts ?? []) {
    const results = sender.resultsFor(alert.id);
    if (!results) continue;
    alert.channels = { ...alert.channels, ...Object.fromEntries(results) };
  }
  return JSON.stringify(doc);
}

/** The demo's one sender, wired to the emulated device. */
export async function realSender(): Promise<RealSender> {
  if (shared) return shared;
  const { demoDevice } = await import('./device');
  shared = new RealSender(demoDevice, {
    fetch: (input, init) => window.fetch(input, init),
    mqtt: mqttPublish,
    now: () => Date.now(),
  });
  demoDevice.onChange(() => shared?.check());
  return shared;
}
