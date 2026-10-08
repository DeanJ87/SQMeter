import { describe, expect, it } from 'vitest';
import { deviceTime } from '../deviceTime';

describe('deviceTime', () => {
  it('reads the device clock from the status', () => {
    expect(deviceTime({ time: { iso: '2026-10-08T23:30:00Z', timezone: 'UTC0' } })?.toISOString()).toBe('2026-10-08T23:30:00.000Z');
  });

  it('is undefined before the device has the time', () => {
    expect(deviceTime({ time: { iso: '1970-01-01T00:00:12Z', timezone: 'UTC0' } })).toBeUndefined();
    expect(deviceTime({ time: { iso: '', timezone: 'UTC0' } })).toBeUndefined();
    expect(deviceTime(undefined)).toBeUndefined();
  });
});
