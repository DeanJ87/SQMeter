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

// A changelog section can choose the banner's highlights itself:
//   <!-- banner: Restart history | Fixed: N.I.N.A. disconnects | API explorer -->
// The comment is removed from the published notes.
const BANNER_OVERRIDE = /^[ \t]*<!--\s*banner:([\s\S]*?)-->[ \t]*\n?/m;

/** The section's own banner highlights, if it sets them. */
export function bannerOverride(body) {
  const match = BANNER_OVERRIDE.exec(body);
  if (!match) return null;
  const items = match[1]
    .split('|')
    .map((item) => item.trim())
    .filter(Boolean);
  return items.length ? items : null;
}

/** The section as published: without the banner override comment. */
export const publishedBody = (body) => body.replace(BANNER_OVERRIDE, '').replace(/\n{3,}/g, '\n\n').trim();

// Which subsection a highlight comes from: new things first, fixes last.
function rankOf(subsection) {
  if (subsection === 'added') return 0;
  if (subsection === 'fixed') return 2;
  return 1;
}

/**
 * Up to `count` short highlights for the banner. A `<!-- banner: a | b | c -->`
 * comment in the section wins. Otherwise: the bold titles at the start of
 * bullets ("- **Status card**: …"), Added first, then other subsections, then
 * Fixed (as "Fixed: …"); bullets without a bold title fill any gap with their
 * first clause, in the same order. Planned / Internal are skipped.
 */
export function highlights(body, count = 3) {
  const chosen = bannerOverride(body);
  if (chosen) return chosen.slice(0, count);
  let subsection = '';
  const items = [];
  body.split('\n').forEach((line, order) => {
    const heading = /^###\s+(.*)$/.exec(line);
    if (heading) {
      subsection = plain(heading[1]).replace(/^[^\p{L}]+/u, '').toLowerCase();
      return;
    }
    const bullet = /^- (.+)$/.exec(line);
    if (!bullet || SKIPPED_SUBSECTIONS.has(subsection)) return;
    const title = /^\*\*(.+?)\*\*/.exec(bullet[1]);
    const text = title ? plain(title[1]).replace(/[:.]$/, '').trim() : firstClause(bullet[1]);
    if (!text) return;
    const rank = rankOf(subsection);
    items.push({ text: rank === 2 ? `Fixed: ${text}` : text, bold: Boolean(title), rank, order });
  });
  items.sort((a, b) => Number(b.bold) - Number(a.bold) || a.rank - b.rank || a.order - b.order);
  return items.slice(0, count).map((item) => item.text);
}

/** The release body: banner, the changelog section, how to upgrade. */
export function releaseBody({ tag, section, bannerUrl, docsBase }) {
  const docs = docsBase.endsWith('/') ? docsBase : `${docsBase}/`;
  return [
    `![SQMeter ${tag}](${bannerUrl})`,
    '',
    publishedBody(section.body),
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
