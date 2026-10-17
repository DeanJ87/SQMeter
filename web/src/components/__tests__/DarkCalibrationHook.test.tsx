import { describe, expect, it, vi } from 'vitest';
import { http, HttpResponse } from 'msw';
import { act, renderHook } from '@testing-library/preact';
import { useDarkCalibration } from '../../hooks/useSensorActions';
import { server } from '../../test/mswServer';

// Spec 030 US3: each sky sensor is dark-calibrated on its own.
describe('useDarkCalibration', () => {
  const calibrate = async (sensor?: 'sqm' | 'sqml') => {
    let url = '';
    server.use(
      http.post('/api/sensors/tsl2591/calibrate-dark', ({ request }) => {
        url = request.url;
        return HttpResponse.json({
          success: true,
          sensor: sensor ?? 'sqm',
          darkVisibleOffset: 2.4,
          sampleCount: 150,
          darkCalibratedAt: 1790000000,
        });
      }),
    );
    const applyStored = vi.fn();
    const { result } = renderHook(() => (sensor ? useDarkCalibration(applyStored, sensor) : useDarkCalibration(applyStored)));
    await act(() => result.current.calibrateDark());
    return { url: new URL(url), applyStored };
  };

  it('calibrates the SQM by default', async () => {
    const { url, applyStored } = await calibrate();
    expect(url.searchParams.get('sensor')).toBe('sqm');
    expect(applyStored.mock.calls[0][0][0]).toEqual([['skyCalibration', 'darkVisibleOffset'], 2.4]);
  });

  it('asks for the SQM-L and stores only its section', async () => {
    const { url, applyStored } = await calibrate('sqml');
    expect(url.searchParams.get('sensor')).toBe('sqml');
    const sections = applyStored.mock.calls[0][0].map(([path]: [string[]]) => path[0]);
    expect(new Set(sections)).toEqual(new Set(['skyCalibrationL']));
  });
});
