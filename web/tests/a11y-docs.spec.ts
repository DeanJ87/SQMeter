import { test, expect } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';
import { readdirSync, readFileSync, statSync, writeFileSync } from 'fs';
import { join, relative, resolve } from 'path';

// WCAG 2.2 AA checks on every page of the built docs site (spec 022 FR-004),
// against tests/a11y/docs-baseline.json (rule ids per page, may only shrink).
// Run: mkdocs build --strict && npx playwright test -c playwright.docs.config.ts
// Refresh: A11Y_UPDATE_BASELINE=1 npx playwright test -c playwright.docs.config.ts

const SITE = resolve(import.meta.dirname, '../../site');
const BASELINE_PATH = resolve(import.meta.dirname, 'a11y/docs-baseline.json');
const UPDATE = process.env.A11Y_UPDATE_BASELINE === '1';
const TAGS = ['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'];
// demo/ only redirects to demo.sqmeter.dev; search/ holds the index, not a page.
const SKIP = ['demo/', 'search/'];

const pages = (dir: string): string[] =>
  readdirSync(dir).flatMap((name) => {
    const path = join(dir, name);
    if (statSync(path).isDirectory()) return pages(path);
    return name === 'index.html' ? [relative(SITE, dir).replace(/\\/g, '/') + (dir === SITE ? '' : '/')] : [];
  });

const readBaseline = (): Record<string, string[]> => JSON.parse(readFileSync(BASELINE_PATH, 'utf8'));

test.describe.configure({ retries: 0 });

for (const page of pages(SITE).filter((p) => !SKIP.some((skip) => p.startsWith(skip)))) {
  const id = page || '/';
  test(`docs a11y: ${id}`, async ({ page: browserPage }) => {
    await browserPage.goto(`./${page}`);
    const results = await new AxeBuilder({ page: browserPage }).withTags(TAGS).analyze();
    const found = [...new Set(results.violations.map((v) => v.id))].sort();
    const known = readBaseline()[id] ?? [];

    if (UPDATE) {
      const next = readBaseline();
      if (found.length) next[id] = found;
      else delete next[id];
      writeFileSync(BASELINE_PATH, `${JSON.stringify(Object.fromEntries(Object.entries(next).sort()), null, 2)}\n`);
    }

    const fixed = known.filter((rule) => !found.includes(rule));
    if (fixed.length) console.log(`[a11y] docs ${id}: no longer violates ${fixed.join(', ')} - remove from tests/a11y/docs-baseline.json`);

    const fresh = results.violations.filter((v) => !known.includes(v.id));
    const report = fresh
      .map(
        (v) =>
          `  ${v.id} (${v.impact}) - ${v.help}\n    ${v.nodes
            .slice(0, 5)
            .map((n) => n.target.join(' '))
            .join('\n    ')}`,
      )
      .join('\n');
    if (!UPDATE) expect(fresh, `New accessibility violations on docs page ${id}:\n${report}`).toEqual([]);
  });
}
