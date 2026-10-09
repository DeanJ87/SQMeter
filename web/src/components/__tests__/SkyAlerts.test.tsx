import { describe, it, expect } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import Settings from '../Settings';
import { mockConfig, mockStatus } from '../../mocks/data';
import { server } from '../../test/mswServer';
import { mockDevice } from '../../test/mockDevice';

describe('Sky alerts', () => {
  it("blocks 'only when it's dark' without a location, and points to Location", async () => {
    server.use(http.get('/api/status', () => HttpResponse.json({ ...mockStatus, sky: { locationSource: 'none', nightKnown: false } })));
    mockDevice({ config: { alerts: { ...mockConfig.alerts!, skyNightOnly: false } }, facts: { gpsFix: false } });
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);

    await waitFor(() => expect(screen.getByText('Needs your location.')).toBeInTheDocument());
    fireEvent.click(screen.getAllByText('Set location')[0]);
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
      }),
    );
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);
    const toggle = (await screen.findByLabelText('Safety alerts only after dark')) as HTMLInputElement;
    expect(toggle.checked).toBe(true);
    fireEvent.click(toggle);
    fireEvent.click(screen.getByRole('button', { name: /save/i }));
    await waitFor(() => expect(saved).not.toBeNull());
    expect(saved.alerts.safetyNightOnly).toBe(false);
  });
});

describe('Alert text defaults', () => {
  it("shows the device's wording for 'Skies clear' and offers {event}", async () => {
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);
    await screen.findByText('Skies clear up below');
    const clearSky = (await screen.findAllByTitle('Write your own title and message')).find((b) => b.closest('[data-event="clear_sky"]'));
    fireEvent.click(clearSky!);
    const editor = await waitFor(() => {
      const el = document.querySelector('[data-template="clear_sky"]');
      if (!el) throw new Error('editor not open');
      return el as HTMLElement;
    });
    const title = editor.querySelector('input[aria-label="Alert title"]') as HTMLInputElement;
    // Sky alerts default to darkness-only in the mock config.
    const nightOnly = (screen.getByLabelText("Sky alerts only when it's dark") as HTMLInputElement).checked;
    expect(title.placeholder).toBe(nightOnly ? 'Dark and clear' : 'Skies clear');
    fireEvent.click(screen.getByLabelText("Sky alerts only when it's dark"));
    await waitFor(() => expect(title.placeholder).toBe(nightOnly ? 'Skies clear' : 'Dark and clear'));
    expect(editor.querySelector('button[title^="Event name"]')).not.toBeNull();
  });
});
