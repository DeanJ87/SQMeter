#!/usr/bin/env node
// The release notes and banner for a tag, from CHANGELOG.md.
//
//   node tools/release/notes.mjs --tag v0.3.2-beta.1            → release-notes/
//   node tools/release/notes.mjs --tag v0.3.2-beta.1 --out dir --dry-run
//   node tools/release/notes.mjs --latest --dry-run            newest version in the changelog
//
// Writes release-notes.md (the release body) and sqmeter-release-<tag>.png
// (+ @2x). The release job in .github/workflows/build.yml runs this on a tag;
// --dry-run also prints the body, for checking a changelog before tagging.
// Fails if CHANGELOG.md has no section for the tag.
//
// Needs the web dev dependencies for the banner (Playwright + Chromium):
// cd web && npm ci && npx playwright install chromium. No network access.

import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { renderReleaseBanner } from '../brand/render.mjs';
import { bannerUrl, findSection, formatDate, highlights, releaseBody, versionOf } from './changelog.mjs';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const REPOSITORY = process.env.GITHUB_REPOSITORY || 'DeanJ87/SQMeter';

function arg(name) {
  const at = process.argv.indexOf(name);
  return at === -1 ? undefined : process.argv[at + 1];
}

// The docs address lives in one place: mkdocs.yml's site_url.
function docsBase() {
  const match = /^site_url:\s*(\S+)/m.exec(readFileSync(join(ROOT, 'mkdocs.yml'), 'utf8'));
  if (!match) throw new Error('mkdocs.yml has no site_url');
  return match[1];
}

function newestVersion(changelog) {
  const match = /^## \[(\d[^\]]*)\]/m.exec(changelog);
  if (!match) throw new Error('CHANGELOG.md has no released version');
  return match[1];
}

async function main() {
  const changelog = readFileSync(resolve(arg('--changelog') ?? join(ROOT, 'CHANGELOG.md')), 'utf8');
  const tag = process.argv.includes('--latest') ? `v${newestVersion(changelog)}` : arg('--tag');
  if (!tag) throw new Error('Pass --tag vX.Y.Z[-beta.N] or --latest');
  const out = resolve(arg('--out') ?? join(ROOT, 'release-notes'));

  const section = findSection(changelog, versionOf(tag));
  if (!section.date) throw new Error(`The ${tag} heading has no date: ${section.heading}`);
  const changes = highlights(section.body);
  const body = releaseBody({ tag, section, bannerUrl: bannerUrl(REPOSITORY, tag), docsBase: docsBase() });

  mkdirSync(out, { recursive: true });
  writeFileSync(join(out, 'release-notes.md'), body);
  const files = await renderReleaseBanner({ tag, date: formatDate(section.date), changes, dir: out });

  console.log(`${tag}: highlights ${JSON.stringify(changes)}`);
  console.log(`wrote ${join(out, 'release-notes.md')}`);
  files.forEach((file) => console.log(`wrote ${file}`));
  if (process.argv.includes('--dry-run')) console.log(`\n${body}`);
}

main().catch((error) => {
  console.error(`release notes: ${error.message}`);
  process.exit(1);
});
