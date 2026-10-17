import { describe, it, expect } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor, within } from '@testing-library/preact';
import Settings from '../Settings';
import { mockConfig } from '../../mocks/data';
import { server } from '../../test/mswServer';
import { mockDevice } from '../../test/mockDevice';

// The mock device has both sky sensors on (spec 030): one card per sensor.
const card = async (id: string) =>
  within(
    await waitFor(() => {
      const el = document.getElementById(id);
      if (!el) throw new Error(`#${id} not rendered`);
      return el;
    }),
  );

const openSensors = () => {
  window.history.replaceState(null, '', '/settings?tab=sensors');
  render(<Settings />);
};

describe('Sky quality settings', () => {
  it('stores a dark calibration without marking the form unsaved', async () => {
    openSensors();
    const sqm = await card('sky');
    fireEvent.click(await sqm.findByRole('button', { name: 'Calibrate dark' }));
    expect(await sqm.findByText('Dark offset saved')).toBeInTheDocument();
    expect(sqm.getByText(/2\.40 counts/)).toBeInTheDocument();
    expect(screen.queryByText(/unsaved/i)).toBeNull();
  });

  it("shows the device's reason when calibration is refused", async () => {
    server.use(
      http.post('/api/sensors/tsl2591/calibrate-dark', () =>
        HttpResponse.json(
          { error: "The averaging window isn't full yet (40 of 150 samples). Keep the sensor covered and try again." },
          { status: 409 },
        ),
      ),
    );
    openSensors();
    const sqm = await card('sky');
    fireEvent.click(await sqm.findByRole('button', { name: 'Calibrate dark' }));
    expect(await sqm.findByText(/40 of 150 samples/)).toBeInTheDocument();
  });

  it('saves the averaging window and SQM offset', async () => {
    let saved: any = null;
    server.use(
      http.post('/api/config', async ({ request }) => {
        saved = await request.json();
        return HttpResponse.json({ success: true });
      }),
    );
    openSensors();
    const sqm = await card('sky');
    fireEvent.change(document.querySelector('[data-field="skyAveraging.windowSeconds"]') as HTMLInputElement, { target: { value: '120' } });
    fireEvent.click(sqm.getByLabelText('Apply SQM offset'));
    const offset = document.querySelector('[data-field="skyCalibration.sqmOffset"]') as HTMLInputElement;
    fireEvent.change(offset, { target: { value: '0.25' } });

    fireEvent.click(screen.getByRole('button', { name: /save/i }));
    await waitFor(() => expect(saved).not.toBeNull());
    expect(saved.skyAveraging.windowSeconds).toBe(120);
    expect(saved.skyCalibration.enabled).toBe(true);
    expect(saved.skyCalibration.sqmOffset).toBeCloseTo(0.25);
  });

  it('titles one card per sensor and calibrates the SQM-L on its own (US3)', async () => {
    let url = '';
    server.use(
      http.post('/api/sensors/tsl2591/calibrate-dark', ({ request }) => {
        url = request.url;
        return HttpResponse.json({ success: true, sensor: 'sqml', darkVisibleOffset: 1.7, sampleCount: 150, darkCalibratedAt: 0 });
      }),
    );
    openSensors();
    const sqm = await card('sky');
    const sqml = await card('sky-sqml');
    expect(sqm.getByRole('heading', { name: 'SQM' })).toBeInTheDocument();
    expect(sqml.getByRole('heading', { name: 'SQM-L' })).toBeInTheDocument();

    fireEvent.click(sqml.getByRole('button', { name: 'Calibrate dark' }));
    expect(await sqml.findByText(/1\.70 counts/)).toBeInTheDocument();
    expect(new URL(url).searchParams.get('sensor')).toBe('sqml');
    expect(sqm.queryByText(/1\.70 counts/)).toBeNull();
  });

  it('saves the SQM-L averaging and offset to its own sections', async () => {
    let saved: any = null;
    server.use(
      http.post('/api/config', async ({ request }) => {
        saved = await request.json();
        return HttpResponse.json({ success: true });
      }),
    );
    openSensors();
    const sqml = await card('sky-sqml');
    fireEvent.change(document.querySelector('[data-field="skyAveragingL.windowSeconds"]') as HTMLInputElement, { target: { value: '60' } });
    fireEvent.click(sqml.getByLabelText('Apply SQM offset'));
    fireEvent.change(document.querySelector('[data-field="skyCalibrationL.sqmOffset"]') as HTMLInputElement, { target: { value: '-0.1' } });
    fireEvent.click(screen.getByRole('button', { name: /save/i }));
    await waitFor(() => expect(saved).not.toBeNull());
    expect(saved.skyAveragingL.windowSeconds).toBe(60);
    expect(saved.skyCalibrationL).toMatchObject({ enabled: true, sqmOffset: -0.1 });
    expect(saved.skyAveraging.windowSeconds).toBe(mockConfig.skyAveraging!.windowSeconds);
  });

  it('keeps the single "Sky quality" card with only the SQM on', async () => {
    mockDevice({ config: { sky: { ...mockConfig.sky!, sqmlEnabled: false } } });
    openSensors();
    const sqm = await card('sky');
    expect(sqm.getByRole('heading', { name: 'Sky quality' })).toBeInTheDocument();
    expect(document.getElementById('sky-sqml')).toBeNull();
  });
});
