import { describe, it, expect } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import WifiSetup from '../WifiSetup';
import { mockStatus } from '../../mocks/data';
import { server } from '../../test/mswServer';

const statusWith = (wifi: Partial<typeof mockStatus.wifi>) => HttpResponse.json({ ...mockStatus, wifi: { ...mockStatus.wifi, ...wifi } });

const field = (name: string) => document.querySelector(`[data-field="${name}"]`) as HTMLInputElement;

describe('WiFi setup screen', () => {
  it('scans, connects and shows the new address', async () => {
    let sent: any = null;
    let polls = 0;
    server.use(
      http.post('/api/wifi/connect', async ({ request }) => {
        sent = await request.json();
        return HttpResponse.json({ success: true, pending: true }, { status: 202 });
      }),
      http.get('/api/status', () =>
        ++polls < 2
          ? statusWith({ connectPending: true, connected: false, apMode: true })
          : statusWith({
              connectPending: false,
              connected: true,
              ssid: 'DarkSkyLab',
              ip: '192.168.1.77',
              hostname: 'sqmeter',
              mdns: true,
              apMode: true,
            }),
      ),
    );
    render(<WifiSetup pollMs={5} />);

    fireEvent.click(await screen.findByRole('radio', { name: /DarkSkyLab/ }));
    fireEvent.input(field('setup.password'), { target: { value: 'hunter22' } });
    fireEvent.click(screen.getByRole('button', { name: 'Connect' }));

    expect(await screen.findByText('Connected')).toBeInTheDocument();
    expect(sent).toEqual({ ssid: 'DarkSkyLab', password: 'hunter22' });
    expect(screen.getByRole('link', { name: 'http://sqmeter.local' })).toBeInTheDocument();
    expect(screen.getByRole('link', { name: 'http://192.168.1.77' })).toBeInTheDocument();
  });

  it('sends no password for an open network', async () => {
    let sent: any = null;
    server.use(
      http.post('/api/wifi/connect', async ({ request }) => {
        sent = await request.json();
        return HttpResponse.json({ success: true, pending: true }, { status: 202 });
      }),
      http.get('/api/status', () => statusWith({ connectPending: false, connected: true, ssid: 'Observatory-Guest', ip: '10.0.0.5' })),
    );
    render(<WifiSetup pollMs={5} />);

    fireEvent.click(await screen.findByRole('radio', { name: /Observatory-Guest/ }));
    expect(document.querySelector('[data-field="setup.password"]')).toBeNull();
    fireEvent.click(screen.getByRole('button', { name: 'Connect' }));
    await waitFor(() => expect(sent).toEqual({ ssid: 'Observatory-Guest', password: '' }));
    expect(await screen.findByText('Connected')).toBeInTheDocument();
  });

  it('reports a failed attempt', async () => {
    server.use(http.get('/api/status', () => statusWith({ connectPending: false, connected: false, apMode: true })));
    render(<WifiSetup pollMs={5} />);

    fireEvent.click(await screen.findByRole('radio', { name: /Other network/ }));
    fireEvent.input(field('setup.ssid'), { target: { value: 'Hidden' } });
    fireEvent.input(field('setup.password'), { target: { value: 'wrongpass' } });
    fireEvent.click(screen.getByRole('button', { name: 'Connect' }));

    expect(await screen.findByText(/Couldn't join Hidden/)).toBeInTheDocument();
  });
});
