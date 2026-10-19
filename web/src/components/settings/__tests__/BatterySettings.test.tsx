import { describe, it, expect } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor, within } from '@testing-library/preact';
import Settings from '../../Settings';
import { mockConfig, mockStatus } from '../../../mocks/data';
import { server } from '../../../test/mswServer';
import { mockDevice } from '../../../test/mockDevice';

// The battery in Settings (spec 032 contracts/ui.md): the Safety rule row and the Alerts group.

const noGauge = () =>
  server.use(
    http.get('/api/status', () =>
      HttpResponse.json({
        ...mockStatus,
        battery: { present: false },
        sensors: { ...mockStatus.sensors, battery: undefined },
        diagnostics: { ...mockStatus.diagnostics!, battery: undefined },
      }),
    ),
  );

const open = (tab: string, alpaca: Record<string, unknown> = {}, facts: Record<string, unknown> = { batteryDetected: true }) => {
  mockDevice({ config: { alpaca: { ...mockConfig.alpaca!, ...alpaca } }, facts });
  window.history.replaceState(null, '', `/settings?tab=${tab}`);
  render(<Settings />);
};

const captureSave = () => {
  const saved: { body: any } = { body: null };
  server.use(
    http.post('/api/config', async ({ request }) => {
      saved.body = await request.json();
      return HttpResponse.json({ success: true });
    }),
  );
  return saved;
};

