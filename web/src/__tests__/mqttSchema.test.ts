import { describe, it, expect } from 'vitest';
import { getConfigValidationErrors } from '../validation/configSchema';
import { mockConfig } from '../mocks/data';

describe('MQTT config validation', () => {
  const withMqtt = (mqtt: Record<string, unknown>) =>
    getConfigValidationErrors({ ...mockConfig, mqtt: { ...mockConfig.mqtt, enabled: true, broker: '192.168.1.5', ...mqtt } });

  it('rejects wildcards or a trailing slash in the base topic', () => {
    expect(withMqtt({ topic: 'sqm/#' })['mqtt.topic']).toMatch(/letters, numbers/);
    expect(withMqtt({ topic: 'sqm/' })['mqtt.topic']).toMatch(/letters, numbers/);
    expect(withMqtt({ topic: '/sqm' })['mqtt.topic']).toMatch(/letters, numbers/);
    expect(withMqtt({ topic: 'sqm+x' })['mqtt.topic']).toMatch(/letters, numbers/);
    expect(withMqtt({ topic: 'sqmeter/roof' })['mqtt.topic']).toBeUndefined();
  });

  it('checks the discovery prefix only when discovery is on', () => {
    expect(
      withMqtt({ topic: 'sqmeter', homeAssistant: { enabled: false, discoveryPrefix: '' } })['mqtt.homeAssistant.discoveryPrefix'],
    ).toBeUndefined();
    expect(
      withMqtt({ topic: 'sqmeter', homeAssistant: { enabled: true, discoveryPrefix: '' } })['mqtt.homeAssistant.discoveryPrefix'],
    ).toBeDefined();
  });
});
