# Data Model: Diagrams in the Docs (024)

## Diagram

A Mermaid diagram in a Markdown file.

| Field | Source | Rules |
|---|---|---|
| `id` | metadata `diagram:` | `DIA-NN`, from the spec catalogue. One ID may appear in several files only if every copy's body is identical (FR-012). |
| `file`, `line` | position of the ` ```mermaid ` fence | Used in every error message. |
| `body` | fence contents | Must contain `accTitle:` and `accDescr` (`accDescr:` or `accDescr {`). Must not contain `classDef`, `style ` or `linkStyle` (FR-003). |
| `sources` | metadata `sources:` | One or more SourceRefs, space-separated. Required. |
| `blocking` | metadata `blocking:` | `true` or `false`, default `false`. `true` for safety-verdict diagrams (DIA-02, DIA-03, DIA-04). |
| `fingerprint` | metadata `fingerprint:` | 16 hex characters, or `unconfirmed`. Must equal the computed fingerprint, or the diagram is **stale**. |
| `caption` | `<figcaption>` after the fence inside `<figure>`, or a following `*Figure: …*` line (README) | Required, non-empty. |
| `inWords` | a `??? info "Diagram in words"` block after the figure (docs), or `<details><summary>Diagram in words</summary>` (README) | Required, non-empty. |

## SourceRef

| Form | Meaning |
|---|---|
| `path/to/file` | The whole file's bytes. |
| `path/to/dir/` | Every file under the directory, recursively, sorted by path. |
| `path/to/file#symbol` | The definition of `symbol` in that file: from the first line that contains `symbol(` and isn't a declaration (it doesn't end in `;`) up to its matching closing brace. If no such line exists, that is an error (the symbol was renamed or removed). |

A path that doesn't exist is an error.

## Fingerprint

`sha256( for each SourceRef in listed order: ref + "\n" + content + "\n" )`, the first 16 hex digits. It is recorded by `diagrams.py --confirm DIA-NN` (or `--confirm all`).

## CheckResult (per diagram)

| State | When | Exit effect |
|---|---|---|
| `ok` | complete, fingerprint matches | none |
| `incomplete` | missing metadata, accTitle/accDescr, caption, words block, or has inline styling | error |
| `stale` | fingerprint differs, `blocking: false` | warning (GitHub `::warning` annotation) |
| `stale-blocking` | fingerprint differs, `blocking: true` | error |
| `bad-source` | path or symbol not found | error |
| `mismatch` | two copies of one ID differ | error |

## Site check (`--site <dir>`)

For each built HTML page:
- if it contains `class="mermaid"`, it must include `assets/javascripts/vendor/mermaid.min.js`;
- no page may contain `unpkg.com` or `cdn.jsdelivr.net`.

`<dir>/assets/javascripts/vendor/mermaid.min.js` must exist.
