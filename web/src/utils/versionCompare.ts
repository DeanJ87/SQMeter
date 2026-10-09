type Version = { core: number[]; pre: string[] };

// "v0.2.0-beta.1+dev" -> core [0,2,0], pre ["beta","1"]; build metadata
// ("+dev", used by local builds) doesn't affect ordering.
const parse = (v: string): Version | null => {
  const match = v
    .trim()
    .replace(/^v/i, '')
    .match(/^(\d+(?:\.\d+)*)(?:-([0-9A-Za-z.-]+))?(?:\+[0-9A-Za-z.-]+)?$/);
  if (!match) return null;
  return { core: match[1].split('.').map(Number), pre: match[2] ? match[2].split('.') : [] };
};

// Numeric identifiers compare as numbers, and sort before alphanumeric ones.
const compareIdentifier = (p: string, q: string): number => {
  const pn = /^\d+$/.test(p);
  const qn = /^\d+$/.test(q);
  if (pn && qn) return Math.sign(Number(p) - Number(q));
  if (pn !== qn) return pn ? -1 : 1;
  if (p !== q) return p < q ? -1 : 1;
  return 0;
};

const compareCore = (x: number[], y: number[]): number => {
  for (let i = 0; i < Math.max(x.length, y.length); i++) {
    const d = (x[i] ?? 0) - (y[i] ?? 0);
    if (d !== 0) return Math.sign(d);
  }
  return 0;
};

// A prerelease sorts before its release; a shorter prerelease before a longer one it prefixes.
const comparePre = (x: string[], y: string[]): number => {
  if (!x.length || !y.length) return x.length === y.length ? 0 : x.length ? -1 : 1;
  for (let i = 0; i < Math.max(x.length, y.length); i++) {
    if (x[i] === undefined) return -1;
    if (y[i] === undefined) return 1;
    const d = compareIdentifier(x[i], y[i]);
    if (d !== 0) return d;
  }
  return 0;
};

// Semantic-version precedence: a prerelease sorts before its release
// (0.2.0-beta.2 < 0.2.0), numeric identifiers compare as numbers.
export const compareVersions = (a: string, b: string): number | null => {
  const x = parse(a);
  const y = parse(b);
  if (!x || !y) return null;
  return compareCore(x.core, y.core) || comparePre(x.pre, y.pre);
};

/**
 * True if the release `latestTag` (e.g. "v0.2.0-beta.2") is newer than the
 * running firmware (e.g. "0.2.0-beta.1" or a local "0.2.0-beta.1+dev").
 * Malformed input is treated as not-stale (fails safe: no update nagging).
 */
export const isVersionStale = (currentVersion: string, latestTag: string): boolean => (compareVersions(latestTag, currentVersion) ?? 0) > 0;
