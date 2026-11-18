// @vitest-environment node
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { demoDevice } from '../device';

// The demo answers the safety hold routes with the firmware's own rules
// (specs/035-safety-holds, lib/DeviceCore DeviceHolds.cpp in the device core).

const safety = () => JSON.parse(demoDevice.safety());
const tick = () => new Promise((resolve) => setTimeout(resolve, 1100));

describe('demo safety holds', () => {
  beforeAll(async () => {
    await demoDevice.start();
    await tick();
  });
  afterAll(() => demoDevice.stop());

  it('a hold makes it unsafe and is listed with its reason', async () => {
    const set = demoDevice.holdRequest(
      'POST',
      '/api/safety/hold',
      JSON.stringify({ id: 'smoke', reason: 'Smoke: test' }),
      'Home Assistant',
    );
    expect(set.status).toBe(200);
    expect(JSON.parse(set.body)).toMatchObject({ id: 'smoke', reason: 'Smoke: test', source: 'Home Assistant' });
    await tick();
    const doc = safety();
    expect(doc.safe).toBe(false);
    expect(doc.reasons).toContain('Smoke: test');
    expect(doc.reasonFlags & (1 << 17)).not.toBe(0);
    expect(doc.holds.map((h: { id: string }) => h.id)).toEqual(['smoke']);
  });

  it('clears that hold, then answers 404 for it', () => {
    expect(demoDevice.holdRequest('DELETE', '/api/safety/hold/smoke', '', '')).toEqual({ status: 200, body: '{"cleared":true}' });
    const again = demoDevice.holdRequest('POST', '/api/safety/hold/smoke/clear', '', '');
    expect(again.status).toBe(404);
    expect(JSON.parse(again.body)).toEqual({ cleared: false, reason: 'no such hold' });
    expect(safety().holds).toEqual([]);
  });

  it('refuses a bad id', () => {
    const bad = demoDevice.holdRequest('POST', '/api/safety/hold', JSON.stringify({ id: 'Bad Id', reason: 'x' }), '');
    expect(bad.status).toBe(400);
  });

  it('the manual hold is Force unsafe', () => {
    expect(demoDevice.holdRequest('POST', '/api/safety/hold', JSON.stringify({ id: 'manual' }), 'ui').status).toBe(200);
    expect(demoDevice.rawConfig().alpaca?.manualOverrideUnsafe).toBe(true);
    demoDevice.holdRequest('DELETE', '/api/safety/hold/manual', '', '');
    expect(demoDevice.rawConfig().alpaca?.manualOverrideUnsafe).toBe(false);
  });
});
