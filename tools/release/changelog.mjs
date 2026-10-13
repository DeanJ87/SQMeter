// Reads a release's section from CHANGELOG.md and turns it into release notes
// and banner highlights. Pure functions: tools/release/notes.mjs does the I/O.

const MONTHS = [
  'January',
  'February',
  'March',
  'April',
  'May',
  'June',
  'July',
  'August',
  'September',
  'October',
  'November',
  'December',
];

// Sections that describe work, not changes a user notices.
const SKIPPED_SUBSECTIONS = new Set(['planned', 'internal']);

const escapeRegex = (text) => text.replace(/[.*+?^${}()|[\]\\]/g, '\\$&');

/** "v0.3.2-beta.1" → "0.3.2-beta.1" */
export const versionOf = (tag) => tag.replace(/^v/, '');

/**
 * The section of `changelog` for `version`: the heading line, its ISO date and
 * the text up to the next `## [`. Throws, naming the versions that do exist,
 * when there's no such section (the changelog PR always comes before the tag).
 */
export function findSection(changelog, version) {
  const lines = changelog.split(/\r?\n/);
  const heading = new RegExp(`^## \\[${escapeRegex(version)}\\]`);
  const start = lines.findIndex((line) => heading.test(line));
  if (start === -1) {
    const known = lines.filter((line) => line.startsWith('## [')).map((line) => /^## \[([^\]]+)\]/.exec(line)[1]);
    throw new Error(`CHANGELOG.md has no "## [${version}]" section (it has: ${known.join(', ')}). Merge the changelog PR before tagging.`);
  }
  let end = lines.findIndex((line, at) => at > start && line.startsWith('## ['));
  if (end === -1) end = lines.length;
  const date = /(\d{4}-\d{2}-\d{2})/.exec(lines[start])?.[1] ?? null;
  return { heading: lines[start], date, body: lines.slice(start + 1, end).join('\n').trim() };
}

/** "2026-10-11" → "11 October 2026" */
export function formatDate(isoDate) {
  const match = /^(\d{4})-(\d{2})-(\d{2})$/.exec(isoDate ?? '');
  if (!match) throw new Error(`Not an ISO date: ${isoDate}`);
  const [, year, month, day] = match;
  return `${Number(day)} ${MONTHS[Number(month) - 1]} ${year}`;
}

// Plain text from a bullet: no markdown emphasis, code or links.
function plain(text) {
  return text
    .replace(/\[([^\]]+)\]\([^)]*\)/g, '$1')
    .replace(/`([^`]*)`/g, '$1')
    .replace(/\*\*([^*]*)\*\*/g, '$1')
    .replace(/\*([^*]*)\*/g, '$1')
    .trim();
}

// The headline of a bullet with no bold title: its first clause.
function firstClause(text) {
  const clause = plain(text).split(/[:.;(]| - | — /)[0].trim();
  return clause.replace(/[,\s]+$/, '');
}

/**
 * Up to `count` short highlights, in the order they're written: the bold title
 * at the start of a bullet ("- **Status card**: …"), else the bullet's first
 * clause. Bullets under Planned / Internal are skipped.
 */
export function highlights(body, count = 3) {
  let skipping = false;
  const bold = [];
  const others = [];
  for (const line of body.split('\n')) {
    const subsection = /^###\s+(.*)$/.exec(line);
    if (subsection) {
      skipping = SKIPPED_SUBSECTIONS.has(plain(subsection[1]).replace(/^[^\p{L}]+/u, '').toLowerCase());
      continue;
    }
    if (skipping) continue;
    const bullet = /^- (.+)$/.exec(line);
    if (!bullet) continue;
    const title = /^\*\*(.+?)\*\*/.exec(bullet[1]);
    if (title) bold.push(plain(title[1]).replace(/[:.]$/, '').trim());
    else others.push(firstClause(bullet[1]));
  }
  return [...bold, ...others].filter(Boolean).slice(0, count);
}

/** The release body: banner, the changelog section, how to upgrade. */
export function releaseBody({ tag, section, bannerUrl, docsBase }) {
  const docs = docsBase.endsWith('/') ? docsBase : `${docsBase}/`;
  return [
    `![SQMeter ${tag}](${bannerUrl})`,
    '',
    section.body,
    '',
    '## Upgrading',
    '',
    `- **From v0.3.x**: Updates → Check for updates (${tag.includes('-') ? 'beta' : 'stable'} track) → Update. Settings are kept.`,
    `- **From v0.2.x**: a one-time USB flash moves the device to the v0.3 layout, keeping its settings: ${docs}getting-started/usb-flash/`,
    '',
  ].join('\n');
}

/** Where the banner lives once attached to the release. */
export const bannerUrl = (repository, tag) => `https://github.com/${repository}/releases/download/${tag}/sqmeter-release-${tag}.png`;
