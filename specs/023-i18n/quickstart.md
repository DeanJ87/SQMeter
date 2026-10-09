# Quickstart: verify translations

1. `node tools/i18n/check.mjs` - every language complete, placeholders and plurals right (exit 0). `--review` prints the review flags.
2. `node tools/i18n/literals.mjs` - no hard-coded UI text (exit 0). Add `<p>Hello there</p>` to a component: it fails, naming the file and line.
3. `python3 tools/i18n/gen_device_catalog.py --check` - device catalogue in sync with `en.json`, and every device literal has a template.
4. `pio test -e native` - `test_messages`, `test_language_logic`, config `language` validation.
5. `cd web && npx vitest run` and `npx playwright test tests/i18n.spec.ts` - every page per language at 320 and 1280 px with no overflow; Arabic right-to-left with axe clean.
6. Demo: `npm run build:demo && npm run preview:demo`; Settings → Device → Language → Español: the UI turns Spanish and no request leaves the page.
7. Device (spare): choose Español - the UI is Spanish within 10 s and `GET /api/i18n` shows `installed`; choose English - the file is deleted; upload `sqmeter-i18n-fr.json.gz` while offline - French.
