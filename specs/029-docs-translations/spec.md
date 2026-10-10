# Feature Specification: Docs site in the UI's languages

**Feature Branch**: `spec/029-docs-translations`

**Created**: 2026-10-10

**Status**: Draft, for review.

## Context

The web UI ships in English plus 13 languages (spec 023), translated with AI against per-language
glossaries (`tools/i18n/glossary/`) and a review pass, with a record of the English each translation
was made from (`tools/i18n/record.json`). The docs site (sqmeter.dev, MkDocs with the Material
theme, about 39 pages) is English only, so a user who reads the UI in Spanish lands on English help.
This spec adds the same languages to the docs, reusing the UI's translation workflow and glossaries,
so terms match between the UI and its help.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Read the docs in my language (Priority: P1)

A user who reads the UI in German opens sqmeter.dev and switches the site to Deutsch with a language
selector in the header; every page they open is in German, with English shown for any page not yet
translated, clearly marked.

**Independent Test**: build the site; open `/de/` and three linked pages; all are German; a page with
no German translation shows English with a notice and a link to the English original.

**Acceptance Scenarios**:

1. **Given** the site, **When** the user picks a language, **Then** the same page opens in that
   language (not the home page), and the choice persists while browsing.
2. **Given** a page without a translation, **Then** English is shown with a short translated notice.
3. **Given** Arabic, **Then** the layout is right-to-left like the UI (spec 023), with code, units,
   coordinates and diagrams kept left-to-right.

---

### User Story 2 - Translations stay current (Priority: P1)

A contributor edits an English page. The translations of that page are marked stale until they are
regenerated; CI reports stale pages and the site shows the notice on them, so nobody reads outdated
instructions as current.

**Independent Test**: change one paragraph of an English page; the check lists that page as stale in
all 13 languages; regenerating clears it.

**Acceptance Scenarios**:

1. **Given** a translated page, **Then** it records the hash of the English source it was made from.
2. **Given** the English source changed, **Then** the check marks the translation stale; the release
   build shows the stale notice on those pages (the build doesn't fail, so docs fixes are never
   blocked by translations).
3. **Given** the translation tool, **When** it is run, **Then** it re-translates only stale or missing
   pages, using the UI glossaries and the same review pass as the UI strings.

---

### User Story 3 - Same words as the UI (Priority: P2)

Settings paths, button names and sensor names in the docs match the UI in each language (e.g.
"Settings → Sensors" reads exactly as the German UI labels it).

**Independent Test**: the docs UI-path check (DS-28) runs per language against the UI's translated
labels and passes.

---

### Edge Cases

- Code blocks, API paths, JSON, MQTT topics, units and command lines are never translated (spec 023's
  machine-readable rule applies to docs examples too).
- Diagrams (spec 024): Mermaid text labels may be translated; diagram fingerprints and the stale check
  apply per language; captions and the "diagram in words" text are translated.
- Screenshots stay English at first (see Assumptions); alt text is translated.
- Search works per language (Material's search language setting).
- The demo (demo.sqmeter.dev) links to the docs in the demo's current language.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The docs MUST be built in English plus the 13 UI languages (id, es, fr, it, de, nl, ar,
  pt-BR, pl, ja, zh-Hans, ko, tr) with a language selector, one URL prefix per language (e.g. `/de/`),
  and English at the root.
- **FR-002**: Missing translations MUST fall back to English with a translated notice; stale ones MUST
  show a notice too.
- **FR-003**: Each translation MUST record the hash of its English source; a check MUST list stale and
  missing pages per language, and run in CI as a report (warning), not a failure.
- **FR-004**: The translation tool MUST reuse `tools/i18n` (glossaries, review pass, record), translate
  prose only, and leave code, paths, units and machine examples untouched.
- **FR-005**: Arabic MUST render right-to-left with LTR islands for code and numbers.
- **FR-006**: The DS-28 docs UI-path check MUST run per language against the UI's translations.
- **FR-007**: The docs accessibility checks (spec 022) and the no-third-party-requests rule (spec 024)
  MUST pass for every language.
- **FR-008**: The build MUST stay within CI time limits (a target is in the success criteria), e.g. by
  building languages in parallel or only changed languages on pull requests.

### Key Entities

- **Docs page translation**: a translated Markdown page plus the hash of the English source it came
  from.
- **Stale report**: per language, the pages missing or older than their English source.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All 14 language sites build with `--strict` and no broken links.
- **SC-002**: After the first translation run, 100% of pages exist in every language; the stale report
  is empty.
- **SC-003**: Editing one English page produces exactly that page as stale in 13 languages.
- **SC-004**: The docs CI job stays under 15 minutes on pull requests.
- **SC-005**: Accessibility checks pass in all languages, including Arabic.

## Decisions to record in planning

- **Plugin**: `mkdocs-static-i18n` (suffix or folder layout) is the established way to build Material
  sites per language; planning confirms it supports the pinned `mkdocs-material` (9.7.x).
- **MkDocs 2.0**: the Material team has said Material 9.x won't support MkDocs 2.0 and is moving to its
  successor (Zensical). Planning MUST check the current status and record whether to build this on
  MkDocs 1.x + Material 9 (pinned) or wait/migrate, so translations aren't laid out twice. Keep the
  translated files plain Markdown with the source hash in front matter so they survive a migration.

## Assumptions

- Translations are AI-generated and committed, as for the UI; native-speaker review is welcome but
  not required to publish.
- Screenshots per language come later (they need the screenshot test run per language and roughly
  13× the image storage).
- The 13 languages match the UI's; adding a UI language adds it to the docs.
