# Quickstart: Diagrams in the Docs (024)

## Prerequisites

- Python 3.12 with the docs requirements: `pip install -r docs/requirements.txt`
- Node 24, and `npm ci` in `web/`

## Build the site with self-hosted mermaid

```bash
cd web && npm run docs:vendor && cd ..   # copies mermaid.min.js into docs/assets/javascripts/vendor/
mkdocs build --strict
python3 tools/docs/diagrams.py --site site
```

**Expected:**
- The build passes.
- The site check prints `site: N diagram pages, all load the vendored mermaid; no CDN references`.

## Check every diagram

```bash
python3 tools/docs/diagrams.py          # completeness + freshness
cd web && npm run docs:diagrams         # parse + render each diagram in Chromium
```

**Expected:**
- `diagrams.py` prints one line per diagram, `ok`.
- `docs:diagrams` prints `rendered 16/16` (15 diagrams plus the README copy of DIA-01).

## Prove the checks fail (SC-002, SC-003, SC-004)

1. **Broken syntax.** Change `-->` to `-=>` in any diagram, then run `npm run docs:diagrams`. It fails, naming `docs/<page>.md:<line> DIA-NN`. Revert.
2. **Incomplete.** Delete a diagram's `accDescr` line, then run `diagrams.py`. It fails with `incomplete`. Revert.
3. **Stale, non-blocking.** Add a comment to `src/MQTTClient.cpp`, then run `diagrams.py`. It prints a `stale` warning for DIA-09 and exits 0. Revert.
4. **Stale, blocking.** Add a comment inside `evaluateSafety` in `lib/AlpacaLogic/src/SafetyEvaluator.cpp`, then run `diagrams.py`. It fails with `stale-blocking` for DIA-02 (and DIA-03). Revert.
5. **CDN.** Build without `docs:vendor`, then run `diagrams.py --site site`. It fails with "vendored mermaid missing".

## Change behaviour shown in a diagram

1. Update the diagram, its caption and its "Diagram in words" block.
2. Run `python3 tools/docs/diagrams.py --confirm DIA-NN`. This records the new fingerprint, which is your statement that the diagram matches the code.
3. Commit the code and the docs together.

## Render review

Serve `site/` and screenshot every diagram page at 1280 px and 400 px. Check:
- every diagram is drawn in the site palette;
- there is no horizontal page scroll at 400 px;
- every caption and "Diagram in words" block is present.
