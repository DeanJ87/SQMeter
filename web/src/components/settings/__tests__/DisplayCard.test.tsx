import { describe, it, expect } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor, within } from '@testing-library/preact';
import Settings from '../../Settings';
import { mockConfig, mockStatus } from '../../../mocks/data';
import { server } from '../../../test/mswServer';
import { mockDevice } from '../../../test/mockDevice';
import type { DisplayConfig } from '../../../types';

// Settings -> Device -> Display (spec 031 contracts/ui.md, mockups d1-d6).

const displayStatus = (display: Partial<NonNullable<typeof mockStatus.display>>, extra: Record<string, unknown> = {}) =>
  server.use(
    http.get('/api/status', () => HttpResponse.json({ ...mockStatus, display: { ...mockStatus.display!, ...display }, ...extra })),
  );

const open = (display: Partial<DisplayConfig> = {}, extra: Record<string, unknown> = {}, facts = {}) => {
  mockDevice({ config: { display: { ...mockConfig.display!, ...display }, ...extra }, facts: { displayDetected: true, ...facts } });
  window.history.replaceState(null, '', '/settings?tab=device');
  render(<Settings />);
};

const card = async () =>
  within(
    await waitFor(() => {
      const el = document.getElementById('display');
      if (!el?.textContent?.includes('Pages')) throw new Error('not rendered');
      return el;
    }),
  );

const field = (name: string) => document.querySelector(`[data-field="${name}"]`) as HTMLInputElement | HTMLSelectElement;

describe('Display card (spec 031)', () => {
  it('shows a detected display with its pages, timing and wake', async () => {
    displayStatus({ enabled: true, detected: true });
    open();
    const c = await card();
    expect(await c.findByText('Detected')).toBeInTheDocument();
    expect(c.getByRole('switch', { name: 'Show on display' })).toBeChecked();
    expect(c.getByRole('switch', { name: 'Sky' })).toBeChecked();
    expect(c.getByRole('switch', { name: 'Cloud' })).not.toBeChecked();
    expect(field('display.cycleSeconds')).not.toBeDisabled();
    expect(c.getByRole('button', { name: 'Wake' })).not.toBeDisabled();
  });

  it('moves a page up in the order', async () => {
    let body: any = null;
    server.use(
      http.post('/api/config', async ({ request }) => {
        body = await request.json();
        return HttpResponse.json({ success: true });
      }),
    );
    displayStatus({ enabled: true, detected: true });
    open();
    const c = await card();
    fireEvent.click(c.getByRole('button', { name: 'Move Safety up' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    await waitFor(() => expect(body?.display?.pages).toEqual(['sky', 'safety', 'environment']));
  });

  it('greys the controls when nothing answers at 0x3C', async () => {
    displayStatus({ enabled: true, detected: false, on: false, page: null });
    open({}, {}, { displayDetected: false });
    const c = await card();
    expect(await c.findByText('Not detected')).toBeInTheDocument();
    expect(c.getAllByText(/No display detected/).length).toBeGreaterThan(0);
    expect(c.getByRole('switch', { name: 'Show on display' })).not.toBeDisabled();
    expect(c.getByRole('switch', { name: 'Sky' })).toBeDisabled();
    expect(field('display.cycleSeconds')).toBeDisabled();
    expect(c.getByRole('button', { name: 'Wake' })).toBeDisabled();
  });

  it('is off: only its own switch works', async () => {
    displayStatus({ enabled: false, detected: false, on: false, page: null });
    open({ enabled: false });
    const c = await card();
    expect(await c.findByText('Off')).toBeInTheDocument();
    expect(c.getByRole('switch', { name: 'Show on display' })).not.toBeChecked();
    expect(c.getByRole('switch', { name: 'Sky' })).toBeDisabled();
    expect(field('display.brightness')).toBeDisabled();
    expect(c.queryByRole('button', { name: 'Wake' })).toBeDisabled();
  });

  it('warns that staying on at night uses light', async () => {
    displayStatus({ enabled: true, detected: true });
    open({ stayOnAtNight: true }, { location: { set: true, latitude: 51.5, longitude: -0.1, showSunMoon: true } });
    const c = await card();
    expect(c.getByText('Uses light at night')).toBeInTheDocument();
  });

  it('needs a location to stay on at night (D-44)', async () => {
    displayStatus({ enabled: true, detected: true });
    open({}, { location: { set: false, latitude: 0, longitude: 0, showSunMoon: true } }, { gpsFix: false });
    const c = await card();
    await waitFor(() => expect(c.getByRole('switch', { name: 'Stay on at night' })).toBeDisabled());
    expect(c.getByText(/Needs your location/)).toBeInTheDocument();
  });

  it('has nothing to cycle with one page', async () => {
    displayStatus({ enabled: true, detected: true });
    open({ pages: ['sky'] });
    await card();
    expect(field('display.cycleSeconds')).toBeDisabled();
  });

  it('marks a page without a data source', async () => {
    displayStatus({ enabled: true, detected: true }, { battery: { present: false } });
    open();
    const c = await card();
    // No fuel gauge (spec 032): the Battery page is skipped on the device.
    await waitFor(() => {
      const battery = c.getByRole('switch', { name: 'Battery' }).closest('.display-page') as HTMLElement;
      expect(within(battery).getByRole('button', { description: /No data source/ })).toBeInTheDocument();
    });
    const safety = c.getByRole('switch', { name: 'Safety' }).closest('.display-page') as HTMLElement;
    expect(within(safety).queryByRole('button', { description: /No data source/ })).not.toBeInTheDocument();
  });

  it('rejects a wake button on a pin already in use', async () => {
    displayStatus({ enabled: true, detected: true });
    open();
    await card();
    fireEvent.change(field('display.buttonPin'), { target: { value: '21' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    expect(await screen.findByText('Display: wake button pin is already in use')).toBeInTheDocument();
  });
});
