# Feature Specification: UI design system and copy

**Feature Branch**: `spec/026-ui-design-system`

**Created**: 2026-10-09

**Status**: Implemented (PR #117; review fixes #118). Supersedes parts of spec 025 (glance area) and spec 023 (FR-024).

## Context

The web UI was built on a small, consistent set of parts: cards with a title, an icon and a "?" hint;
metric tiles and reading rows; pills for state; one-line notes inside the card they belong to; the
"?" info tip for explanations; a progress meter inside the card that started a task (Updates). Those
rules were never written down. Recent features (specs 015, 023, 025) were built without them and
added UI that does not look or read like the rest of the app:

- a full-width "glance strip" above the dashboard cards, which reads as a run-on sentence;
- a language-download banner in the page header, far from the control that started it;
- IPv6 addresses run together in the Device & Network card;
- explanations written as paragraphs on the page (e.g. the Sun & Moon time-zone note) instead of
  behind "?";
- English copy that is long, repetitive and machine-sounding, faithfully translated into 13 languages.

This spec writes the design system down as numbered, testable rules (DS-xx), audits the current UI
against them ([audit.md](audit.md), screenshots in [audit/](audit/)), and proposes designs for the
three worst offenders ([mockups/](mockups/), built on the throwaway branch `mock/026`). Nothing here
is built yet; the owner reviews the rules and mockups first.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Status is a card like the others (Priority: P1)

An observer opens the dashboard and sees the device's state in a **Status** card that looks like
every other card: title, icon, "?" hint, a pill summarising it, tiles for Safety, Alerts and Data
(and the imaging app when relevant), and any problems listed inside the card with a "?" for detail
and one action. There is no strip above the cards.

**Why this priority**: it replaces the most visible violation and restores the dashboard's design.

**Independent Test**: open the demo dashboard healthy, with a sensor fault, and with an imaging app
connected; the Status card shows the right tiles and rows; no element other than cards sits between
the header and the card grid.

**Acceptance Scenarios**:

1. **Given** everything is healthy, **When** the dashboard loads, **Then** the Status card shows
   Safety *Safe*, Alerts *Sending*, Data *Live* as tiles and an *All good* pill, and nothing else.
2. **Given** the IR sensor has failed, **When** the dashboard loads, **Then** the Safety tile reads
   *Unsafe*, the card's pill reads *N to check*, and the sensor appears as one row inside the card
   with a "?" giving the consequence and an *Open settings* link.
3. **Given** an imaging app is connected, **Then** an *Imaging app* tile shows *Connected* and
   "Checked N s ago"; when no imaging app has connected and the send mode doesn't depend on one,
   the tile is absent.
4. **Given** a phone at 320-390 px, **Then** the Status card is the first card, tiles sit two per
   row, and no control is hidden.

### User Story 2 - Changing language reports inside the Language card (Priority: P1)

The owner picks a language in Settings → Device → Language. The card itself shows a status row and
a progress meter while the device downloads, then *Installed - reloading*, or a one-line failure
with **Retry** and a "?" explaining what to do. Nothing appears in the page header.

**Why this priority**: the current flow gives poor feedback in the wrong place.

**Independent Test**: in the demo, pick Deutsch with a forced failure; the Language card shows
*Downloading* with a progress meter, then *Couldn't install* with Retry; the header is unchanged.

**Acceptance Scenarios**:

1. **Given** a language is chosen, **When** the download runs, **Then** the Language card shows the
   language name, *Downloading* and a progress meter, and nothing is added to the header or other pages.
2. **Given** the download fails, **Then** the card shows one line naming the problem
   ("No language file for v0.2.0-beta.3 yet.") with **Retry** and a "?" with what to do.
3. **Given** the device restarts mid-download, **Then** the row reads *Device restarting* and the page
   stays on the same address.
4. **Given** the chosen language couldn't load after a reload, **Then** the Language card (not every
   page) says so; the Status card lists it as one row with *Open settings*.

### User Story 3 - Long values and explanations follow the parts (Priority: P2)

Addresses, names and other long values are one per row with a label, truncated with the full value
available; explanations are behind "?", not paragraphs.

**Independent Test**: the Device & Network card shows IPv4, each IPv6 address and the local name on
separate labelled rows; the Sun & Moon card has no paragraph under the chart and its time-zone
detail is in the card's "?".

