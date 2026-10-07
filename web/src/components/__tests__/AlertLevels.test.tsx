import { describe, it, expect } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import Settings from '../Settings';
import { server } from '../../test/mswServer';

describe('Alert levels', () => {
  it('sets a level and Pushover sound per event and saves them', async () => {
    let saved: any = null;
    server.use(
      http.post('/api/config', async ({ request }) => {
        saved = await request.json();
        return HttpResponse.json({ success: true });
      })
    );
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);

    const clearLevel = (await screen.findByLabelText('Skies clear up below: level')) as HTMLSelectElement;
    expect(clearLevel.value).toBe('2');
    fireEvent.change(clearLevel, { target: { value: '1' } });
    // Quiet alerts make no sound, so there's no sound to pick.
    await waitFor(() => expect(screen.queryByLabelText('Skies clear up below: sound')).toBeNull());

    fireEvent.change(screen.getByLabelText('A sensor fails: sound'), { target: { value: 'siren' } });
    fireEvent.click(screen.getByRole('button', { name: /save/i }));

    await waitFor(() => expect(saved).not.toBeNull());
    expect(saved.alerts.events.clear_sky).toEqual({ level: 1, sound: 'magic' });
    expect(saved.alerts.events.sensor_fault).toEqual({ level: 4, sound: 'siren' });
  });


  it('tests one event with its unsaved level and sound, and reports every channel', async () => {
    let query = '';
    let calls = 0;
    server.use(
      http.post('/api/alerts/test', ({ request }) => {
        query = new URL(request.url).search;
        return HttpResponse.json({ success: true }, { status: 202 });
      }),
      http.get('/api/alerts/recent', () => {
        calls += 1;
        const alerts =
          calls === 1
            ? []
            : [{ id: 9, event: 'clouded_over', title: 'Test: Clouded over', message: '', level: 'wake', ageSeconds: 1, channels: { pushover: { status: 'sent', detail: 'HTTP 200' } } }];
        return HttpResponse.json({ enabled: true, alerts });
      })
    );
    window.history.replaceState(null, '', '/settings?tab=alerts');
    render(<Settings />);

    fireEvent.change(await screen.findByLabelText('Skies cloud over above: sound'), { target: { value: 'siren' } });
    const row = document.querySelector('[data-event="clouded_over"]')!;
    fireEvent.click(row.querySelector('button')!);

    expect(await screen.findByText('Pushover sent', {}, { timeout: 4000 })).toBeInTheDocument();
    expect(query).toBe('?channel=all&event=clouded_over&level=4&sound=siren');
  });
});
