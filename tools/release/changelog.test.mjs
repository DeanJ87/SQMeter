// node --test tools/release/
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { test } from 'node:test';
import { bannerOverride, bannerUrl, findSection, formatDate, highlights, publishedBody, releaseBody, versionOf } from './changelog.mjs';

const CHANGELOG = `# Changelog

## [Unreleased]

### Planned

- **Enclosure**: printed case

## [0.4.0-beta.2] — 2026-11-02

An over-the-air update.

### Added

- **Restart history** on the System page: the last ten restarts and why.
- A soak test for connections (\`tools/soak\`).

### Fixed

- **N.I.N.A. losing the Safety Monitor**: browser tabs held connections.
- Long values no longer wrap.

### Internal

- **Refactor**: split a file.

## [0.4.0-beta.1] — 2026-11-01

### Fixed

- Something small.
`;

test('versionOf drops the v', () => {
  assert.equal(versionOf('v0.3.2-beta.1'), '0.3.2-beta.1');
  assert.equal(versionOf('0.3.2'), '0.3.2');
});

test('findSection returns the heading, date and body up to the next version', () => {
  const section = findSection(CHANGELOG, '0.4.0-beta.2');
  assert.equal(section.date, '2026-11-02');
  assert.match(section.body, /^An over-the-air update\./);
  assert.match(section.body, /### Internal/);
  assert.doesNotMatch(section.body, /0\.4\.0-beta\.1|Something small/);
});

test('findSection does not match a version that is only a prefix', () => {
  assert.throws(() => findSection(CHANGELOG, '0.4.0'), /no "## \[0\.4\.0\]" section/);
});

test('findSection names the versions that exist when the tag is missing', () => {
  assert.throws(() => findSection(CHANGELOG, '9.9.9'), /it has: Unreleased, 0\.4\.0-beta\.2, 0\.4\.0-beta\.1/);
});

test('the last section runs to the end of the file', () => {
  assert.equal(findSection(CHANGELOG, '0.4.0-beta.1').body, '### Fixed\n\n- Something small.');
});

test('formatDate writes the date the way the banner shows it', () => {
  assert.equal(formatDate('2026-10-11'), '11 October 2026');
  assert.equal(formatDate('2027-01-05'), '5 January 2027');
  assert.throws(() => formatDate('11/10/2026'), /Not an ISO date/);
});

test('highlights: bold titles first, Added before Fixed, fixes marked, Internal and Planned skipped', () => {
  const section = findSection(CHANGELOG, '0.4.0-beta.2');
  assert.deepEqual(highlights(section.body), ['Restart history', 'Fixed: N.I.N.A. losing the Safety Monitor', 'A soak test for connections']);
});

test('highlights puts Added before other subsections, and those before Fixed, whatever the written order', () => {
  const body = '### Fixed\n\n- **A fix**: x.\n\n### Changed\n\n- **A change**: y.\n\n### Added\n\n- **A feature**: z.';
  assert.deepEqual(highlights(body), ['A feature', 'A change', 'Fixed: A fix']);
});

test('highlights falls back to a bullet’s first clause and strips markdown', () => {
  const body = '### Changed\n\n- `Settings` switches tab when the address changes (an in-app link).\n- Long tile values: step down.';
  assert.deepEqual(highlights(body), ['Settings switches tab when the address changes', 'Long tile values']);
});

test('highlights returns fewer than three when the section is short', () => {
  assert.deepEqual(highlights('### Fixed\n\n- **One fix**: done.'), ['Fixed: One fix']);
});

test('a banner comment chooses the highlights and is left out of the published notes', () => {
  const body = 'Intro.\n\n<!-- banner: Restart history | Fixed: N.I.N.A. disconnects | API explorer | ignored -->\n\n### Added\n\n- **Something else**: x.';
  assert.deepEqual(bannerOverride(body), ['Restart history', 'Fixed: N.I.N.A. disconnects', 'API explorer', 'ignored']);
  assert.deepEqual(highlights(body), ['Restart history', 'Fixed: N.I.N.A. disconnects', 'API explorer']);
  assert.equal(publishedBody(body), 'Intro.\n\n### Added\n\n- **Something else**: x.');
  const notes = releaseBody({ tag: 'v1.0.0-beta.1', section: { body }, bannerUrl: 'x', docsBase: 'https://d/' });
  assert.doesNotMatch(notes, /banner:/);
});

test('an empty banner comment falls back to the automatic choice', () => {
  assert.equal(bannerOverride('<!-- banner: | -->\n- **A**: b'), null);
  assert.deepEqual(highlights('<!-- banner: -->\n\n### Added\n\n- **A**: b'), ['A']);
});

test('the real changelog has highlights for every released 0.3 version', () => {
  const changelog = readFileSync(new URL('../../CHANGELOG.md', import.meta.url), 'utf8');
  for (const version of ['0.3.0-beta.1', '0.3.1-beta.1', '0.3.2-beta.1']) {
    const section = findSection(changelog, version);
    assert.ok(section.date, version);
    assert.equal(highlights(section.body).length, 3, version);
  }
});

test('releaseBody has the banner, the section and how to upgrade', () => {
  const section = findSection(CHANGELOG, '0.4.0-beta.2');
  const url = bannerUrl('DeanJ87/SQMeter', 'v0.4.0-beta.2');
  assert.equal(url, 'https://github.com/DeanJ87/SQMeter/releases/download/v0.4.0-beta.2/sqmeter-release-v0.4.0-beta.2.png');
  const body = releaseBody({ tag: 'v0.4.0-beta.2', section, bannerUrl: url, docsBase: 'https://docs.example.dev' });
  assert.ok(body.startsWith(`![SQMeter v0.4.0-beta.2](${url})`));
  assert.match(body, /\*\*Restart history\*\*/);
  assert.match(body, /beta track/);
  assert.match(body, /https:\/\/docs\.example\.dev\/getting-started\/usb-flash\//);
});

test('a stable tag points at the stable track', () => {
  const section = findSection(CHANGELOG, '0.4.0-beta.1');
  assert.match(releaseBody({ tag: 'v1.0.0', section, bannerUrl: 'x', docsBase: 'https://d/' }), /stable track/);
});
