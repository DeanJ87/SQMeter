import { afterEach, beforeAll, describe, expect, it } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor, within } from '@testing-library/preact';
import Settings from '../Settings';
import Alpaca from '../Alpaca';
import { mockConfig } from '../../mocks/data';
import { server } from '../../test/mswServer';
import { mockDevice } from '../../test/mockDevice';
import { CASES, alerts, level, mqttOff, mqttOn } from '../../test/settingsDependencyCases';

// Every catalogued dependency, in the UI (specs/020-settings-dependencies,
// FR-013b): the note with its reason, the one-click fix, and that a setting
// can't be switched on while what it needs is off but can always be
// switched off. The device side is test/test_settings_deps.

const control = (label: string) =>
  screen.queryByLabelText(label, { selector: 'input[type=checkbox]' }) ?? screen.getByLabelText(label, { selector: 'select' });

describe('settings dependencies in Settings (FR-013b)', () => {
  beforeAll(() => {
    Element.prototype.scrollIntoView = () => undefined; // fixes jump to their section
  });
  afterEach(() => window.history.replaceState(null, '', '/'));

  it.each(CASES.map((c) => [`${c.id}: ${c.note}`, c] as const))('%s', async (_name, c) => {
    let restarted = false;
    server.use(
      http.post('/api/restart', () => {
        restarted = true;
        return HttpResponse.json({ success: true });
      }),
    );
    mockDevice({ config: c.config, facts: c.facts });
    window.history.replaceState(null, '', `/settings?tab=${c.tab}`);
    render(<Settings />);

    const note = (await screen.findAllByText(c.note, {}, { timeout: 3000 }))[0];
    if (c.control) {
      const input = control(c.control) as HTMLInputElement;
      if (c.control_state === 'locked')
        expect(input).toBeDisabled(); // can't be switched on
      else expect(input).not.toBeDisabled(); // already on: can always be switched off
    }
    if (c.fixLabel) {
      const row = note.closest('.note') as HTMLElement;
      fireEvent.click(within(row).getByRole('button', { name: c.fixLabel }));
      if (c.fixTab) expect(await screen.findByRole('tab', { name: c.fixTab, selected: true })).toBeInTheDocument();
      else await waitFor(() => expect(restarted).toBe(true));
    }
  });

  it("D-05: an inactive event can't be tested (spec: test sends)", async () => {
    mockDevice({ config: { rain: { ...mockConfig.rain, enabled: false }, alerts: { ...alerts, events: level('rain_started', 2) } } });
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);
    const row = (await screen.findByLabelText('Rain starts: level')).closest('.event-row') as HTMLElement;
    expect(within(row).getByRole('button', { name: 'Test' })).toBeDisabled();
    const unsafe = screen.getByLabelText('It turns unsafe: level').closest('.event-row') as HTMLElement;
    expect(within(unsafe).getByRole('button', { name: 'Test' })).not.toBeDisabled();
  });

  it('D-04: with alerts off the event levels are greyed out', async () => {
    mockDevice({ config: { alerts: { ...alerts, enabled: false } } });
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);
    expect(await screen.findByLabelText('It turns unsafe: level')).toBeDisabled();
  });

  it('D-11: "Dark means" is off while both night-only options are off', async () => {
    mockDevice({ config: { alerts: { ...alerts, skyNightOnly: false, safetyNightOnly: false } } });
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);
    expect(await screen.findByLabelText('Dark means')).toBeDisabled();
  });

  it('D-23: the wind vane only shows with the anemometer on', async () => {
    mockDevice({ config: { wind: { ...mockConfig.wind, enabled: false, directionEnabled: true } } });
    window.history.replaceState(null, '', '/settings?tab=sensors');
    render(<Settings />);
    await screen.findByLabelText('Anemometer');
    expect(screen.queryByLabelText('Wind vane')).toBeNull();
  });

  it('D-30/D-31: Bluetooth settings need the Bluetooth build, the phone alarm a passkey', async () => {
    mockDevice({ facts: { bluetoothBuild: false } });
    window.history.replaceState(null, '', '/settings?tab=device');
    render(<Settings />);
    expect(await screen.findByText('Needs the Bluetooth firmware build, installed over USB.')).toBeInTheDocument();
    expect(screen.queryByLabelText('Turn on Bluetooth')).toBeNull();
  });

  it("FR-007: channels that can't deliver are not counted, and alerts reaching nowhere is a warning", async () => {
    mockDevice({
      config: {
        mqtt: mqttOff,
        alerts: {
          ...alerts,
          pushover: { ...alerts.pushover, enabled: false },
          ntfy: { ...alerts.ntfy, enabled: false },
          webhook: { ...alerts.webhook, enabled: false },
          mqtt: { enabled: true },
        },
      },
    });
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);
    expect(await screen.findByText('Alerts reach nowhere: no channel can deliver right now.')).toBeInTheDocument();
    expect(screen.getByText('No channels')).toBeInTheDocument();
    expect(screen.queryByText('Send test')).toBeNull(); // not offered on an inactive channel
  });

  it('FR-006: previews what saving would make inactive', async () => {
    mockDevice({ config: { mqtt: mqttOn, alerts: { ...alerts, mqtt: { enabled: true } } } });
    window.history.replaceState(null, '', '/settings?tab=network');
    render(<Settings />);
    fireEvent.click(await screen.findByLabelText('Publish to a broker'));
    expect(await screen.findByText(/Saving makes these inactive: .*MQTT alerts \(MQTT is off\)/)).toBeInTheDocument();
  });

  it('US1: an inactive setting keeps its value when saved (FR-001)', async () => {
    let saved: { alerts?: { mqtt?: { enabled?: boolean } }; mqtt?: { enabled?: boolean } } | null = null;
    server.use(
      http.post('/api/config', async ({ request }) => {
        saved = (await request.json()) as typeof saved;
        return HttpResponse.json({ success: true });
      }),
    );
    mockDevice({ config: { mqtt: mqttOn, alerts: { ...alerts, mqtt: { enabled: true } } } });
    window.history.replaceState(null, '', '/settings?tab=network');
    render(<Settings />);
    fireEvent.click(await screen.findByLabelText('Publish to a broker'));
    fireEvent.click(screen.getByRole('button', { name: /save/i }));
    await waitFor(() => expect(saved).not.toBeNull());
    expect(saved!.mqtt?.enabled).toBe(false);
    expect(saved!.alerts?.mqtt?.enabled).toBe(true);
  });
});

describe('Alpaca page (D-20, D-21, D-22)', () => {
  it('D-20: says when Alpaca is off', async () => {
    mockDevice({ config: { alpaca: { ...mockConfig.alpaca, enabled: false } } });
    render(<Alpaca />);
    expect(await screen.findByText('Alpaca is off, so no devices are advertised.')).toBeInTheDocument();
  });

  it('D-21/D-22: shows only the properties the device reports (no rain or wind without the sensors)', async () => {
    server.use(
      http.get('/api/v1/observingconditions/0/devicestate', () =>
        HttpResponse.json({
          Value: [
            { Name: 'CloudCover', Value: 3 },
            { Name: 'TimeStamp', Value: new Date().toISOString() },
          ],
          ErrorNumber: 0,
          ErrorMessage: '',
          ClientTransactionID: 0,
          ServerTransactionID: 1,
        }),
      ),
    );
    mockDevice({ config: { rain: { ...mockConfig.rain, enabled: false }, wind: { ...mockConfig.wind, enabled: false } } });
    render(<Alpaca />);
    expect(await screen.findByText('CloudCover')).toBeInTheDocument();
    expect(screen.queryByText('RainRate')).toBeNull();
    expect(screen.queryByText('WindSpeed')).toBeNull();
  });
});
