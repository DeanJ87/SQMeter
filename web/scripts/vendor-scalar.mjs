// Copies the pinned Scalar build into the docs so sqmeter.dev renders the
// API reference itself, like mermaid (spec 028 FR-005, spec 024 FR-004).
// The copy is generated and git-ignored; CI runs this before `mkdocs build`.
import { copyFileSync, mkdirSync, readFileSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

// The package's exports don't include package.json, so read it by path.
const web = join(dirname(fileURLToPath(import.meta.url)), '..');
const pkgDir = join(web, 'node_modules', '@scalar', 'api-reference');
const { version } = JSON.parse(readFileSync(join(pkgDir, 'package.json'), 'utf8'));
const source = join(pkgDir, 'dist', 'browser', 'standalone.js');
const target = join(web, '..', 'docs', 'assets', 'javascripts', 'vendor', 'scalar.js');

mkdirSync(dirname(target), { recursive: true });
copyFileSync(source, target);
console.log(`@scalar/api-reference ${version} -> ${target}`);
