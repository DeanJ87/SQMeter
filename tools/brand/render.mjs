#!/usr/bin/env node
// Renders the brand SVGs that contain text (social cards, banners, stickers) to
// PNG and print PDF with the project's own fonts, so they look right where
// Inter / JetBrains Mono aren't installed (GitHub, chat apps, print shops).
//
//   node tools/brand/render.mjs                  everything into brand/render/
//   node tools/brand/render.mjs --only social    one group (social | icons | sticker)
//   node tools/brand/render.mjs --release v0.3.2-beta.1 --date "11 October 2026" \
//        --change "Connections" --change "WiFi" --change "API explorer"
//                                                a release banner (brand/render/release/)
//
// tools/release/notes.mjs uses renderReleaseBanner() to make the banner from
// CHANGELOG.md when a release is tagged.
//
// Needs the web dev dependencies (Playwright + Chromium): cd web && npm ci &&
// npx playwright install chromium. No network access at render time.

import { mkdirSync, readFileSync, readdirSync, writeFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';

const ROOT = resolve(dirname(fileURLToPath(import.meta.url)), '../..');
const require = createRequire(join(ROOT, 'web/package.json'));
const { chromium } = require('playwright');

const SRC = join(ROOT, 'brand/src');
const OUT = join(ROOT, 'brand/render');
const FONTS = join(ROOT, 'docs/assets/fonts');

// Print sizes: the sticker SVGs use 10 units per millimetre.
const UNITS_PER_MM = 10;
const PRINT_DPI = 600;

function fontFaces() {
  const face = (family, file) =>
    `@font-face{font-family:'${family}';src:url(data:font/woff2;base64,${readFileSync(join(FONTS, file)).toString('base64')}) format('woff2');font-weight:100 900;}`;
  return face('Inter', 'inter-sqm.woff2') + face('JetBrains Mono', 'jetbrains-mono-sqm.woff2');
}

function svgSize(svg) {
  const width = Number(/<svg[^>]*\swidth="([\d.]+)"/.exec(svg)?.[1]);
  const height = Number(/<svg[^>]*\sheight="([\d.]+)"/.exec(svg)?.[1]);
  if (!width || !height) throw new Error('SVG needs numeric width and height');
  return { width, height };
}

function page(svg) {
  return `<!doctype html><html><head><meta charset="utf-8"><style>${fontFaces()}html,body{margin:0;padding:0;background:transparent}svg{display:block}</style></head><body>${svg}</body></html>`;
}

async function renderPng(browser, svg, file, scale, prepare) {
  const { width, height } = svgSize(svg);
  const context = await browser.newContext({ viewport: { width, height }, deviceScaleFactor: scale });
  const tab = await context.newPage();
  await tab.setContent(page(svg));
  await tab.evaluate(() => document.fonts.ready);
  if (prepare) await tab.evaluate(prepare);
  await tab.locator('svg').first().screenshot({ path: file, omitBackground: true });
  await context.close();
}

async function renderPdf(browser, svg, file) {
  const { width, height } = svgSize(svg);
  const context = await browser.newContext();
  const tab = await context.newPage();
  await tab.setContent(page(svg.replace(/<svg([^>]*)\swidth="[\d.]+"\s+height="[\d.]+"/, `<svg$1 width="${width / UNITS_PER_MM}mm" height="${height / UNITS_PER_MM}mm"`)));
  await tab.evaluate(() => document.fonts.ready);
  await tab.pdf({ path: file, width: `${width / UNITS_PER_MM}mm`, height: `${height / UNITS_PER_MM}mm`, printBackground: true, pageRanges: '1' });
  await context.close();
}

function arg(name) {
  const at = process.argv.indexOf(name);
  return at === -1 ? undefined : process.argv[at + 1];
}

function args(name) {
  return process.argv.flatMap((value, at) => (process.argv[at - 1] === name ? [value] : []));
}

// Space for each text on the release banner, in SVG units: the version and the
// date sit left of the divider (x 720), the highlights right of it.
const RELEASE_FIT = { 'rel-tag': 618, 'rel-date': 618, 'rel-c0': 432, 'rel-c1': 432, 'rel-c2': 432 };

function releaseSvg(tag, date, changes) {
  let svg = readFileSync(join(SRC, 'social/sqmeter-release-banner-template.svg'), 'utf8');
  const escape = (text) => text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
  svg = svg.replace(/<text([^>]*)>v1\.4\.0</, `<text id="rel-tag"$1>${escape(tag)}<`);
  svg = svg.replace(/<text([^>]*)>Released [^<]*</, `<text id="rel-date"$1>Released ${escape(date)}<`);
  ['Change one', 'Change two', 'Change three'].forEach((placeholder, at) => {
    svg = svg.replace(new RegExp(`<text([^>]*)>${placeholder}<`), `<text id="rel-c${at}"$1>${escape(changes[at] ?? '')}<`);
  });
  return svg;
}

// Runs in the page: shrinks the version until it fits, and shortens any other
// text that doesn't at a word boundary with an ellipsis. Measured with the real
// fonts, so nothing overflows whatever the tag or the highlights are.
function fitReleaseText(limits) {
  for (const [id, limit] of Object.entries(limits)) {
    const text = document.getElementById(id);
    if (!text || text.getComputedTextLength() <= limit) continue;
    if (id === 'rel-tag') {
      let size = Number(text.getAttribute('font-size'));
      while (size > 24 && text.getComputedTextLength() > limit) text.setAttribute('font-size', String(--size));
      continue;
    }
    let words = text.textContent.split(' ');
    while (words.length > 1 && text.getComputedTextLength() > limit) {
      words = words.slice(0, -1);
      text.textContent = `${words.join(' ').replace(/[,;:.-]+$/, '')}…`;
    }
    while (text.textContent.length > 1 && text.getComputedTextLength() > limit) {
      text.textContent = `${text.textContent.slice(0, -2)}…`;
    }
  }
}

/**
 * Renders the release banner for `tag` into `dir` as sqmeter-release-<tag>.png
 * (1280×320) and @2x. Returns the two file paths.
 */
export async function renderReleaseBanner({ tag, date, changes, dir }) {
  mkdirSync(dir, { recursive: true });
  const svg = releaseSvg(tag, date, changes);
  const files = [join(dir, `sqmeter-release-${tag}.png`), join(dir, `sqmeter-release-${tag}@2x.png`)];
  const prepare = `(${fitReleaseText.toString()})(${JSON.stringify(RELEASE_FIT)})`;
  const browser = await chromium.launch();
  try {
    await renderPng(browser, svg, files[0], 1, prepare);
    await renderPng(browser, svg, files[1], 2, prepare);
  } finally {
    await browser.close();
  }
  return files;
}

async function main() {
  const only = arg('--only');
  const release = arg('--release');
  if (release) {
    const date = arg('--date') ?? new Date().toISOString().slice(0, 10);
    await renderReleaseBanner({ tag: release, date, changes: args('--change'), dir: join(OUT, 'release') });
    console.log(`rendered release banner for ${release}`);
    return;
  }
  const browser = await chromium.launch();
  try {
    if (!only || only === 'social') {
      const dir = join(OUT, 'social');
      mkdirSync(dir, { recursive: true });
      for (const name of readdirSync(join(SRC, 'social')).filter((f) => f.endsWith('.svg') && !f.includes('template'))) {
        const svg = readFileSync(join(SRC, 'social', name), 'utf8');
        const base = name.replace(/\.svg$/, '');
        await renderPng(browser, svg, join(dir, `${base}.png`), 1);
        await renderPng(browser, svg, join(dir, `${base}@2x.png`), 2);
        console.log(`social/${base}`);
      }
    }
    if (!only || only === 'icons') {
      const dir = join(OUT, 'icons');
      mkdirSync(dir, { recursive: true });
      const icons = [
        ['app-icon/sqmeter-app-icon-512.svg', 'apple-touch-icon-180.png', 180],
        ['favicon/favicon-32.svg', 'favicon-32.png', 32],
      ];
      for (const [source, file, pixels] of icons) {
        const svg = readFileSync(join(SRC, source), 'utf8');
        await renderPng(browser, svg, join(dir, file), pixels / svgSize(svg).width);
        console.log(`icons/${file}`);
      }
    }
    if (!only || only === 'sticker') {
      const dir = join(OUT, 'print');
      mkdirSync(dir, { recursive: true });
      for (const name of readdirSync(join(SRC, 'sticker')).filter((f) => f.endsWith('.svg'))) {
        const svg = readFileSync(join(SRC, 'sticker', name), 'utf8');
        const base = name.replace(/\.svg$/, '');
        await renderPng(browser, svg, join(dir, `${base}-${PRINT_DPI}dpi.png`), PRINT_DPI / 25.4 / UNITS_PER_MM);
        await renderPdf(browser, svg, join(dir, `${base}.pdf`));
        console.log(`print/${base}`);
      }
    }
  } finally {
    await browser.close();
  }
  writeFileSync(join(OUT, 'README.md'), '# Rendered brand files\n\nGenerated by `node tools/brand/render.mjs` from `brand/src/`. Do not edit by hand.\n');
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  main().catch((error) => {
    console.error(error);
    process.exit(1);
  });
}
