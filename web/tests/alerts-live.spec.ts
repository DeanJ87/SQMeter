import { test, expect, type Page } from '@playwright/test';
import { readFileSync } from 'fs';
import { resolve } from 'path';

// Alerts in every language and without a reload: custom wording saves in
// any script, the reset asks first, nothing on the alert surfaces stays in
// English, and new alerts and a new location reach an open page at once.

const I18N = resolve(import.meta.dirname, '../src/i18n');
const ENGLISH: Record<string, unknown> = JSON.parse(readFileSync(resolve(I18N, 'en.json'), 'utf8'));
const messagesFor = (code: string): Record<string, unknown> => JSON.parse(readFileSync(resolve(I18N, `locales/${code}.json`), 'utf8'));

// English texts (two or more words) that read differently in this language.
const englishLeftovers = (code: string) => {
  const messages = messagesFor(code);
  return Object.entries(ENGLISH)
    .filter(([key, en]) => typeof en === 'string' && /\S+\s+\S+/.test(en) && messages[key] !== en)
    .map(([, en]) => (en as string).replace(/\{\w+\}/g, '').trim())
    .filter((en) => en.length > 6);
};

const visibleTexts = (page: Page) =>
  page.evaluate(() => {
    const out: string[] = [];
    const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
    for (let node = walker.nextNode(); node; node = walker.nextNode()) {
      const el = node.parentElement;
      if (!el || el.closest('.demo-panel, .demo-panel-toggle, .tour-card, .tour-offer, script, style')) continue;
      const text = (node.textContent ?? '').trim();
      if (text && el.getClientRects().length) out.push(text);
    }
    for (const el of document.querySelectorAll('[aria-label], [title]')) {
      if (el.closest('.demo-panel, .demo-panel-toggle, .tour-card, .tour-offer')) continue;
      for (const attr of ['aria-label', 'title']) {
        const value = el.getAttribute(attr);
        if (value) out.push(value);
      }
    }
    return out;
  });

const openAlerts = async (page: Page, lang: string) => {
  await page.goto(`./?lang=${lang}#/settings?tab=alerts`);
  await page.locator('[data-event]').first().waitFor();
};

const editWording = async (page: Page, event: string) => {
  await page.locator(`[data-event="${event}"] button`).last().click();
  return page.locator(`[data-template="${event}"]`);
};

const savedEvents = (page: Page) => page.evaluate(async () => (await (await fetch('/api/config')).json()).alerts.events);

test.describe('custom alert wording', () => {
  const samples: Record<string, string> = { es: 'Está lloviendo', de: 'Es regnet', ar: 'إنها تمطر', ja: '雨'.repeat(80) };
  for (const [lang, title] of Object.entries(samples)) {
    test(`saves in ${lang}`, async ({ page }) => {
      await openAlerts(page, lang);
      const editor = await editWording(page, 'rain_started');
      await editor.locator('input').fill(title);
      await expect(editor.locator('.field-error')).toHaveCount(0); // 80 characters, even at 3 bytes each
      await page.locator('.save-bar button').last().click();
      await expect.poll(async () => (await savedEvents(page)).rain_started.title).toBe(title);
    });
  }

  test('reset asks first and is not a primary button', async ({ page }) => {
    await openAlerts(page, 'en');
    const editor = await editWording(page, 'rain_started');
    await editor.locator('input').fill('My own title');
    const reset = editor.getByRole('button', { name: 'Reset' });
    await expect(reset).toHaveClass(/btn-link/);
    await reset.click();
    await editor.getByRole('button', { name: 'Cancel' }).click();
    await expect(editor.locator('input')).toHaveValue('My own title');
    await editor.getByRole('button', { name: 'Reset' }).click();
    await editor.getByRole('group').getByRole('button', { name: 'Reset' }).click();
    await expect(editor.locator('input')).toHaveValue('');
  });
});

test('alert surfaces are translated', async ({ page }) => {
  const lang = 'es';
  const leftovers = englishLeftovers(lang);
  await openAlerts(page, lang);
  for (const event of ['rain_started', 'unsafe', 'sensor_fault', 'client_lost', 'dew_risk', 'clouded_over']) {
    await page.evaluate((e) => fetch(`/api/alerts/test?event=${e}&level=2`, { method: 'POST' }), event);
  }
  await page.locator('[data-event="rain_started"] button').first().click(); // a test from the page
  await expect(page.locator('[data-event="rain_started"] ~ .note, .result-note').first()).toBeVisible();
  await page.getByRole('navigation').getByRole('button').last().click(); // the bell
  await expect(page.locator('.event-list li').first()).toBeVisible();
  const texts = await visibleTexts(page);
  const english = texts.filter(
    (text) =>
      leftovers.some((en) => text.includes(en)) ||
      /^Test: |This is how a |Demo: nothing was sent|: (sent|failed|skipped)\b|: level$|: sound$/.test(text),
  );
  expect(english).toEqual([]);
});

test('a new alert reaches the bell without waiting for a poll', async ({ page }) => {
  await page.goto('./#/');
  await page.locator('main').waitFor();
  await page.waitForTimeout(1500);
  await page.evaluate(() => fetch('/api/alerts/test?event=rain_started&level=2', { method: 'POST' }));
  // The fallback poll is a minute; the pushed revision brings it in seconds.
  await expect(page.locator('.alerts-bell-count')).toBeVisible({ timeout: 5000 });
});

test('a new location reaches Sun & Moon without a reload', async ({ page }) => {
  await page.goto('./#/');
  const sunMoon = page
    .locator('section.sq-card')
    .filter({ hasText: /Moon|moon/ })
    .first();
  await expect(sunMoon).toBeVisible();
  await page.waitForTimeout(1500);
  const before = await sunMoon.innerText();
  await page.evaluate(() =>
    fetch('/api/config', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ location: { set: true, latitude: -33.8688, longitude: 151.2093 } }),
    }),
  );
  await expect.poll(async () => sunMoon.innerText(), { timeout: 6000 }).not.toBe(before);
});
