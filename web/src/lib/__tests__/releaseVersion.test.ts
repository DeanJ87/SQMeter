import { readFileSync } from 'fs';
import { resolve } from 'path';
import { describe, expect, it } from 'vitest';
import { latestReleasedVersion } from '../releaseVersion';
import { DEMO_VERSION } from '../../demo/device';

describe('latestReleasedVersion', () => {
  it('takes the first released section, skipping Unreleased', () => {
    const changelog = '# Changelog\n\n## [Unreleased]\n\n- x\n\n## [0.4.0-beta.2] — 2026-11-01\n\n## [0.4.0-beta.1] — 2026-10-20\n';
    expect(latestReleasedVersion(changelog)).toBe('0.4.0-beta.2');
  });

  it('reads stable versions', () => {
    expect(latestReleasedVersion('## [1.0.0] — 2027-01-01\n')).toBe('1.0.0');
  });

  it('throws when nothing is released', () => {
    expect(() => latestReleasedVersion('# Changelog\n\n## [Unreleased]\n')).toThrow(/no released/);
  });

  it('is what the demo device reports', () => {
    const changelog = readFileSync(resolve(__dirname, '../../../../CHANGELOG.md'), 'utf8');
    expect(DEMO_VERSION).toBe(latestReleasedVersion(changelog));
  });
});
