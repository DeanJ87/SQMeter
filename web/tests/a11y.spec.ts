import { test, expect, type Page } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';
import { readFileSync, writeFileSync } from 'fs';
import { resolve } from 'path';
import { INVENTORY, VIEWPORTS, type A11yState, type InventoryEntry } from './a11y/inventory';

// Automated WCAG 2.2 AA checks on every page of the inventory, in each of its
// states, at desktop and 320 px (spec 022 FR-003, FR-005).
//
// Known violations are listed per rule x page in tests/a11y/baseline.json.
// A violation that isn't listed fails the test; a listed one that no longer
// occurs is reported so the baseline can shrink. Refresh it (only to shrink
// it, or with a reviewed reason) with:
//   A11Y_UPDATE_BASELINE=1 npx playwright test tests/a11y.spec.ts

const BASELINE_PATH = resolve(import.meta.dirname, 'a11y/baseline.json');
const UPDATE = process.env.A11Y_UPDATE_BASELINE === '1';
const TAGS = ['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'];

type Baseline = Record<string, string[]>; // page id -> rule ids

const readBaseline = (): Baseline => JSON.parse(readFileSync(BASELINE_PATH, 'utf8'));

interface Finding {
  rule: string;
  impact: string;
  where: string;
  targets: string[];
  help: string;
}

const ready = async (page: Page, url: string) => {
  await page.goto(url);
  await expect(page.locator('main')).toBeVisible();
  await page.waitForTimeout(2500); // first ticks of the emulated device
};

// Put the page into a state before it is checked.
const enter: Record<A11yState, (page: Page, entry: InventoryEntry) => Promise<void>> = {
  default: async (page, entry) => ready(page, `./${entry.route}`),
  unsafe: async (page, entry) => {
    await ready(page, `./?scenario=rain${entry.route}`);
    await expect(page.getByText('Rain detected').first()).toBeVisible({ timeout: 15_000 });
  },
  dialog: async (page, entry) => {
    await ready(page, `./${entry.route}`);
    await page.getByRole('button', { name: /^Alerts/ }).click();
    await expect(page.getByRole('dialog')).toBeVisible();
  },
  'demo-panel': async (page, entry) => {
    await ready(page, `./${entry.route}`);
    await page.getByRole('button', { name: /Demo/ }).click();
    await expect(page.getByRole('region', { name: 'Demo controls' })).toBeVisible();
  },
  error: async (page, entry) => {
    await ready(page, `./${entry.route}`);
    await page.locator('[data-field="deviceName"]').fill('');
    await page.getByRole('button', { name: 'Save' }).click();
    await page.waitForTimeout(500);
  },
};

const check = async (page: Page, where: string): Promise<Finding[]> => {
  const results = await new AxeBuilder({ page }).withTags(TAGS).analyze();
  const findings = results.violations.map((v) => ({
    rule: v.id,
    impact: v.impact ?? 'unknown',
    where,
    targets: v.nodes.slice(0, 5).map((n) => n.target.join(' ')),
    help: v.help,
  }));
  // Reflow (WCAG 1.4.10): no sideways page scroll, at 320 px included. Tables
  // and charts scroll inside their own box, which this doesn't count.
  const overflow = await page.evaluate(() => document.documentElement.scrollWidth - document.documentElement.clientWidth);
  if (overflow > 1)
    findings.push({ rule: 'reflow', impact: 'serious', where, targets: [`page is ${overflow}px wider than the viewport`], help: 'Content must reflow without horizontal scrolling' });
  // One h1 per page (A11Y-06): the header's "SQMeter".
  const h1s = await page.locator('h1').count();
  if (h1s !== 1) findings.push({ rule: 'single-h1', impact: 'moderate', where, targets: [`${h1s} h1 elements`], help: 'A page has exactly one h1' });
  return findings;
};

test.describe.configure({ retries: 0 });

for (const entry of INVENTORY) {
  test(`a11y: ${entry.id}`, async ({ browser }) => {
    test.setTimeout(60_000 * entry.states.length);
    const findings: Finding[] = [];
    for (const viewport of VIEWPORTS) {
      for (const state of entry.states) {
        // A fresh context per state: the demo keeps its state in sessionStorage.
        const context = await browser.newContext({ viewport: { width: viewport.width, height: viewport.height }, colorScheme: 'dark' });
        const page = await context.newPage();
        await enter[state](page, entry);
        findings.push(...(await check(page, `${state} @ ${viewport.name}`)));
        await context.close();
      }
    }

    const found = [...new Set(findings.map((f) => f.rule))].sort();
    const baseline = readBaseline();
    const known = baseline[entry.id] ?? [];

    if (UPDATE) {
      const next = readBaseline();
      if (found.length) next[entry.id] = found;
      else delete next[entry.id];
      writeFileSync(BASELINE_PATH, `${JSON.stringify(Object.fromEntries(Object.entries(next).sort()), null, 2)}\n`);
    }

    const fixed = known.filter((rule) => !found.includes(rule));
    if (fixed.length) {
      const note = `${entry.id}: no longer violates ${fixed.join(', ')} - remove from tests/a11y/baseline.json`;
      test.info().annotations.push({ type: 'baseline can shrink', description: note });
      console.log(`[a11y] ${note}`);
    }

    const fresh = findings.filter((f) => !known.includes(f.rule));
    const report = fresh
      .map((f) => `  ${f.rule} (${f.impact}) on ${entry.id} [${f.where}] - ${f.help}\n    ${f.targets.join('\n    ')}`)
      .join('\n');
    if (!UPDATE) expect(fresh, `New accessibility violations on ${entry.id} (${entry.component}):\n${report}`).toEqual([]);
  });
}
