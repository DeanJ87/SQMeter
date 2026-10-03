import { describe, it, expect, vi } from 'vitest';
import { http, HttpResponse } from 'msw';
import { render, screen } from '@testing-library/preact';
import BleSettings from '../BleSettings';
import { mockConfig, mockStatus } from '../../mocks/data';
import { server } from '../../test/mswServer';

describe('BleSettings', () => {
  it('explains the BLE build is needed on standard firmware', async () => {
    server.use(http.get('/api/status', () => HttpResponse.json({ ...mockStatus, ble: { available: false, active: false, clients: 0 } })));
    render(<BleSettings config={mockConfig} updateConfig={vi.fn()} />);
    expect(await screen.findByText(/standard build/)).toBeInTheDocument();
  });

  it('offers the toggle on the BLE build', async () => {
    server.use(http.get('/api/status', () => HttpResponse.json({ ...mockStatus, ble: { available: true, active: true, clients: 1 } })));
    render(<BleSettings config={{ ...mockConfig, ble: { enabled: true } }} updateConfig={vi.fn()} />);
    expect(await screen.findByLabelText('Enable Bluetooth')).toBeInTheDocument();
    expect(screen.getByText(/advertising, 1 client connected/)).toBeInTheDocument();
  });
});
