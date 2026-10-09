import { test, expect, type Page } from '@playwright/test';
import AxeBuilder from '@axe-core/playwright';
import { readFileSync } from 'fs';
import { resolve } from 'path';
import { INVENTORY, VIEWPORTS } from './a11y/inventory';

// Every page in every language at 320 px and 1280 px (specs/023-i18n FR-019,
// FR-023, SC-008, SC-009): the language is applied (html lang/dir and the
// navigation), longer text doesn't push the page wider than the viewport, and
// a screenshot of each page is kept with the results. Arabic also gets the
// accessibility checks right-to-left, and readings and charts stay
// left-to-right inside the page.
//
//   npx playwright test tests/i18n.spec.ts               all languages
//   I18N_LANGS=ar,de npx playwright test tests/i18n.spec.ts

const LOCALES = resolve(import.meta.dirname, '../src/i18n/locales');
const ALL = readFileSync(resolve(import.meta.dirname, '../src/i18n/languages.ts'), 'utf8')
  .match(/code: '[^']+'/g)!
  .map((m) => m.slice(7, -1))
  .filter((code) => code !== 'en');
const LANGS = process.env.I18N_LANGS ? process.env.I18N_LANGS.split(',') : ALL;
const A11Y_BASELINE: Record<string, string[]> = JSON.parse(readFileSync(resolve(import.meta.dirname, 'a11y/baseline.json'), 'utf8'));
const TAGS = ['wcag2a', 'wcag2aa', 'wcag21a', 'wcag21aa', 'wcag22aa'];
const PAGES = INVENTORY.filter((entry) => entry.id !== 'wifi'); // the captive portal has no navigation

const messagesFor = (code: string): Record<string, unknown> => JSON.parse(readFileSync(resolve(LOCALES, `${code}.json`), 'utf8'));

const open = async (page: Page, code: string, route: string) => {
  // A full load per page: a hash change alone keeps the settings tab that was open.
  await page.goto('about:blank');
  await page.goto(`./?lang=${code}${route}`);
  await expect(page.locator('main')).toBeVisible();
  await page.waitForTimeout(1200); // first ticks of the emulated device
  // Let the dashboard's cards finish moving into place.
  await page.evaluate(() =>
    Promise.all(
      document
        .getAnimations()
        .filter((animation) => animation.effect?.getTiming().iterations !== Infinity)
        .map((animation) => animation.finished.catch(() => undefined)),
    ),
  );
};

const overflow = (page: Page) => page.evaluate(() => document.documentElement.scrollWidth - document.documentElement.clientWidth);

// The innermost elements that stick out past the right edge, to say what to fix.
const culprits = (page: Page) =>
  page.evaluate(() => {
    const edge = document.documentElement.clientWidth + 1;
    // Inside a box that scrolls or clips, sticking out is fine.
    const clipped = (el: Element) => {
      for (let p = el.parentElement; p; p = p.parentElement) if (getComputedStyle(p).overflowX !== 'visible') return true;
      return false;
    };
    const out = [...document.querySelectorAll('main *')].filter((el) => el.getBoundingClientRect().right > edge && !clipped(el));
    return out
      .filter((el) => !out.some((other) => other !== el && el.contains(other)))
      .slice(0, 3)
      .map((el) => `${el.tagName.toLowerCase()}.${[...el.classList].join('.')} "${(el.textContent ?? '').trim().slice(0, 40)}"`);
  });

// Readings and charts stay left-to-right in a right-to-left page.
const ltrIslands = (page: Page) =>
  page.evaluate(() =>
    [...document.querySelectorAll('.metric-value, .metric-unit, .sqm-unit, .reading-value, main svg')]
      .filter((el) => getComputedStyle(el).direction !== 'ltr')
      .map((el) => el.className.toString() || el.tagName),
  );

test.describe.configure({ retries: 0 });

for (const code of LANGS) {
  test(`i18n: ${code}`, async ({ browser }, testInfo) => {
    test.setTimeout(30_000 + PAGES.length * VIEWPORTS.length * 6_000);
    const messages = messagesFor(code);
    const rtl = code === 'ar';
    const problems: string[] = [];
    for (const viewport of VIEWPORTS) {
      // Reduced motion: the cards don't animate while a full-page screenshot resizes the page.
      const context = await browser.newContext({
        viewport: { width: viewport.width, height: viewport.height },
        colorScheme: 'dark',
        reducedMotion: 'reduce',
      });
      const page = await context.newPage();
      for (const entry of PAGES) {
        await open(page, code, entry.route);
        const where = `${entry.id} @ ${viewport.name}`;
        const html = await page.evaluate(() => ({ lang: document.documentElement.lang, dir: document.documentElement.dir }));
        if (html.lang !== code || html.dir !== (rtl ? 'rtl' : 'ltr')) problems.push(`${where}: html lang="${html.lang}" dir="${html.dir}"`);
        const nav = (await page.getByRole('navigation').first().textContent()) ?? ''; // not innerText: CSS uppercases it on phones
        if (!nav.includes(String(messages['layout.settings'])))
          problems.push(`${where}: navigation not in ${code}: ${nav.replace(/\s+/g, ' ')}`);
        const wider = await overflow(page);
        if (wider > 1) problems.push(`${where}: page is ${wider}px wider than the viewport: ${(await culprits(page)).join(', ')}`);
        await page.screenshot({ path: testInfo.outputPath(`${code}-${viewport.name}-${entry.id}.png`), fullPage: true });
        if (!rtl) continue;
        const flipped = await ltrIslands(page);
        if (flipped.length) problems.push(`${where}: right-to-left readings or charts: ${flipped.slice(0, 5).join(', ')}`);
        const known = A11Y_BASELINE[entry.id] ?? [];
        const results = await new AxeBuilder({ page }).withTags(TAGS).analyze();
        for (const v of results.violations.filter((v) => !known.includes(v.id)))
          problems.push(
            `${where}: axe ${v.id} (${v.impact}) ${v.help}: ${v.nodes
              .slice(0, 3)
              .map((n) => n.target.join(' '))
              .join(' | ')}`,
          );
      }
      await context.close();
    }
    expect(problems, `${code}:\n  ${problems.join('\n  ')}`).toEqual([]);
  });
}
