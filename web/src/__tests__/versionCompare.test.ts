import { describe, it, expect } from 'vitest';
import { isVersionStale } from '../utils/versionCompare';

describe('isVersionStale', () => {
  it('flags a newer patch version as stale', () => {
    expect(isVersionStale('0.0.2', 'v0.0.3')).toBe(true);
  });

  it('flags a newer minor/major version as stale', () => {
    expect(isVersionStale('0.0.9', 'v0.1.0')).toBe(true);
    expect(isVersionStale('1.9.9', 'v2.0.0')).toBe(true);
  });

  it('does not flag the same version as stale', () => {
    expect(isVersionStale('0.0.2', 'v0.0.2')).toBe(false);
  });

  it('does not flag an older release as stale', () => {
    expect(isVersionStale('0.0.3', 'v0.0.2')).toBe(false);
  });

  it('handles tags without a v prefix', () => {
    expect(isVersionStale('0.0.2', '0.0.3')).toBe(true);
  });

  it('handles differing segment counts', () => {
    expect(isVersionStale('0.0', 'v0.0.1')).toBe(true);
    expect(isVersionStale('0.0.0', 'v0.0')).toBe(false);
  });

  it('fails safe (not stale) on malformed input', () => {
    expect(isVersionStale('0.0.2', 'not-a-version')).toBe(false);
    expect(isVersionStale('garbage', 'v0.0.3')).toBe(false);
  });
});
