import { demoDevice } from './device';

// A simulated imaging app (N.I.N.A.-like) for the demo (specs/021, FR-023):
// it connects the emulated device's Alpaca devices and polls them at the
// usual rates, in demo time, so the 10x clock speeds it up. It keeps running
// while the Demo panel is closed.

export type ImagingAppState = 'off' | 'checking' | 'silent';

const SAFETY_POLL_MS = 3_000;
const WEATHER_POLL_MS = 60_000;
const CLIENT_ID = '4021';
const DEVICES = ['safetymonitor', 'observingconditions'] as const;

class ImagingApp {
  state: ImagingAppState = 'off';
  private lastSafetyPoll = 0;
  private lastWeatherPoll = 0;
  private transaction = 0;
  private unsubscribe: (() => void) | null = null;

  private request(method: 'GET' | 'PUT', device: (typeof DEVICES)[number], action: string, params: [string, string][] = []) {
    this.transaction += 1;
    demoDevice.alpaca(method, `/api/v1/${device}/0/${action}`, [
      ...params,
      ['ClientID', CLIENT_ID],
      ['ClientTransactionID', String(this.transaction)],
    ]);
  }

  private poll() {
    if (this.state !== 'checking') return;
    const now = demoDevice.nowMs;
    if (now - this.lastSafetyPoll >= SAFETY_POLL_MS) {
      this.lastSafetyPoll = now;
      this.request('GET', 'safetymonitor', 'issafe');
    }
    if (now - this.lastWeatherPoll >= WEATHER_POLL_MS) {
      this.lastWeatherPoll = now;
      this.request('GET', 'observingconditions', 'cloudcover');
    }
  }

  connect() {
    for (const device of DEVICES) this.request('PUT', device, 'connected', [['Connected', 'True']]);
    this.state = 'checking';
    this.lastSafetyPoll = this.lastWeatherPoll = demoDevice.nowMs;
    this.unsubscribe ??= demoDevice.onChange(() => this.poll());
  }

  // Stops polling without disconnecting: a crash, a sleeping PC, a network drop.
  goSilent() {
    if (this.state === 'checking') this.state = 'silent';
  }

  resume() {
    if (this.state !== 'silent') return;
    this.state = 'checking';
    this.lastSafetyPoll = this.lastWeatherPoll = -Infinity; // check straight away
    this.poll();
  }

  disconnect() {
    for (const device of DEVICES) this.request('PUT', device, 'connected', [['Connected', 'False']]);
    this.state = 'off';
  }
}

export const imagingApp = new ImagingApp();
