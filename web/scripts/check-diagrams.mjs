// Parses and renders every ```mermaid diagram in docs/**/*.md and README.md
// with the same pinned mermaid the docs site serves, in headless Chromium.
// Fails naming the file, line and diagram ID (spec 024 FR-006).
//
//   npm run docs:diagrams
import { chromium } from '@playwright/test';
import { readdirSync, readFileSync, statSync } from 'node:fs';
import { createRequire } from 'node:module';
import { dirname, join, relative } from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const root = join(dirname(fileURLToPath(import.meta.url)), '..', '..');
const mermaidPath = require.resolve('mermaid/dist/mermaid.min.js');

const markdownFiles = (dir) =>
  readdirSync(dir).flatMap((name) => {
    const path = join(dir, name);
    if (statSync(path).isDirectory()) return markdownFiles(path);
    return name.endsWith('.md') ? [path] : [];
  });

// Each ```mermaid fence: its text, where it starts, and the ID from the
// metadata comment above it (tools/docs/diagrams.py checks that comment).
const diagramsIn = (file) => {
  const lines = readFileSync(file, 'utf8').split('\n');
  const found = [];
  for (let i = 0; i < lines.length; i++) {
    const open = lines[i].match(/^(\s*)```mermaid\s*$/);
    if (!open) continue;
    const indent = open[1];
    let j = i + 1;
    while (j < lines.length && lines[j].trim() !== '```') j++;
    const code = lines
      .slice(i + 1, j)
      .map((line) => (line.startsWith(indent) ? line.slice(indent.length) : line))
      .join('\n');
    let id = '?';
    for (let k = i - 1; k >= 0 && k > i - 12; k--) {
      const meta = lines[k].match(/<!--\s*diagram:\s*(\S+)/);
      if (meta) {
        id = meta[1];
        break;
      }
    }
    found.push({ where: `${relative(root, file)}:${i + 1}`, id, code });
    i = j;
  }
  return found;
};

const files = [...markdownFiles(join(root, 'docs')), join(root, 'README.md')];
const diagrams = files.flatMap(diagramsIn);

const browser = await chromium.launch(process.env.CI ? { channel: 'chrome' } : {});
const page = await browser.newPage();
await page.setContent('<!doctype html><html><body></body></html>');
await page.addScriptTag({ path: mermaidPath });
await page.evaluate(() => window.mermaid.initialize({ startOnLoad: false, securityLevel: 'strict' }));

const failures = [];
for (const [index, diagram] of diagrams.entries()) {
  const error = await page.evaluate(
    async ({ code, index }) => {
      try {
        await window.mermaid.parse(code);
        const { svg } = await window.mermaid.render(`diagram-${index}`, code);
        if (!svg.includes('<svg')) return 'rendered no SVG';
        if (svg.includes('Syntax error')) return 'rendered a syntax error';
        return null;
      } catch (e) {
        return String(e?.message ?? e).split('\n').slice(0, 4).join(' ');
      }
    },
    { code: diagram.code, index },
  );
  if (error) failures.push(`${diagram.where} ${diagram.id}: ${error}`);
}
await browser.close();

for (const failure of failures) console.error(`error: ${failure}`);
console.log(`rendered ${diagrams.length - failures.length}/${diagrams.length}`);
process.exit(failures.length ? 1 : 0);
