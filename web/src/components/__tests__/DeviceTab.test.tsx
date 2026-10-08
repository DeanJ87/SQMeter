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
        ble: { available: true, active: true, clients: 1, alarm: { serviceActive: true, active: false, sequence: 0, acknowledgedSequence: 0, bondedPhones: 1, ...alarm } },
      })
    ),
    http.get('/api/config', () =>
      HttpResponse.json({ ...mockConfig, ble: { ...mockConfig.ble, enabled: true, passkey: '********' } })
    )
  );

describe('Device tab - Bluetooth phone alarm', () => {
  it('shows a ringing alarm and acknowledges it', async () => {
    let acked = false;
    bleStatus({ active: true, sequence: 4 });
    server.use(http.post('/api/ble/ack', () => { acked = true; return HttpResponse.json({ success: true }, { status: 202 }); }));
    render(<Settings />);

    expect(await screen.findByText('Alarm #4 ringing')).toBeInTheDocument();
    fireEvent.click(screen.getByText('Acknowledge alarm #4'));
    expect(await screen.findByText('Alarm acknowledged.')).toBeInTheDocument();
    expect(acked).toBe(true);
  });

  it('asks before unpairing every phone', async () => {
    let forgot = false;
    bleStatus({});
    server.use(http.post('/api/ble/forget-bonds', () => { forgot = true; return HttpResponse.json({ success: true }, { status: 202 }); }));
    render(<Settings />);

    fireEvent.click(await screen.findByText('Unpair all phones'));
    expect(forgot).toBe(false);
    fireEvent.click(screen.getByText('Yes, unpair all'));
    await waitFor(() => expect(forgot).toBe(true));
  });

  it('generates a 6-digit passkey and shows it until saved', async () => {
    bleStatus({});
    render(<Settings />);

    fireEvent.click(await screen.findByText('Generate'));
    const shown = await screen.findByText(/^\d{6}$/);
    expect(shown.textContent).toMatch(/^[1-9]\d{5}$/);
    expect(screen.getByText(/needs a restart, and paired phones must be unpaired/)).toBeInTheDocument();
  });

  it('turns the alarm off without a passkey', async () => {
    server.use(
      http.get('/api/status', () => HttpResponse.json({ ...mockStatus, ble: { available: true, active: true, clients: 0 } })),
      http.get('/api/config', () => HttpResponse.json({ ...mockConfig, ble: { ...mockConfig.ble, enabled: true, passkey: '' } }))
    );
    render(<Settings />);
    expect(await screen.findByText('Set a passkey to turn on the phone alarm.')).toBeInTheDocument();
  });
});
