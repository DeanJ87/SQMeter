import { describe, it, expect } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import Settings from '../Settings';
import { mockConfig, mockStatus } from '../../mocks/data';
import { server } from '../../test/mswServer';

const bleStatus = (alarm: Record<string, unknown>) =>
  server.use(
    http.get('/api/status', () =>
      HttpResponse.json({
        ...mockStatus,
        ble: {
          available: true,
          active: true,
          clients: 1,
          alarm: { serviceActive: true, active: false, sequence: 0, acknowledgedSequence: 0, bondedPhones: 1, ...alarm },
        },
      }),
    ),
    http.get('/api/config', () => HttpResponse.json({ ...mockConfig, ble: { ...mockConfig.ble, enabled: true, passkey: '********' } })),
  );

describe('Device tab - Bluetooth phone alarm', () => {
  it('shows a ringing alarm and acknowledges it', async () => {
    let acked = false;
    bleStatus({ active: true, sequence: 4 });
    server.use(
      http.post('/api/ble/ack', () => {
        acked = true;
        return HttpResponse.json({ success: true }, { status: 202 });
      }),
    );
    render(<Settings />);

    expect(await screen.findByText('Alarm #4 ringing')).toBeInTheDocument();
    fireEvent.click(screen.getByText('Acknowledge alarm #4'));
    expect(await screen.findByText('Alarm acknowledged.')).toBeInTheDocument();
    expect(acked).toBe(true);
  });

  it('asks before unpairing every phone', async () => {
    let forgot = false;
    bleStatus({});
    server.use(
      http.post('/api/ble/forget-bonds', () => {
        forgot = true;
        return HttpResponse.json({ success: true }, { status: 202 });
      }),
    );
    render(<Settings />);

    fireEvent.click(await screen.findByText('Unpair all phones'));
    expect(forgot).toBe(false);
    fireEvent.click(screen.getByText('Unpair every phone'));
    await waitFor(() => expect(forgot).toBe(true));
  });

  it('generates a 6-digit passkey and shows it until saved', async () => {
    bleStatus({});
    render(<Settings />);

    fireEvent.click(await screen.findByText('Generate'));
    const shown = await screen.findByText(/New passkey \d{6}/);
    expect(shown.textContent).toMatch(/New passkey [1-9]\d{5}/);
    expect(shown.textContent).toMatch(/re-pairing/);
  });

  it('turns the alarm off without a passkey', async () => {
    server.use(
      http.get('/api/status', () => HttpResponse.json({ ...mockStatus, ble: { available: true, active: true, clients: 0 } })),
      http.get('/api/config', () => HttpResponse.json({ ...mockConfig, ble: { ...mockConfig.ble, enabled: true, passkey: '' } })),
    );
    render(<Settings />);
    expect(await screen.findByText('Set a passkey to turn on the phone alarm.')).toBeInTheDocument();
  });
});

describe('Device tab - display wake (spec 031)', () => {
  const openDisplay = () => {
    server.use(http.get('/api/status', () => HttpResponse.json({ ...mockStatus, display: { ...mockStatus.display!, on: false } })));
    window.history.replaceState(null, '', '/settings?tab=device');
    render(<Settings />);
  };

  it('wakes the display', async () => {
    let woke = false;
    server.use(
      http.post('/api/display/wake', () => {
        woke = true;
        return HttpResponse.json({ success: true });
      }),
    );
    openDisplay();
    fireEvent.click(await screen.findByRole('button', { name: 'Wake' }));
    expect(await screen.findByText('Display on.')).toBeInTheDocument();
    expect(woke).toBe(true);
  });

  it.each(['Display is off in settings', 'Display not detected', 'Display is off to save power'])(
    'shows "%s" from the device',
    async (error) => {
      server.use(http.post('/api/display/wake', () => HttpResponse.json({ error }, { status: 409 })));
      openDisplay();
      fireEvent.click(await screen.findByRole('button', { name: 'Wake' }));
      expect(await screen.findByText(error)).toBeInTheDocument();
    },
  );
});
