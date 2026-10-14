// The version the demo device reports: the newest released section of
// CHANGELOG.md, read at build time (vite.demo.config.ts, vitest.config.ts) so
// it never goes stale. Node-free so the unit test can feed it any text.

const RELEASED_HEADING = /^## \[(\d+\.\d+\.\d+(?:-[0-9A-Za-z.-]+)?)\]/;

/** "## [0.3.3-beta.1] — 2026-10-13" → "0.3.3-beta.1"; skips "## [Unreleased]". */
export function latestReleasedVersion(changelog: string): string {
  for (const line of changelog.split(/\r?\n/)) {
    const match = RELEASED_HEADING.exec(line);
    if (match) return match[1];
  }
  throw new Error('CHANGELOG.md has no released "## [x.y.z]" section');
}
