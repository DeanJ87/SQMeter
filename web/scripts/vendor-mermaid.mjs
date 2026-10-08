// Copies the pinned mermaid build into the docs so sqmeter.dev serves it
// itself instead of Material fetching it from a CDN (spec 024 FR-004).
// The copy is generated and git-ignored; CI runs this before `mkdocs build`.
import { copyFileSync, mkdirSync, readFileSync } from 'node:fs';
import { createRequire } from 'node:module';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const source = require.resolve('mermaid/dist/mermaid.min.js');
const { version } = JSON.parse(readFileSync(join(dirname(source), '..', 'package.json'), 'utf8'));
const target = join(dirname(fileURLToPath(import.meta.url)), '..', '..', 'docs', 'assets', 'javascripts', 'vendor', 'mermaid.min.js');

mkdirSync(dirname(target), { recursive: true });
copyFileSync(source, target);
console.log(`mermaid ${version} -> ${target}`);