describe('Safety tab battery rule (spec 032)', () => {
  it('shows the rule on a board with a gauge and saves it', async () => {
    const saved = captureSave();
    open('safety');
    const toggle = await screen.findByRole('switch', { name: 'Unsafe below' });
    expect(toggle).not.toBeChecked();
    expect(screen.getByRole('switch', { name: 'Send an alert' })).toBeDisabled();
    fireEvent.click(toggle);
    expect(screen.getByRole('switch', { name: 'Send an alert' })).not.toBeDisabled();
    const limit = document.querySelector('[data-field="alpaca.batteryMinPercent"]') as HTMLInputElement;
    fireEvent.change(limit, { target: { value: '15' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    await waitFor(() => expect(saved.body?.alpaca).toMatchObject({ batteryEnabled: true, batteryMinPercent: 15 }));
  });

  it('without a gauge: hidden while off, fail-safe note while on', async () => {
    noGauge();
    open('safety', {}, { batteryDetected: false });
    await screen.findByRole('switch', { name: 'Max wind speed' });
    expect(screen.queryByRole('switch', { name: 'Unsafe below' })).not.toBeInTheDocument();
  });

  it('a rule left on without a gauge says it reports unsafe', async () => {
    noGauge();
    open('safety', { batteryEnabled: true }, { batteryDetected: false });
    await screen.findByRole('switch', { name: 'Unsafe below' });
    expect(await screen.findByText(/Reports unsafe/)).toBeInTheDocument();
    expect(screen.getAllByText(/No battery gauge detected/).length).toBeGreaterThan(0);
  });

  it('ringing phones needs Bluetooth (D-50)', async () => {
    open('safety', { batteryEnabled: true });
    const ring = await screen.findByRole('switch', { name: 'Ring paired phones' });
    const note = ring.closest('div')!.parentElement!;
    expect(note.textContent).toMatch(/Bluetooth/);
  });
});

describe('Alerts tab battery group (spec 032)', () => {
  const group = async () => {
    const heading = await waitFor(() => {
      const el = [...document.querySelectorAll('h3.card-group-title')].find((h) => h.textContent === 'Battery');
      if (!el) throw new Error('no Battery group');
      return el;
    });
    return heading;
  };

  it('lists the six battery events with their defaults', async () => {
    open('alerts');
    await group();
    const level = (key: string) =>
      (within(document.querySelector(`[data-event="${key}"]`) as HTMLElement).getAllByRole('combobox')[0] as HTMLSelectElement).value;
    expect(level('battery_low')).toBe('2');
    expect(level('battery_critical')).toBe('4');
    expect(level('battery_shutdown')).toBe('4');
    expect(level('power_restored')).toBe('0');
    expect(level('charge_complete')).toBe('0');
    expect(level('charge_fault')).toBe('2');
    expect(screen.getByText('Going to sleep: battery empty')).toBeInTheDocument();
  });

  it('saves a changed battery event level', async () => {
    const saved = captureSave();
    open('alerts');
    await group();
    fireEvent.change(screen.getByLabelText('Battery charged: level'), { target: { value: '2' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    await waitFor(() => expect(saved.body?.alerts?.events?.charge_complete?.level).toBe(2));
  });

  it('has no battery group without a gauge', async () => {
    noGauge();
    open('alerts', {}, { batteryDetected: false });
    await screen.findByLabelText('Skies clear up below: level');
    expect(document.querySelector('[data-event="battery_low"]')).toBeNull();
  });
});

describe('Device tab Battery card (spec 032)', () => {
  const openDevice = (battery: Record<string, unknown> = {}, facts: Record<string, unknown> = { batteryDetected: true }) => {
    mockDevice({ config: { battery: { ...mockConfig.battery!, ...battery } }, facts });
    window.history.replaceState(null, '', '/settings?tab=device');
    render(<Settings />);
  };
  const card = async () =>
    within(
      await waitFor(() => {
        const el = document.getElementById('battery');
        if (!el?.textContent?.includes('Monitor battery')) throw new Error('not rendered');
        return el;
      }),
    );
  const field = (name: string) => document.querySelector(`[data-field="${name}"]`) as HTMLInputElement;

  it('s1: a detected gauge with its levels, power saving and pins', async () => {
    openDevice();
    const c = await card();
    expect(await c.findByText('Detected')).toBeInTheDocument();
    expect(c.getByRole('switch', { name: 'Monitor battery' })).toBeChecked();
    expect(field('battery.lowPercent').value).toBe('20');
    expect(field('battery.criticalPercent')).not.toBeDisabled();
    expect(field('battery.capacityMah').value).toBe('');
    expect(c.getByRole('switch', { name: 'Save power when low' })).toBeChecked();
    expect(field('battery.saveReadIntervalSeconds')).not.toBeDisabled();
    expect(field('battery.pgPin').value).toBe('');
  });

  it('s2: without a gauge every row but the pins is muted, said once', async () => {
    openDevice({}, { batteryDetected: false });
    const c = await card();
    expect(await c.findByText('Not detected')).toBeInTheDocument();
    expect(c.getAllByText(/No battery gauge detected/)).toHaveLength(1);
    expect(field('battery.lowPercent')).toBeDisabled();
    expect(field('battery.saveReadIntervalSeconds')).toBeDisabled();
    expect(field('battery.stat1Pin')).not.toBeDisabled();
  });

  it('s3: shows the message for a level out of order', async () => {
    openDevice();
    await card();
    fireEvent.change(field('battery.criticalPercent'), { target: { value: '22' } });
    fireEvent.change(field('battery.lowPercent'), { target: { value: '15' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    expect(await screen.findAllByText('2 to 25%, below the low level')).not.toHaveLength(0);
  });

  it('saves the levels, an empty capacity as 0 and an unset pin as 255', async () => {
    const saved = captureSave();
    openDevice({ capacityMah: 7000, alertPin: 4 });
    await card();
    fireEvent.change(field('battery.lowPercent'), { target: { value: '25' } });
    fireEvent.change(field('battery.capacityMah'), { target: { value: '' } });
    fireEvent.change(field('battery.alertPin'), { target: { value: '' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    await waitFor(() => expect(saved.body?.battery).toMatchObject({ lowPercent: 25, capacityMah: 0, alertPin: 255 }));
  });

  it('the Safety battery limit follows the critical level while equal (FR-008)', async () => {
    const saved = captureSave();
    openDevice();
    await card();
    fireEvent.change(field('battery.criticalPercent'), { target: { value: '15' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    await waitFor(() => expect(saved.body?.battery).toMatchObject({ criticalPercent: 15 }));
    expect(saved.body?.alpaca).toMatchObject({ batteryMinPercent: 15 });
  });

  it('a Safety battery limit set on its own stays when the critical level changes', async () => {
    const saved = captureSave();
    mockDevice({ config: { alpaca: { ...mockConfig.alpaca!, batteryMinPercent: 20 } }, facts: { batteryDetected: true } });
    window.history.replaceState(null, '', '/settings?tab=device');
    render(<Settings />);
    await card();
    fireEvent.change(field('battery.criticalPercent'), { target: { value: '15' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    await waitFor(() => expect(saved.body?.battery).toMatchObject({ criticalPercent: 15 }));
    expect(saved.body?.alpaca).toMatchObject({ batteryMinPercent: 20 });
  });

  it('is absent on firmware without a battery', async () => {
    const { battery: _battery, ...older } = mockConfig;
    mockDevice({ config: older });
    server.use(http.get('/api/config', () => HttpResponse.json(older)));
    window.history.replaceState(null, '', '/settings?tab=device');
    render(<Settings />);
    await screen.findByText('Show on display');
    expect(document.getElementById('battery')).toBeNull();
  });
});
