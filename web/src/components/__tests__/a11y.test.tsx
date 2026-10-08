import { describe, it, expect, afterEach } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import { InfoTip, ProgressMeter } from '../ui';
import { Field, NumberInput, SelectInput, TextInput } from '../settings/controls';
import Settings from '../Settings';
import AlertsBell from '../AlertsBell';
import { server } from '../../test/mswServer';

// Roles and accessible names of the shared building blocks (spec 022 FR-018).

describe('InfoTip', () => {
  it('is a button whose description is the tip', () => {
    render(<InfoTip text="Shown in N.I.N.A." />);
    const tip = screen.getByRole('button', { name: 'More information' });
    expect(tip).toHaveAccessibleDescription('Shown in N.I.N.A.');
  });

  it('opens on tap and hides on Escape', () => {
    render(<InfoTip text="Detail" />);
    const tip = screen.getByRole('button', { name: 'More information' });
    fireEvent.click(tip);
    expect(tip).toHaveAttribute('aria-expanded', 'true');
    fireEvent.keyDown(tip, { key: 'Escape' });
    expect(tip).toHaveAttribute('aria-expanded', 'false');
    expect(tip).toHaveClass('is-dismissed');
  });
});

describe('Field', () => {
  it('names its inputs by the label, not the tip', () => {
    render(
      <Field label="Hostname" hint="Reachable at .local">
        <TextInput value="" onInput={() => undefined} />
      </Field>,
    );
    expect(screen.getByRole('textbox', { name: 'Hostname' })).toBeInTheDocument();
  });

  it('ties its error to the input and marks it invalid', () => {
    render(
      <Field label="Port" error="Port must be 1-65535">
        <NumberInput value={0} onChange={() => undefined} error="Port must be 1-65535" />
      </Field>,
    );
    const input = screen.getByRole('spinbutton', { name: 'Port' });
    expect(input).toHaveAttribute('aria-invalid', 'true');
    expect(input).toHaveAccessibleDescription('Port must be 1-65535');
  });

  it('lets an explicit ariaLabel win', () => {
    render(
      <Field label="Range">
        <SelectInput value="a" onChange={() => undefined} options={[{ value: 'a', label: 'A' }]} ariaLabel="Units" />
      </Field>,
    );
    expect(screen.getByRole('combobox', { name: 'Units' })).toBeInTheDocument();
  });
});

describe('ProgressMeter', () => {
  it('is a progress bar with its value', () => {
    render(<ProgressMeter value={42.4} label="Update progress" />);
    const bar = screen.getByRole('progressbar', { name: 'Update progress' });
    expect(bar).toHaveAttribute('aria-valuenow', '42');
  });
});

describe('Settings tabs', () => {
  afterEach(() => window.history.replaceState(null, '', '/'));

  it('follow the ARIA tabs pattern: arrows, Home and End', async () => {
    render(<Settings />);
    const device = await screen.findByRole('tab', { name: 'Device', selected: true });
    expect(device).toHaveAttribute('tabindex', '0');
    expect(screen.getByRole('tabpanel', { name: 'Device' })).toBeInTheDocument();

    fireEvent.keyDown(device, { key: 'ArrowRight' });
    const network = await screen.findByRole('tab', { name: 'Network', selected: true });
    expect(network).toHaveFocus();
    expect(screen.getByRole('tab', { name: 'Device' })).toHaveAttribute('tabindex', '-1');

    fireEvent.keyDown(network, { key: 'End' });
    expect(await screen.findByRole('tab', { name: 'Alerts', selected: true })).toHaveFocus();
    fireEvent.keyDown(screen.getByRole('tab', { name: 'Alerts' }), { key: 'Home' });
    expect(await screen.findByRole('tab', { name: 'Device', selected: true })).toHaveFocus();
  });
});

describe('Alerts flyout', () => {
  it('takes focus when opened and gives it back to the bell on Escape', async () => {
    server.use(http.get('/api/alerts/recent', () => HttpResponse.json({ enabled: true, armed: true, alerts: [] })));
    render(<AlertsBell />);
    const bell = await screen.findByRole('button', { name: /^Alerts/ });
    fireEvent.click(bell);
    const dialog = await screen.findByRole('dialog', { name: 'Recent alerts' });
    await waitFor(() => expect(dialog.contains(document.activeElement)).toBe(true));
    fireEvent.keyDown(document, { key: 'Escape' });
    await waitFor(() => expect(screen.queryByRole('dialog')).toBeNull());
    expect(bell).toHaveFocus();
  });
});
