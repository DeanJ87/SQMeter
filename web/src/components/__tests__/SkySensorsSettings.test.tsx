import { describe, it, expect } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor, within } from '@testing-library/preact';
import Settings from '../Settings';
import { mockConfig } from '../../mocks/data';
import { server } from '../../test/mswServer';
import { mockDevice } from '../../test/mockDevice';

// Settings -> Sensors -> Sky sensors with two sky sensors (spec 030 FR-004,
// FR-005, FR-006, FR-010, FR-013).

const skySensors = async () =>
  within(
    await waitFor(() => {
      const el = document.getElementById('sky-sensors');
      if (!el?.textContent?.includes('SQM-L')) throw new Error('not rendered');
      return el;
    }),
  );

const field = (name: string) => document.querySelector(`[data-field="${name}"]`) as HTMLInputElement | HTMLSelectElement;

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

const open = () => {
  window.history.replaceState(null, '', '/settings?tab=sensors');
  render(<Settings />);
};

describe('Sky sensors settings (spec 030)', () => {
  it('lists the SQM and SQM-L with their state, switches, pins and the authoritative choice', async () => {
    open();
    const card = await skySensors();
    const rows = [...document.querySelectorAll('#sky-sensors .reading-label')].map((row) => row.textContent);
    expect(rows.slice(0, 2)).toEqual(['SQM TSL2591', 'SQM-L TSL2591']);
    expect(card.getAllByText('OK').length).toBeGreaterThanOrEqual(2);
    expect(card.getByRole('switch', { name: 'Use SQM' })).toBeChecked();
    expect(card.getByRole('switch', { name: 'Use SQM-L' })).toBeChecked();
    expect((field('sensor.i2c2SDA') as HTMLInputElement).value).toBe('25');
    expect((field('sensor.i2c2SCL') as HTMLInputElement).value).toBe('26');
    expect(card.getByLabelText('Authoritative reading', { selector: 'select' })).not.toBeDisabled();
    expect(card.getByRole('switch', { name: 'Show other reading' })).toBeChecked();
  });

  it('saves the authoritative choice and the dashboard option', async () => {
    const saved = captureSave();
    open();
    const card = await skySensors();
    fireEvent.change(card.getByLabelText('Authoritative reading', { selector: 'select' }), { target: { value: 'sqml' } });
    fireEvent.click(card.getByRole('switch', { name: 'Show other reading' }));
    fireEvent.click(screen.getByRole('button', { name: /save/i }));
    await waitFor(() => expect(saved.body).not.toBeNull());
    expect(saved.body.sky).toEqual({ sqmEnabled: true, sqmlEnabled: true, authoritative: 'sqml', showOther: false });
  });

  it('saves empty I2C-2 pins as 255 once the SQM-L is off', async () => {
    const saved = captureSave();
    open();
    const card = await skySensors();
    fireEvent.click(card.getByRole('switch', { name: 'Use SQM-L' }));
    fireEvent.change(field('sensor.i2c2SDA'), { target: { value: '' } });
    fireEvent.change(field('sensor.i2c2SCL'), { target: { value: '' } });
    fireEvent.click(screen.getByRole('button', { name: /save/i }));
    await waitFor(() => expect(saved.body).not.toBeNull());
    expect(saved.body.sensor).toMatchObject({ i2c2SDA: 255, i2c2SCL: 255 });
    expect(saved.body.sky.sqmlEnabled).toBe(false);
  });

  it('blocks the SQM-L switch while the I2C-2 pins are unset, and unblocks it once they are typed (D-39)', async () => {
    mockDevice({
      config: { sensor: { ...mockConfig.sensor, i2c2SDA: 255, i2c2SCL: 255 }, sky: { ...mockConfig.sky!, sqmlEnabled: false } },
    });
    open();
    const card = await skySensors();
    expect(card.getByRole('switch', { name: 'Use SQM-L' })).toBeDisabled();
    expect(card.getByText('Second I2C bus pins not set.')).toBeInTheDocument();
    expect(card.getByLabelText('Authoritative reading', { selector: 'select' })).toBeDisabled();
    fireEvent.change(field('sensor.i2c2SDA'), { target: { value: '25' } });
    fireEvent.change(field('sensor.i2c2SCL'), { target: { value: '26' } });
    await waitFor(() => expect(card.getByRole('switch', { name: 'Use SQM-L' })).not.toBeDisabled());
  });

  it('names a switched-off authoritative sensor and offers the other (FR-010)', async () => {
    mockDevice({ config: { sky: { ...mockConfig.sky!, sqmlEnabled: false, authoritative: 'sqml' } } });
    open();
    const card = await skySensors();
    const note = card.getByText('SQM-L is off; no sky reading.');
    fireEvent.click(within(note).getByRole('button', { name: 'Use SQM' }));
    await waitFor(() => expect(card.queryByText('SQM-L is off; no sky reading.')).toBeNull());
    expect((card.getByLabelText('Authoritative reading', { selector: 'select' }) as HTMLSelectElement).value).toBe('sqm');
  });
});
