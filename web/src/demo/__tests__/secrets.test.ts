// @vitest-environment node
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import { demoDevice } from '../device';

// The demo answers GET /api/secrets with the firmware's own rules
// (specs/037-share-secrets, lib/ConfigModel SharedSecrets in the device core).
// The demo has no login, so step 3 of the contract is skipped.

const save = (settings: Record<string, unknown>) => expect(demoDevice.applyConfig(JSON.stringify(settings)).status).toBe(200);

describe('demo password sharing', () => {
  beforeAll(async () => {
    await demoDevice.start();
  });
  afterAll(() => demoDevice.stop());

  it('is off by default', () => {
    expect(demoDevice.secretsRequest()).toEqual({ status: 403, body: '{"error":"Password sharing is off in SQMeter"}' });
  });

  it('needs a device password', () => {
    save({ sharing: { secrets: true }, auth: { enabled: false } });
    expect(demoDevice.secretsRequest()).toEqual({
      status: 403,
      body: '{"error":"Set a web password in SQMeter before sharing passwords"}',
    });
  });

  it('shares the four values, empty when unset', () => {
    save({
      sharing: { secrets: true },
      auth: { enabled: true, username: 'admin', password: 'demo-pass' },
      mqtt: { password: 'broker-pass' },
    });
    const answer = demoDevice.secretsRequest();
    expect(answer.status).toBe(200);
    expect(JSON.parse(answer.body)).toEqual({
      mqtt: { password: 'broker-pass' },
      alerts: { ntfy: { token: '' }, pushover: { userKey: '', appToken: '' } },
    });
    save({ sharing: { secrets: false }, auth: { enabled: false } });
  });
});
