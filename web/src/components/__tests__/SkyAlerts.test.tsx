import { describe, it, expect } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import Settings from '../Settings';
import { mockStatus } from '../../mocks/data';
import { server } from '../../test/mswServer';

describe('Sky alerts', () => {
  it("blocks 'only when it's dark' without a location, and points to Location", async () => {
    server.use(http.get('/api/status', () => HttpResponse.json({ ...mockStatus, sky: { locationSource: 'none', nightKnown: false } })));
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);

    await waitFor(() => expect(screen.getByText('Needs your location.')).toBeInTheDocument());
    fireEvent.click(screen.getByText('Set up'));
    expect(await screen.findByRole('tab', { name: 'Time & Location', selected: true })).toBeInTheDocument();

    fireEvent.input(screen.getByPlaceholderText('51.4779, -0.0015'), { target: { value: '51.4779, -0.0015' } });
    fireEvent.click(screen.getByRole('tab', { name: 'Alerts' }));
    await waitFor(() => expect(screen.queryByText('Needs your location.')).toBeNull());
  });

  it('shows whether it is dark at the device', async () => {
    window.history.replaceState(null, '', '/settings?tab=time');
    render(<Settings />);
    expect(await screen.findByText('Sun at -24.3° - dark now.')).toBeInTheDocument();
    expect(screen.getByText('Using GPS')).toBeInTheDocument();
  });
});

describe('Safety alerts at night', () => {
  it('can be limited to darkness and saves the setting', async () => {
    let saved: any = null;
    server.use(
      http.post('/api/config', async ({ request }) => {
        saved = await request.json();
        return HttpResponse.json({ success: true });
      })
    );
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);
    const toggle = (await screen.findByLabelText("Safety alerts only when it's dark")) as HTMLInputElement;
    expect(toggle.checked).toBe(true);
    fireEvent.click(toggle);
    fireEvent.click(screen.getByRole('button', { name: /save/i }));
    await waitFor(() => expect(saved).not.toBeNull());
    expect(saved.alerts.safetyNightOnly).toBe(false);
  });
});
