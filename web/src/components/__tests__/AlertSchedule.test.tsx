import { describe, it, expect } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor, within } from '@testing-library/preact';
import Settings from '../Settings';
import { mockConfig, mockStatus } from '../../mocks/data';
import { server } from '../../test/mswServer';
import { describeSchedule } from '../settings/alertSchedule';
import type { AlertSchedule } from '../../types';

// When alerts are sent (specs/021): the wording, the status line and
// Pause/Resume.

// A label's own words, without the "?" tooltip's text inside it.
const ownText = (el: Element) =>
  Array.from(el.childNodes)
    .filter((node) => node.nodeType === Node.TEXT_NODE)
    .map((node) => node.textContent ?? '')
    .join('');

const openAlerts = async () => {
  window.history.replaceState(null, '', '/settings?tab=alerts');
  render(<Settings />);
  return screen.findByRole('combobox', { name: 'When to send' });
};

describe('Alert schedule wording (FR-017, SC-004)', () => {
  it('labels say what happens: no questions, no negations, no product names', async () => {
    await openAlerts();
    const card = document.getElementById('alerts') as HTMLElement;
    const table = document.querySelector('.event-table') as HTMLElement;
    const labels = [
      ...Array.from(card.querySelectorAll('label, .card-group-title, option, button')).map(ownText),
      ...Array.from(table.querySelectorAll('[data-event^="client_"] .event-label > span')).map(ownText),
      ...['Silent for - safety monitor', 'Silent for - weather device'],
    ]
      .map((text) => text.trim())
      .filter(Boolean);
    expect(labels).toEqual(expect.arrayContaining(['When to send', 'Any time', 'Only while an imaging app is connected', 'Pause alerts']));
    for (const label of labels) {
      expect(label, label).not.toMatch(/\?$/);
      expect(label, label).not.toMatch(/\b(not|no|don't)\b|off when/i);
      expect(label, label).not.toMatch(/N\.?I\.?N\.?A/i);
    }
    expect(screen.queryByText('Alerts on now')).toBeNull();
    expect(screen.queryByText(/When you're not imaging/)).toBeNull();
  });

  it('explains the selected mode, naming N.I.N.A. only as an example', async () => {
    await openAlerts();
    expect(screen.getByText('Alerts go out whenever something happens, unless you pause them.')).toBeInTheDocument();
    fireEvent.change(screen.getByRole('combobox', { name: 'When to send' }), { target: { value: 'whileConnected' } });
    expect(await screen.findByText(/Alerts start when an imaging app \(e\.g\. N\.I\.N\.A\.\) connects/)).toBeInTheDocument();
  });

  it('saves the mode with armWithAlpaca for older firmware', async () => {
    let saved: any = null;
    server.use(
      http.post('/api/config', async ({ request }) => {
        saved = await request.json();
        return HttpResponse.json({ success: true });
      }),
    );
    await openAlerts();
    fireEvent.change(screen.getByRole('combobox', { name: 'When to send' }), { target: { value: 'whileConnected' } });
    fireEvent.click(await screen.findByRole('button', { name: /save/i }));
    await waitFor(() => expect(saved).not.toBeNull());
    expect(saved.alerts.sendMode).toBe('whileConnected');
    expect(saved.alerts.armWithAlpaca).toBe(true);
  });
});

describe('Alert schedule status line', () => {
  it('pauses from the button, tagged as the web UI, and says so', async () => {
    const calls: string[] = [];
    let armed = true;
    server.use(
      http.post('/api/alerts/disarm', ({ request }) => {
        calls.push(new URL(request.url).search);
        armed = false;
        return HttpResponse.json({ success: true, armed: false }, { status: 202 });
      }),
      http.get('/api/alerts/armed', () =>
        HttpResponse.json(
          armed
            ? { armed: true, mode: 'any', reason: 'none', since: null, sinceAgeMs: null }
            : { armed: false, mode: 'any', reason: 'user-ui', since: '2026-10-08T21:04:00Z', sinceAgeMs: 1000 },
        ),
      ),
    );
    await openAlerts();
    expect(await screen.findByText('Sending alerts.')).toBeInTheDocument();
    fireEvent.click(screen.getByRole('button', { name: 'Pause alerts' }));
    await waitFor(() => expect(calls).toEqual(['?source=ui']));
    expect(await screen.findByText(/^Paused by you at .* \(Pause button\)\. Alerts resume when you resume them\.$/)).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Resume alerts' })).toBeInTheDocument();
    expect(screen.queryByRole('button', { name: /save/i })).toBeNull();
  });

  it('follows the device status (e.g. paused from Home Assistant)', async () => {
    server.use(
      http.get('/api/status', () =>
        HttpResponse.json({
          ...mockStatus,
          alerts: { armed: false, armWithAlpaca: false, mode: 'any', reason: 'user-mqtt', since: null, sinceAgeMs: 120000 },
        }),
      ),
    );
    await openAlerts();
    expect(await screen.findByText('Paused from Home Assistant or MQTT 2m ago. Alerts resume when you resume them.')).toBeInTheDocument();
  });

  it('shows a silent imaging app even while alerts are paused', async () => {
    server.use(
      http.get('/api/status', () =>
        HttpResponse.json({
          ...mockStatus,
          alerts: { armed: false, armWithAlpaca: false, mode: 'any', reason: 'user-ui', since: null, sinceAgeMs: 60000 },
          alpaca: {
            enabled: true,
            clients: {
              safetymonitor: { connected: true, watching: true, silent: true, lastCheckedAgeMs: 240000, clientId: 1 },
              observingconditions: { connected: false, watching: false, silent: false, lastCheckedAgeMs: null, clientId: null },
            },
          },
        }),
      ),
    );
    await openAlerts();
    expect(await screen.findByText('The imaging app has gone quiet - safety monitor last checked 4m ago.')).toBeInTheDocument();
  });

  it('warns when imaging-app mode needs Alpaca, which is off', async () => {
    server.use(http.get('/api/config', () => HttpResponse.json({ ...mockConfig, alpaca: { ...mockConfig.alpaca, enabled: false } })));
    await openAlerts();
    fireEvent.change(screen.getByRole('combobox', { name: 'When to send' }), { target: { value: 'whileConnected' } });
    expect(await screen.findByText('Imaging apps connect over Alpaca, which is switched off.')).toBeInTheDocument();
    const lost = document.querySelector('[data-event="client_lost"]') as HTMLElement;
    expect(within(lost).getByText('The imaging app stops checking')).toBeInTheDocument();
    expect(screen.getAllByText('Alpaca is switched off.').length).toBe(3);
  });
});

describe('describeSchedule', () => {
  const at = (schedule: Partial<AlertSchedule>) => describeSchedule({ armed: false, since: null, sinceAgeMs: null, ...schedule });
  it('states each reason in words', () => {
    expect(at({ armed: true })).toBe('Sending alerts.');
    expect(at({ armed: true, mode: 'whileConnected', reason: 'client-connected', sinceAgeMs: 60000 })).toBe(
      'Sending alerts - an imaging app connected 1m ago.',
    );
    expect(at({ mode: 'whileConnected', reason: 'client-disconnected', sinceAgeMs: 300000 })).toBe(
      'Paused - the imaging app disconnected 5m ago. Alerts resume when it connects again.',
    );
    expect(at({ mode: 'whileConnected', reason: 'waiting-for-client' })).toBe(
      'Waiting for an imaging app to connect - nothing is sent until then.',
    );
    expect(at({ reason: 'user-rest' })).toBe('Paused by a script (REST). Alerts resume when you resume them.');
    expect(at({ mode: 'whileConnected', reason: 'user-ui' })).toBe(
      'Paused by you (Pause button). Alerts resume when an imaging app connects, or when you resume them.',
    );
    expect(at({ reason: 'migrated' })).toBe('Paused (before the update). Alerts resume when you resume them.');
    expect(at({})).toBe('Paused. Alerts resume when you resume them.');
  });
});
