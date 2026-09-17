# Design: Typed views via `// @vs` annotations

Date: 2026-09-17  
Status: draft (awaiting user review)  
Related: `docs/superpowers/specs/2026-09-17-mt-sim-trace-view-design.md`

## Goal

Let the C simulator emit **semantic categories** in the JSONL `meta` line (matrix, ops such as matmul/mul, optional exprs). `vs-view` keeps the raw port board and, **below it**, renders typed views (matrix grids). Clicking a cell expands a numeric formula when an `expr` is present.

## Non-goals (v1)

- Evaluating expressions inside `vs` (C only forwards strings).
- Inferring matrix layout from signal names (`a00` heuristics) — optional later.
- Parsing `@vs` into the flex/bison grammar (v1: pre-scan source comments).
- Auto-deriving `expr` from assign AST (future upgrade path).
- Changing simulation semantics.

## Approach (approved)

**Hybrid metadata**: `@vs view` for layout, `@vs op` for operation links, `@vs expr` for expandable formulas. UI: matrix by default + click-to-expand with substituted values.

## Annotation syntax

Line comments only, prefix `@vs`, anywhere in the compilation unit (typically above `module`):

```text
// @vs view matrix <Name> rows=<R> cols=<C> cells=<row>;...<row>
// @vs op <kind> out=<ViewName> left=<ViewName> right=<ViewName>
// @vs expr <sig> = <infix-expr-over-signal-names>
```

### `view matrix`

- `Name`: identifier used by `op` and UI titles (e.g. `A`, `B`, `C`).
- `rows`, `cols`: positive integers.
- `cells`: `;`-separated rows, each row `,`-separated **signal names** that must match netlist signals.
- Example:

```verilog
// @vs view matrix A rows=2 cols=2 cells=a00,a01;a10,a11
// @vs view matrix B rows=2 cols=2 cells=b00,b01;b10,b11
// @vs view matrix C rows=2 cols=2 cells=c00,c01;c10,c11
```

### `op`

- Extensible `kind` string. **v1 UI must implement `matmul`**; other kinds are stored in meta and ignored by the view (or shown as a plain label).
- `matmul`: relates three **matrix** views (`left` × `right` = `out`). Names refer to `view` `name`s, not signal names.

```verilog
// @vs op matmul out=C left=A right=B
```

### `expr`

- Maps one **output (or any) signal** to an infix expression using signal identifiers and `+ - * / ( )`.
- Used only by vs-view for expansion; C does not evaluate.

```verilog
// @vs expr c00 = a00*b00+a01*b10
```

### Error handling

- Unknown `@vs` verb / malformed line → diagnostic **warning**, ignore that line; parse/sim continue.
- Cell/expr signal not in netlist → warning when emitting meta (or at anno bind time); still emit views so UI can show `x` / missing.

## Trace `meta` extension

Existing `meta` fields (`module`, `ports`, `procs`) unchanged. Add optional arrays/objects:

```json
{
  "t": 0, "d": 0, "tid": 0, "op": "meta",
  "module": "matmul2x2",
  "ports": [ "...unchanged..." ],
  "procs": [ "...unchanged..." ],
  "views": [
    {
      "kind": "matrix",
      "name": "A",
      "rows": 2,
      "cols": 2,
      "cells": [["a00", "a01"], ["a10", "a11"]]
    }
  ],
  "ops": [
    { "kind": "matmul", "out": "C", "left": "A", "right": "B" }
  ],
  "exprs": {
    "c00": "a00*b00+a01*b10",
    "c01": "a00*b01+a01*b11",
    "c10": "a10*b00+a11*b10",
    "c11": "a10*b01+a11*b11"
  }
}
```

- Absent `views` / `ops` / `exprs` → vs-view behaves exactly as today.
- Buffer sizes: meta line may grow; prefer building via arena/`FILE*` chunked write or larger buffer if current 4KiB `snprintf` is insufficient.

## C pipeline

1. **Pre-scan** (v1): given the same source path as parse, read text, collect `@vs` lines into `vs_anno_set_t` (arena-allocated).
2. Store the set on **`vs_design_t`**; elab copies a **const pointer** onto `vs_netlist_t` so `trace_meta` can see it without re-parsing.
3. `trace_meta()` serializes `views` / `ops` / `exprs` as JSON (enlarge or stream the meta write if >4KiB).
4. No change to event ops (`commit`, `force`, …).

Files likely touched: `src/util/anno.c` + `include/vs/anno.h`, design/elab headers, `src/main.c` (call pre-scan), `src/sim/sim.c` (`trace_meta`), `examples/matmul2x2.v`, `tools/vs-view`, tests.

## vs-view UI

1. Keep existing module board (raw pins).
2. New section **Typed views** under the board when `meta.views` non-empty:
   - For `op.kind === "matmul"`, lay out `left` grid, `×`, `right` grid, `=`, `out` grid.
   - Orphan matrices (no op) still render individually.
3. Cell value = `valueAt(waves, sig, t)` (same as pins).
4. **Click cell**:
   - If `exprs[sig]`: tokenize identifiers, substitute current values, show e.g. `1*5+2*7 = 19` (right-hand side = watched cell value, not re-evaluated in JS unless we choose to; v1 **display substitute + show cell value**, optional simple eval for sanity).
   - Else: `sig = val`.
5. CSS: simple HTML table / CSS grid; no card clutter; match existing board chrome.

### Expression display (v1)

- Replace whole-word signal names in `expr` string with decimal values from waves at `t`.
- Unknown/`x` → keep name or show `x`.
- Show `substituted_expr = cell_value` (do not require a JS evaluator for correctness).

## Example update

`examples/matmul2x2.v` gains the `@vs` block matching A/B/C and four `expr` lines for `c00`…`c11`. Document in `examples/README.md` and `docs/` briefly.

## Testing

| Test | Expect |
|------|--------|
| Parse/sim matmul without breaking | Existing `sim.matmul2x2` still green |
| Meta contains views | New cmake or unit check: `vs run ... --trace` first line has `"views"` |
| Malformed `@vs` | Warning; sim still finishes |
| vs-view | Manual / light fixture: API returns `views`; client renders (optional snapshot later) |

## Future

- Derive `expr` from assign AST.
- More `view` kinds (vector, struct/bus bundle).
- CLI override of annotations.
- JS eval of substituted expr to verify `=` matches cell.

## Decisions log

- Annotation source: **C** — `// @vs` in Verilog comments.
- Expansion: matrix grid **and** click-through numeric formula (options 2+3).
- Implementation style: **hybrid metadata** (not pure AST reverse, not CLI-only).
