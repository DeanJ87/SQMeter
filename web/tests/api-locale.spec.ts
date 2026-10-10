import { test, expect, type Page } from '@playwright/test';

// Machine-readable output never changes with the UI language (specs/023-i18n
// FR-015, coding standard I18N-06): with the UI in German (decimal comma) or
// Arabic (right to left), the API and the WebSocket still send JSON numbers
// with '.' decimals, under the same field names as in English.

type Shape = Record<string, string>;

const ENDPOINTS = ['/api/sensors', '/api/status', '/api/safety'];

const open = async (page: Page, code: string) => {
  await page.goto('about:blank');
  await page.goto(`./?lang=${code}#/`);
  await expect(page.locator('html')).toHaveAttribute('lang', code);
  await expect(page.locator('main')).toBeVisible();
  await page.waitForTimeout(1500); // first ticks of the emulated device
};

/** What a client sees: each endpoint's JSON, and the first /ws/sensors message. */
const capture = (page: Page) =>
  page.evaluate(async (endpoints) => {
    const out: Record<string, { raw: string; json: unknown }> = {};
    for (const path of endpoints) {
      const raw = await (await fetch(path)).text();
      out[path] = { raw, json: JSON.parse(raw) };
    }
    const raw = await new Promise<string>((resolve, reject) => {
      const ws = new WebSocket(`${location.origin.replace(/^http/, 'ws')}/ws/sensors`);
      const timer = setTimeout(() => reject(new Error('no /ws/sensors message')), 10_000);
      ws.onmessage = (event) => {
        clearTimeout(timer);
        ws.close();
        resolve(String(event.data));
      };
    });
    out['/ws/sensors'] = { raw, json: JSON.parse(raw) };
    return out;
  }, ENDPOINTS);

/** Every field path and the JSON type of its value. Array items share one path. */
const shape = (value: unknown, path = '', into: Shape = {}): Shape => {
  if (Array.isArray(value)) value.forEach((item) => shape(item, `${path}[]`, into));
  else if (value !== null && typeof value === 'object')
    for (const [key, item] of Object.entries(value)) shape(item, path ? `${path}.${key}` : key, into);
  else into[path] = value === null ? 'null' : typeof value;
  return into;
};

/** Strings that look like a number written for a language: "12,1", "1.013,4", "-0,5". */
const LOCALISED_NUMBER = /^[-+]?\d[\d.\s]*,\d+$/;
const NUMBER_AS_STRING = /^[-+]?\d+(\.\d+)?$/;

const READINGS = [
  'sky.sqm',
  'sky.nelm',
  'light.lux',
  'environment.temperature',
  'environment.humidity',
  'environment.pressure',
  'environment.dewpoint',
  'infrared.skyTemperature',
  'clouds.coverPercent',
  'timestamp',
];

let english: Awaited<ReturnType<typeof capture>>;

test.beforeAll(async ({ browser, baseURL }) => {
  const page = await browser.newPage({ baseURL });
  await open(page, 'en');
  english = await capture(page);
  await page.close();
});

for (const code of ['de', 'ar']) {
  test(`${code}: the API and WebSocket are the same as in English`, async ({ page }) => {
    await open(page, code);
    const localised = await capture(page);

    for (const [source, { raw, json }] of Object.entries(localised)) {
      const fields = shape(json);

      // Same field names, same JSON types: numbers stay numbers.
      expect(Object.keys(fields).sort(), `${source}: field names`).toEqual(Object.keys(shape(english[source].json)).sort());
      expect(fields, `${source}: value types`).toEqual(shape(english[source].json));

      for (const [path, type] of Object.entries(fields)) {
        if (type !== 'string') continue;
        const value = path.split('.').reduce<unknown>((node, key) => (node as Record<string, unknown>)?.[key.replace('[]', '')], json);
        if (typeof value !== 'string') continue; // inside an array: checked through the raw text below
        expect(value, `${source} ${path}: decimal comma`).not.toMatch(LOCALISED_NUMBER);
        expect(value, `${source} ${path}: number sent as a string`).not.toMatch(NUMBER_AS_STRING);
      }
      // No number anywhere in the text is written with a decimal comma.
      expect(raw, `${source}: "12,1"-style value`).not.toMatch(/"[-+]?\d+,\d+"/);
      expect(raw, `${source}: Arabic-Indic digits`).not.toMatch(/[٠-٩۰-۹]/);
    }

    const readings = [localised['/api/sensors'].json, localised['/ws/sensors'].json];
    for (const doc of readings)
      for (const path of READINGS) {
        const value = path.split('.').reduce<unknown>((node, key) => (node as Record<string, unknown>)?.[key], doc);
        expect(typeof value, `${path} is a JSON number`).toBe('number');
      }
    expect(typeof (localised['/api/safety'].json as { reasonFlags: unknown }).reasonFlags).toBe('number');
    expect(typeof (localised['/api/safety'].json as { safe: unknown }).safe).toBe('boolean');
  });
}
