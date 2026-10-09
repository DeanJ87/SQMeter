// Copies the pinned ESP Web Tools build into the docs so sqmeter.dev serves
// the browser flasher itself, like mermaid (spec 024 FR-004; spec 027
// FR-019). The copy is generated and git-ignored; CI runs this before
// `mkdocs build`.
import { cpSync, mkdirSync, readFileSync, rmSync } from 'node:fs';
import { createRequire } from 'node:module';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const require = createRequire(import.meta.url);
const pkgJson = require.resolve('esp-web-tools/package.json');
const { version } = JSON.parse(readFileSync(pkgJson, 'utf8'));
const source = join(dirname(pkgJson), 'dist', 'web');
const target = join(dirname(fileURLToPath(import.meta.url)), '..', '..', 'docs', 'assets', 'javascripts', 'vendor', 'esp-web-tools');

rmSync(target, { recursive: true, force: true });
mkdirSync(target, { recursive: true });
cpSync(source, target, { recursive: true });
console.log(`esp-web-tools ${version} -> ${target}`);