### User Story 4 - English that reads like a person wrote it (Priority: P2)

Every English string follows the copy rules (DS-20…DS-27); strings that break them are rewritten and
all 13 translations regenerated from the new English.

**Independent Test**: the copy check (FR-012) passes on `en.json`; the audit's flagged strings are
rewritten; translation completeness checks pass.

### User Story 5 - No drift between UI, docs and labels (Priority: P2)

The same thing has the same name on the dashboard, in Settings, on the Alpaca page, in alerts and in
the docs; docs describe the UI as it is.

**Independent Test**: the label-consistency check (FR-013) and docs screenshots regenerate without
mismatches; the audit's drift list is resolved.

### Edge Cases

- A problem with no fix link still appears as a row with a pill.
- More than five problems: the card lists them all (the card grows); no "Show all" toggle.
- Disconnected from the device: the Data tile reads *Disconnected* and the cards grey out as today.
- Right-to-left languages: tiles and rows mirror; addresses stay left-to-right (spec 023).
- The demo marker currently in the glance line moves to the Demo panel button (already present).

## Requirements *(mandatory)*

### Design system rules (to add to docs/development/coding-standards.md as a "UI (DS)" section)

**Layout and parts**

- **DS-01**: Dashboard information lives in cards built with `Card` (title, icon, optional "?"
  `hint`, optional `actions` pill). No full-width banners, strips or bars between the header and
  the card grid, or inside the header, except the header's own navigation and the alerts bell.
- **DS-02**: State is shown with `Pill` (tones: green ok, amber attention, red problem, dim unknown).
  A card's overall state, if any, is one pill in the card's `actions`.
- **DS-03**: Numeric or short values use `MetricTile` (in a `metric-grid`/`tile-grid`); label-value
  pairs use `ReadingRow`. New components need a reason recorded in the PR; prefer composing these.
- **DS-04**: Explanations, definitions and caveats go behind `InfoTip` ("?") - on the card title
  (`hint`), a field label, or a row label. On-page text is limited to one-line `Note`s that report a
  current state or an error, inside the card they belong to.
- **DS-05**: Long values (IPv6, MAC, URLs, topics) are one value per `ReadingRow`, monospace,
  left-to-right, truncated with an ellipsis and the full value in `title`; never run together.
- **DS-06**: A task's progress (update, upload, language download) is shown inside the card that
  started it: a status row plus `ProgressMeter`, then a `Note` with the outcome and at most one
  action (e.g. Retry). Not in the header, not on other pages.
- **DS-07**: Transient confirmations ("Saved") use the existing toast; persistent problems use a
  `Note` in the relevant card and, if they affect observing, a row in the Status card.
- **DS-08**: Each fact appears in exactly one place on the dashboard. A card shows its own state (a
  sensor card its fault, the Safety Monitor card the verdict, the Sky Quality card data freshness);
  the Status card shows only what no other card shows - the imaging app and whether alerts go out,
  plus problems nothing else shows. Settings and rules not in effect belong in Settings, not on
  the dashboard. A test fails if the Status card repeats another card's state.
- **DS-09**: Phone (≤ 599 px): cards stack in one column; tiles stay two per row; nothing overlaps
  or hides a control (with spec 022).
- **DS-10**: Spacing, colour and type come from the existing tokens in `index.css`; no new colours,
  font sizes or shadows without adding a token.

**Copy (English source; translations follow)**

- **DS-20**: Labels and titles: sentence case, ≤ 3 words where possible, nouns not sentences
  ("Local name", not "This device's local network name").
