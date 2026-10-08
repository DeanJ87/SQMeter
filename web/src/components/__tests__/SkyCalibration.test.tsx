import { describe, it, expect } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import Settings from '../Settings';
import { server } from '../../test/mswServer';

describe('Sky quality settings', () => {
  it('stores a dark calibration without marking the form unsaved', async () => {
    window.history.replaceState(null, '', '/settings?tab=sensors');
    render(<Settings />);

    fireEvent.click(await screen.findByRole('button', { name: 'Calibrate dark' }));
    expect(await screen.findByText('Dark offset saved')).toBeInTheDocument();
    expect(screen.getByText(/2\.40 counts/)).toBeInTheDocument();
    expect(screen.queryByText(/unsaved/i)).toBeNull();
  });

  it("shows the device's reason when calibration is refused", async () => {
    server.use(
      http.post('/api/sensors/tsl2591/calibrate-dark', () =>
        HttpResponse.json({ error: "The averaging window isn't full yet (40 of 150 samples). Keep the sensor covered and try again." }, { status: 409 })
      )
    );
    window.history.replaceState(null, '', '/settings?tab=sensors');
    render(<Settings />);

    fireEvent.click(await screen.findByRole('button', { name: 'Calibrate dark' }));
    expect(await screen.findByText(/40 of 150 samples/)).toBeInTheDocument();
  });

  it('saves the averaging window and SQM offset', async () => {
    let saved: any = null;
    server.use(
      http.post('/api/config', async ({ request }) => {
        saved = await request.json();
        return HttpResponse.json({ success: true });
      })
    );
    window.history.replaceState(null, '', '/settings?tab=sensors');
    render(<Settings />);

    const window_ = (await waitFor(() => {
      const el = document.querySelector('[data-field="skyAveraging.windowSeconds"]');
      if (!el) throw new Error('not rendered');
      return el;
    })) as HTMLInputElement;
    fireEvent.change(window_, { target: { value: '120' } });
    fireEvent.click(screen.getByLabelText('Apply SQM offset'));
    const offset = document.querySelector('[data-field="skyCalibration.sqmOffset"]') as HTMLInputElement;
    fireEvent.change(offset, { target: { value: '0.25' } });

    fireEvent.click(screen.getByRole('button', { name: /save/i }));
    await waitFor(() => expect(saved).not.toBeNull());
    expect(saved.skyAveraging.windowSeconds).toBe(120);
    expect(saved.skyCalibration.enabled).toBe(true);
    expect(saved.skyCalibration.sqmOffset).toBeCloseTo(0.25);
  });
});
