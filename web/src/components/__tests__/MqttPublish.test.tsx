import { describe, it, expect } from 'vitest';
import { http, HttpResponse } from 'msw';
import { fireEvent, render, screen, waitFor } from '@testing-library/preact';
import Settings from '../Settings';
import { mockConfig } from '../../mocks/data';
import { server } from '../../test/mswServer';

describe('MQTT publish settings', () => {
  it('chooses what is published and turns on Home Assistant discovery', async () => {
    let saved: any = null;
    server.use(
      http.get('/api/config', () => HttpResponse.json({ ...mockConfig, mqtt: { ...mockConfig.mqtt, enabled: true, broker: '192.168.1.5', topic: 'sqmeter' } })),
      http.post('/api/config', async ({ request }) => {
        saved = await request.json();
        return HttpResponse.json({ success: true });
      })
    );
    window.history.replaceState(null, '', '/settings?tab=network');
    render(<Settings />);

    const diagnostics = (await screen.findByLabelText('Diagnostics')) as HTMLInputElement;
    expect(diagnostics.checked).toBe(false);
    expect((screen.getByLabelText('Wind') as HTMLInputElement).checked).toBe(true);
    fireEvent.click(diagnostics);
    fireEvent.click(screen.getByLabelText('Temperature, humidity, pressure'));
    fireEvent.click(screen.getByLabelText('MQTT discovery'));
    expect(await screen.findByPlaceholderText('homeassistant')).toBeInTheDocument();

    fireEvent.click(screen.getByRole('button', { name: /save/i }));
    await waitFor(() => expect(saved).not.toBeNull());
    expect(saved.mqtt.publish.diagnostics).toBe(true);
    expect(saved.mqtt.publish.environment).toBe(false);
    expect(saved.mqtt.homeAssistant).toEqual({ enabled: true, discoveryPrefix: 'homeassistant' });
  });

  it('rejects a base topic with MQTT wildcards', async () => {
    server.use(
      http.get('/api/config', () => HttpResponse.json({ ...mockConfig, mqtt: { ...mockConfig.mqtt, enabled: true, broker: '192.168.1.5', topic: 'sqmeter' } }))
    );
    window.history.replaceState(null, '', '/settings?tab=network');
    render(<Settings />);
    const topic = await screen.findByPlaceholderText('sqmeter');
    fireEvent.input(topic, { target: { value: 'sqm/#' } });
    fireEvent.click(screen.getByRole('button', { name: /save/i }));
    expect(await screen.findByText(/with \/ between levels/)).toBeInTheDocument();
  });
});
