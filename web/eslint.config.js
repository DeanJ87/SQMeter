// Web UI lint (coding standard: NAME-05, STRUCT-04/05, SMELL-11/18, LIMIT-01..05,
// ERR-01, EXC-01). Formatting is Prettier's job, not ESLint's.
import js from '@eslint/js';
import tseslint from 'typescript-eslint';
import reactHooks from 'eslint-plugin-react-hooks';
import comments from '@eslint-community/eslint-plugin-eslint-comments/configs';
import prettier from 'eslint-config-prettier';
import globals from 'globals';

export default tseslint.config(
  { ignores: ['dist', 'dist-demo', 'coverage', 'public', 'src/demo/core', 'playwright-report', 'test-results'] },
  js.configs.recommended,
  ...tseslint.configs.recommended,
  comments.recommended,
  {
    files: ['**/*.{ts,tsx}'],
    languageOptions: { globals: { ...globals.browser } },
    plugins: { 'react-hooks': reactHooks },
    rules: {
      ...reactHooks.configs.recommended.rules,
      // EXC-01: a suppression says why.
      '@eslint-community/eslint-comments/require-description': ['error', { ignore: [] }],
      '@eslint-community/eslint-comments/no-unlimited-disable': 'error',
      // SMELL-18, SMELL-11, ERR-01
      '@typescript-eslint/no-explicit-any': 'error',
      '@typescript-eslint/no-unused-vars': ['error', { argsIgnorePattern: '^_', varsIgnorePattern: '^_' }],
      'no-empty': ['error', { allowEmptyCatch: false }],
      // LIMIT-01..05
      'max-lines': ['error', { max: 400, skipBlankLines: true, skipComments: true }],
      'max-lines-per-function': ['error', { max: 60, skipBlankLines: true, skipComments: true }],
      complexity: ['error', 15],
      'max-params': ['error', 5],
      'max-depth': ['error', 4],
      // NAME-05
      '@typescript-eslint/naming-convention': [
        'error',
        { selector: 'typeLike', format: ['PascalCase'] },
        { selector: 'variable', modifiers: ['const', 'global'], format: ['camelCase', 'PascalCase', 'UPPER_CASE'] },
        { selector: 'variable', format: ['camelCase', 'PascalCase'], leadingUnderscore: 'allow' },
        { selector: 'function', format: ['camelCase', 'PascalCase'] },
        { selector: 'parameter', format: ['camelCase', 'PascalCase'], leadingUnderscore: 'allow' },
      ],
      // STRUCT-05: only main.tsx and tests use the mocks and the demo.
      'no-restricted-imports': [
        'error',
        { patterns: [{ group: ['**/mocks/*', '**/demo/*'], message: 'STRUCT-05: only main.tsx and tests import mocks/ or demo/' }] },
      ],
    },
  },
  {
    // Components render; the data layer (hooks/, lib/) talks to the device. STRUCT-04
    files: ['src/components/**/*.tsx'],
    ignores: ['src/components/**/__tests__/**'],
    rules: {
      'max-lines-per-function': ['error', { max: 250, skipBlankLines: true, skipComments: true }],
      'no-restricted-globals': [
        'error',
        { name: 'fetch', message: 'STRUCT-04: call the device through the data layer (hooks/ or lib/)' },
        { name: 'WebSocket', message: 'STRUCT-04: use the data layer (hooks/useWebSocket)' },
        { name: 'XMLHttpRequest', message: 'STRUCT-04: use the data layer (lib/)' },
      ],
    },
  },
  {
    // I18N-05: numbers and dates follow the active language (spec 023 FR-017):
    // format with src/i18n/format.ts, read typed numbers with src/i18n/parse.ts.
    files: ['src/components/**/*.{ts,tsx}', 'src/demo/**/*.{ts,tsx}'],
    ignores: ['**/__tests__/**', 'src/demo/core/**'],
    rules: {
      'no-restricted-syntax': [
        'error',
        {
          selector: 'CallExpression[callee.property.name=/^(toFixed|toPrecision|toLocaleString|toLocaleTimeString|toLocaleDateString)$/]',
          message: 'I18N-05: format numbers and dates with src/i18n/format.ts (language-aware, Latin digits); SVG geometry: lib/svg.ts',
        },
        {
          selector: 'CallExpression[callee.name=/^(parseFloat|parseInt)$/]',
          message: 'I18N-05: read typed numbers with parseNumber (src/i18n/parse.ts); option values: Number(value)',
        },
        {
          selector: "CallExpression[callee.object.name='Number'][callee.property.name=/^(parseFloat|parseInt)$/]",
          message: 'I18N-05: read typed numbers with parseNumber (src/i18n/parse.ts); option values: Number(value)',
        },
      ],
    },
  },
  {
    files: ['src/main.tsx', 'src/demo/**', 'src/mocks/**', '**/__tests__/**', 'src/test/**', 'tests/**'],
    rules: { 'no-restricted-imports': 'off' },
  },
  {
    files: ['**/__tests__/**', 'src/test/**', 'tests/**'],
    rules: { 'max-lines-per-function': 'off', '@typescript-eslint/no-explicit-any': 'off' },
  },
  {
    // Build scripts run in Node (docs diagrams, spec 024); code passed to
    // page.evaluate() runs in the browser.
    files: ['scripts/**/*.mjs'],
    languageOptions: { globals: { ...globals.node, ...globals.browser } },
  },
  prettier,
);