- **DS-21**: Pills and tile values: one or two words ("Safe", "Sending", "Live", "Connected").
- **DS-22**: Notes: one sentence, ≤ 90 characters, state first, then the fix ("No language file for
  v0.2.0-beta.3 yet." + Retry).
- **DS-23**: "?" hints: ≤ 2 short sentences, ≤ 160 characters; say what it is and why it matters.
- **DS-24**: No run-on status lines joined with "·" or "-"; one fact per tile or row.
- **DS-25**: Banned filler: "at a glance", "simply", "please note", "note that", "it is important",
  "in order to", "ensure", "seamless", "worked out in", "as soon as the device has it", parentheses
  that restate the label. Product names (N.I.N.A.) only as examples inside hints.
- **DS-26**: Times and places say whose they are once, in a hint, not in every line.
- **DS-27**: The same thing has one name everywhere (UI, alerts, docs, Home Assistant entities):
  the glossary in `tools/i18n/glossary/en.json` is the source of names.

### Functional requirements

- **FR-001**: The dashboard MUST show a **Status** card (first in the default order, movable like
  other cards) replacing the glance strip; it supersedes spec 025 FR-005, FR-006 (placement),
  FR-014 (placement), FR-020 and SC-004. The content requirements of spec 025 (what must be visible
  without a click, priority order, inventory) still apply, delivered through this card.
- **FR-002**: The Status card MUST show an Imaging app tile per Alpaca device (or one "Alpaca off"
  tile) and an Alerts tile, and every problem no other card shows as a row with a "?" (detail) and
  at most one action. *(Amended October 2026: no Safety or Data tile, no sensor or
  settings-not-in-effect rows - DS-08.)*
- **FR-003**: The card's pill MUST read *All good* or *N to check* (tone by worst severity).
- **FR-004**: The Language card MUST show the download's progress and outcome per DS-06; the header
  banner and the page-wide language notice MUST be removed. Supersedes spec 023 FR-024's "on every
  page"; the states it lists (downloading, restarting, installed, still downloading, failed with
  reason and Retry, interrupted after reload) remain.
- **FR-005**: The Device & Network card MUST list IPv4, each IPv6 address (labelled by scope, with a
  "?" for non-global scopes) and the local name as separate rows per DS-05, and MUST NOT repeat the
  IPv4 address in a tile.
- **FR-006**: The Sun & Moon card's time-zone sentence MUST move into the card's "?" hint.
- **FR-007**: Every violation in [audit.md](audit.md) MUST be fixed or recorded as an accepted
  exception with a reason.
- **FR-008**: The English strings flagged in the audit MUST be rewritten per DS-20…DS-27, and all
  13 translations regenerated from the new English with the spec 023 tooling and review pass.
- **FR-009**: Docs MUST match the UI: the dashboard guide, the languages page and any screenshots
  are updated; the screenshot test regenerates the docs images.
- **FR-010**: The dashboard inventory (spec 025) MUST be updated: items move from the glance area
  to the Status card; tests assert the card, not the strip.
- **FR-011**: The DS rules MUST be added to `docs/development/coding-standards.md` and referenced
  from the constitution's quality gate, so convergence checks UI work against them.
- **FR-012**: A copy check MUST run in the quality gate: it fails on English strings over the DS-21,
  DS-22 and DS-23 limits (by string type, from the key's context entry), on banned phrases (DS-25)
  and on "·"-joined status templates (DS-24); existing exceptions need EXC-01 reasons.
- **FR-013**: A label-consistency check MUST compare the names of sensors, states and features
  across `en.json`, the alert templates, the Home Assistant entity names and the docs glossary, and
  fail on mismatches.
- **FR-014**: A layout check MUST fail if the dashboard renders any element between the header and
  the card grid other than the toolbar (Arrange), at 1280 and 390 px.
- **FR-015**: Specs that add or change UI MUST cite the DS rules they rely on and include a mockup
  screenshot reviewed before implementation (added to the spec template).

### Key Entities

- **DS rule**: numbered design-system or copy rule with a test or a review step.
- **Audit finding**: violation → rule → screenshot → fix, tracked in [audit.md](audit.md).

## Success Criteria *(mandatory)*

- **SC-001**: No element sits between the header and the dashboard cards at 1280 or 390 px (FR-014 passes).
- **SC-002**: All audit findings are fixed or accepted with a reason.
- **SC-003**: The copy check passes on `en.json` with no unexplained exceptions; the longest note is ≤ 90 characters.
- **SC-004**: Switching language shows progress within 1 s inside the Language card and nowhere else.
- **SC-005**: The owner signs off the Status card, Language card and Device & Network mockups before build.
- **SC-006**: UI growth stays within the per-feature budget (10 KB gzipped).

## Assumptions

- The Status card reuses spec 025's `glanceItems` logic and inventory; only presentation changes.
- Tile set: Safety, Alerts, Data always; Imaging app per spec 025 relevance; nothing else as a tile.
- Copy limits (90 / 160 characters) are for English; translations may run longer but must pass the
  spec 023 overflow checks.
- Mockups use hard-coded English and are not production code.
