# Typed Views (`// @vs`) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Parse `// @vs` annotations from Verilog, emit them on the JSONL `meta` line, and render matrix typed views (with click-to-expand exprs) under the raw module board in vs-view.

**Architecture:** Text pre-scan collects annotations into `vs_anno_set_t` on `vs_design_t`; elab copies a const pointer onto `vs_netlist_t`; `trace_meta` serializes `views`/`ops`/`exprs`. vs-view passes those fields through `/api/trace` and renders a Typed views section that substitutes expr identifiers with wave values at scrubber time `t`.

**Tech Stack:** C11, CMake/CTest, existing arena/diag, TypeScript vs-view (no new npm deps).

**Spec:** `docs/superpowers/specs/2026-09-17-typed-views-anno-design.md`

## Global Constraints

- Annotations are line comments starting with `// @vs` (pre-scan; do not put `@vs` in flex/bison grammar).
- Simulation semantics unchanged; unknown/malformed `@vs` → `vs_diag_warn`, ignore line.
- C does not evaluate exprs; vs-view only substitutes signal names with values and shows `substituted = cell_value`.
- v1 UI must implement `op.kind === "matmul"`; other op kinds stored but ignored (or plain label).
- Absent `views` → UI identical to today.
- Example path is `examples/` (not `docs/examples/`).
- Each touched directory keeps/updates its `README.md` if responsibility changes; follow `AGENTS.md` / `CLAUDE.md`.

## File map

| File | Role |
|------|------|
| `include/vs/anno.h` | `vs_anno_set_t` + parse/serialize API |
| `src/util/anno.c` | Pre-scan parser + JSON fragment writers |
| `include/vs/ast.h` / `src/front/ast.c` | `vs_design_t.annos` field |
| `include/vs/elab.h` / `src/elab/elab.c` | `vs_netlist_t.annos` pointer |
| `src/main.c` | Call `vs_anno_scan_file` after successful parse (run path) |
| `src/sim/sim.c` | Extend `trace_meta` |
| `CMakeLists.txt` | `anno.c` in `vs_util`; `test_anno`; meta ctest |
| `tests/util/test_anno.c` | Unit tests for scanner |
| `tests/sim/run_meta_views.cmake` | Assert matmul trace meta has `"views"` |
| `examples/matmul2x2.v` | Add `@vs` block |
| `tools/vs-view/src/{server,client}.ts` + `public/{index.html,style.css}` | Pass-through + UI |
| `examples/README.md`, `docs/README.md` or short `docs/` note | Document `@vs` |

---

### Task 1: Annotation types + failing unit test

**Files:**
- Create: `include/vs/anno.h`
- Create: `tests/util/test_anno.c`
- Modify: `CMakeLists.txt` (add `test_anno` executable + CTest; do **not** add `anno.c` yet so link fails or stub)

**Interfaces:**
- Consumes: `vs_arena_t`, `vs_diag_t`, `vs_loc_t` from `vs/arena.h`, `vs/diag.h`
- Produces (declare in header; implement in Task 2):

```c
typedef struct vs_anno_matrix {
    const char *name;
    int rows;
    int cols;
    const char ***cells; /* [row][col] signal name */
} vs_anno_matrix_t;

typedef struct vs_anno_op {
    const char *kind; /* "matmul", ... */
    const char *out;
    const char *left;
    const char *right;
} vs_anno_op_t;

typedef struct vs_anno_expr {
    const char *sig;
    const char *expr; /* rhs string */
} vs_anno_expr_t;

typedef struct vs_anno_set {
    vs_anno_matrix_t *matrices;
    int nmatrices;
    vs_anno_op_t *ops;
    int nops;
    vs_anno_expr_t *exprs;
    int nexprs;
} vs_anno_set_t;

/* Scan path for // @vs lines. Returns set (possibly empty). Never NULL if arena OK. */
vs_anno_set_t *vs_anno_scan_file(vs_arena_t *arena, vs_diag_t *diag, const char *path);

/* Lookup expr by signal name; NULL if absent. */
const char *vs_anno_find_expr(const vs_anno_set_t *set, const char *sig);
```

- [ ] **Step 1: Write `include/vs/anno.h`** with the types and declarations above (include guards, no implementation).

- [ ] **Step 2: Write failing unit test** `tests/util/test_anno.c`:

```c
#include "vs/anno.h"
#include "vs/arena.h"
#include "vs/diag.h"
#include <stdio.h>
#include <string.h>

static int expect(int cond, const char *msg) {
    if (!cond) {
        fprintf(stderr, "FAIL: %s\n", msg);
        return 1;
    }
    return 0;
}

int main(void) {
    const char *path = "tests/util/fixtures/anno_matmul.v";
    vs_arena_t *a = vs_arena_create();
    vs_diag_t *d = vs_diag_create(a);
    vs_anno_set_t *set = vs_anno_scan_file(a, d, path);
    int fail = 0;
    fail |= expect(set != NULL, "set non-null");
    fail |= expect(set && set->nmatrices == 3, "3 matrices");
    fail |= expect(set && set->nops == 1, "1 op");
    fail |= expect(set && set->nexprs == 4, "4 exprs");
    if (set && set->nmatrices >= 1) {
        fail |= expect(strcmp(set->matrices[0].name, "A") == 0, "A name");
        fail |= expect(set->matrices[0].rows == 2 && set->matrices[0].cols == 2, "A shape");
        fail |= expect(strcmp(set->matrices[0].cells[0][0], "a00") == 0, "A[0][0]");
        fail |= expect(strcmp(set->matrices[0].cells[1][1], "a11") == 0, "A[1][1]");
    }
    if (set && set->nops >= 1) {
        fail |= expect(strcmp(set->ops[0].kind, "matmul") == 0, "op kind");
        fail |= expect(strcmp(set->ops[0].out, "C") == 0, "op out");
    }
    fail |= expect(set && vs_anno_find_expr(set, "c00") != NULL, "c00 expr");
    fail |= expect(set && strstr(vs_anno_find_expr(set, "c00"), "a00") != NULL, "c00 has a00");
    vs_arena_destroy(a);
    return fail ? 1 : 0;
}
```

- [ ] **Step 3: Create fixture** `tests/util/fixtures/anno_matmul.v`:

```verilog
// @vs view matrix A rows=2 cols=2 cells=a00,a01;a10,a11
// @vs view matrix B rows=2 cols=2 cells=b00,b01;b10,b11
// @vs view matrix C rows=2 cols=2 cells=c00,c01;c10,c11
// @vs op matmul out=C left=A right=B
// @vs expr c00 = a00*b00+a01*b10
// @vs expr c01 = a00*b01+a01*b11
// @vs expr c10 = a10*b00+a11*b10
// @vs expr c11 = a10*b01+a11*b11
module dummy; endmodule
```

- [ ] **Step 4: Wire CMake for `test_anno`** linking only `vs_util` (will fail to link until Task 2 adds `anno.c`):

```cmake
add_executable(test_anno tests/util/test_anno.c)
target_link_libraries(test_anno PRIVATE vs_util)
add_test(NAME anno COMMAND test_anno)
# Run from source root so fixture path works:
set_tests_properties(anno PROPERTIES WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})
```

- [ ] **Step 5: Configure/build and confirm failure**

Run: `cmake -B build && cmake --build build --target test_anno 2>&1`
Expected: link error `undefined symbol: vs_anno_scan_file` (or equivalent).

- [ ] **Step 6: Commit**

```bash
git add include/vs/anno.h tests/util/test_anno.c tests/util/fixtures/anno_matmul.v CMakeLists.txt
git commit -m "$(cat <<'EOF'
test: add failing anno scanner unit test

EOF
)"
```

---

### Task 2: Implement `vs_anno_scan_file`

**Files:**
- Create: `src/util/anno.c`
- Modify: `CMakeLists.txt` (`vs_util` sources += `src/util/anno.c`)
- Modify: `src/util/README.md` (one line: annotations)

**Interfaces:**
- Consumes: header from Task 1
- Produces: working `vs_anno_scan_file` / `vs_anno_find_expr`

- [ ] **Step 1: Implement `src/util/anno.c`**

Parsing rules (match spec):

1. Read file line-by-line (`fgets`, buffer ≥ 1024).
2. Trim leading whitespace; require prefix `//` then optional space then `@vs`.
3. Tokenize remainder by whitespace for verb:
   - `view matrix <Name> rows=<R> cols=<C> cells=<cells>`
   - `op <kind> out=<O> left=<L> right=<R>` (accept keys in any order after kind)
   - `expr <sig> = <rest-of-line>` (trim spaces around `=`)
4. For `cells`: split on `;` into rows, each row split on `,`; assert `rows`/`cols` match counts or warn and skip.
5. Arena-allocate all strings (`vs_arena_strdup` if exists; else alloc+memcpy) and grow arrays by doubling or fixed max (e.g. 32 matrices / 64 exprs is enough for v1).
6. Malformed → `vs_diag_warn(d, loc, "bad @vs line: %s", line)` with `loc.path=path`, `loc.first_line=lineno`.

Minimal structure sketch:

```c
#include "vs/anno.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *arena_dup(vs_arena_t *a, const char *s) {
    size_t n = strlen(s) + 1;
    char *p = vs_arena_alloc(a, n);
    if (p) memcpy(p, s, n);
    return p;
}

/* ... helpers: starts_with, parse_kv, split_cells ... */

vs_anno_set_t *vs_anno_scan_file(vs_arena_t *arena, vs_diag_t *diag, const char *path) {
    vs_anno_set_t *set = vs_arena_alloc(arena, sizeof(*set));
    memset(set, 0, sizeof(*set));
    FILE *fp = fopen(path, "r");
    if (!fp) {
        vs_loc_t loc = {path, 0, 0, 0, 0};
        vs_diag_warn(diag, loc, "cannot open for @vs scan: %s", path);
        return set;
    }
    char buf[2048];
    int lineno = 0;
    while (fgets(buf, sizeof buf, fp)) {
        lineno++;
        /* strip \n; find // @vs; dispatch */
    }
    fclose(fp);
    return set;
}

const char *vs_anno_find_expr(const vs_anno_set_t *set, const char *sig) {
    if (!set || !sig) return NULL;
    for (int i = 0; i < set->nexprs; i++) {
        if (strcmp(set->exprs[i].sig, sig) == 0) return set->exprs[i].expr;
    }
    return NULL;
}
```

Implement full `view`/`op`/`expr` parsing (no placeholders). Prefer small static helpers in the same file.

- [ ] **Step 2: Add `src/util/anno.c` to `vs_util` in CMakeLists.txt**

```cmake
add_library(vs_util STATIC
  src/util/arena.c
  src/util/strtab.c
  src/util/diag.c
  src/util/anno.c)
```

- [ ] **Step 3: Build and run unit test**

Run: `cmake --build build && ctest --test-dir build -R '^anno$' --output-on-failure`
Expected: `anno` PASSED.

- [ ] **Step 4: Add a second fixture case in the same test file** (or extra asserts): one malformed line `// @vs view matrix` must not crash; `nmatrices` still counts valid lines only. Extend `anno_matmul.v` or add `tests/util/fixtures/anno_bad.v` + second invocation in `test_anno.c`.

- [ ] **Step 5: Commit**

```bash
git add src/util/anno.c CMakeLists.txt src/util/README.md tests/util/test_anno.c tests/util/fixtures/
git commit -m "$(cat <<'EOF'
feat: parse // @vs annotation lines into vs_anno_set

EOF
)"
```

---

### Task 3: Attach annos on design/netlist + scan from `vs run`

**Files:**
- Modify: `include/vs/ast.h` (`struct vs_design` add `const vs_anno_set_t *annos;`)
- Modify: `src/front/ast.c` (`vs_design_new` zero `annos`)
- Modify: `include/vs/elab.h` (`vs_netlist_t` add `const vs_anno_set_t *annos;`)
- Modify: `src/elab/elab.c` (copy `design->annos` onto netlist)
- Modify: `src/main.c` (`cmd_run`: after successful parse, `design->annos = vs_anno_scan_file(...)`)
- Modify: `include/vs/ast.h` include or forward-declare — prefer `#include "vs/anno.h"` in `ast.h` **or** keep opaque pointer with forward decl in `ast.h` and include `anno.h` only where needed. **Prefer:** forward declare `typedef struct vs_anno_set vs_anno_set_t;` in `ast.h` / `elab.h` to avoid include cycles; `main.c` includes `vs/anno.h`.

**Interfaces:**
- Consumes: `vs_anno_scan_file`
- Produces: `nl->annos` available to sim

- [ ] **Step 1: Add fields**

```c
/* ast.h */
typedef struct vs_anno_set vs_anno_set_t;
struct vs_design {
    vs_node_t base;
    vs_module_t *modules;
    const vs_anno_set_t *annos; /* optional; owned by arena via scan */
};
```

```c
/* elab.h */
typedef struct vs_anno_set vs_anno_set_t;
struct vs_netlist {
    ...
    const vs_anno_set_t *annos;
};
```

- [ ] **Step 2: In `vs_elab_flat`, after allocating `nl`:**

```c
nl->annos = design->annos;
```

- [ ] **Step 3: In `cmd_run` after successful `vs_parse_files`:**

```c
#include "vs/anno.h"
...
design->annos = vs_anno_scan_file(arena, diag, file);
```

(Do **not** fail run on warnings.)

- [ ] **Step 4: Rebuild; run existing ctest**

Run: `ctest --test-dir build --output-on-failure`
Expected: all existing tests still PASS (meta shape change comes in Task 4).

- [ ] **Step 5: Commit**

```bash
git add include/vs/ast.h include/vs/elab.h src/front/ast.c src/elab/elab.c src/main.c
git commit -m "$(cat <<'EOF'
feat: attach @vs anno set on design and netlist for run

EOF
)"
```

---

### Task 4: Serialize views/ops/exprs in `trace_meta`

**Files:**
- Modify: `src/sim/sim.c` (`trace_meta`)
- Optionally add helper in `anno.c`: `int vs_anno_write_json(FILE *out, const vs_anno_set_t *set);` that writes `,\"views\":[...],\"ops\":[...],\"exprs\":{...}` or nothing if empty — **recommended** to keep `sim.c` thin.

**Interfaces:**
- Consumes: `nl->annos`
- Produces: meta JSON fields per spec

- [ ] **Step 1: Add `vs_anno_append_meta_json(char *dst, size_t dstsz, size_t *off, const vs_anno_set_t *set)`** in `anno.c` / declare in `anno.h`.

Behavior:
- If `!set` or all counts 0, append nothing.
- Else append comma-prefixed fragments:
  - `"views":[{ "kind":"matrix","name":"...","rows":N,"cols":N,"cells":[[...],[...]] }, ...]`
  - `"ops":[{ "kind":"...","out":"...","left":"...","right":"..." }, ...]`
  - `"exprs":{ "c00":"a00*b00+...", ... }`
- Escape is unnecessary for v1 signal names (identifiers only); do not allow quotes in names.

- [ ] **Step 2: Change `trace_meta` to use a larger buffer (e.g. 16KiB stack or heap via arena) and format:**

```c
snprintf(line, sizeof line,
  "{\"t\":0,\"d\":0,\"tid\":0,\"op\":\"meta\",\"module\":\"%s\",\"ports\":%s,\"procs\":%s%s}\n",
  module, ports, procs, anno_suffix);
```

where `anno_suffix` is built by `vs_anno_append_meta_json` into a side buffer starting empty (functions adds leading commas).

- [ ] **Step 3: Soft-validate cell names** (optional in this task): for each cell, if `vs_netlist_find_sig(nl, name) < 0`, `vs_diag_warn`. Keep emitting JSON.

- [ ] **Step 4: Write failing integration check** `tests/sim/run_meta_views.cmake`:

```cmake
# inputs: VS, SRC, TRACE
execute_process(
  COMMAND ${VS} run ${SRC} --threads 1 --until 40
    --force a00=1 --force a01=2 --force a10=3 --force a11=4
    --force b00=5 --force b01=6 --force b10=7 --force b11=8
    --watch c00,c01,c10,c11
    --trace ${TRACE}
  RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
  message(FATAL_ERROR "vs run failed")
endif()
file(READ ${TRACE} content)
string(FIND "${content}" "\"views\"" idx)
if(idx EQUAL -1)
  message(FATAL_ERROR "meta missing views")
endif()
string(FIND "${content}" "\"exprs\"" idx2)
if(idx2 EQUAL -1)
  message(FATAL_ERROR "meta missing exprs")
endif()
```

Add CTest (will fail until example has `@vs` — Task 5 — **or** point SRC at `tests/util/fixtures/anno_matmul.v` which is not elaboratable as matmul). **Better:** Task 4 test uses a tiny dedicated `tests/sim/fixtures/anno_only.v` that is a 1-port module **plus** `@vs` lines with cells matching that module's ports, so meta test does not depend on matmul example update.

Create `tests/sim/fixtures/tiny_anno.v`:

```verilog
// @vs view matrix M rows=1 cols=1 cells=q
// @vs expr q = q
module tiny_anno(output reg [3:0] q);
  initial q = 0;
endmodule
```

(If `initial` unsupported, use whatever minimal module already elaborates — e.g. copy `inv` pattern with `@vs` on `y`.)

Use `examples/inv.v` pattern:

```verilog
// @vs view matrix Y rows=1 cols=1 cells=y
module inv(input wire a, output wire y);
  assign y = ~a;
endmodule
```

CTest SRC = that fixture under `tests/sim/fixtures/inv_anno.v`, run with `--force a=0 --watch y --trace ...`.

- [ ] **Step 5: Add CTest `sim.meta_views`**; run — **PASS** with fixture.

- [ ] **Step 6: Commit**

```bash
git add src/sim/sim.c src/util/anno.c include/vs/anno.h tests/sim/ CMakeLists.txt
git commit -m "$(cat <<'EOF'
feat: emit views/ops/exprs on trace meta from @vs annos

EOF
)"
```

---

### Task 5: Annotate `examples/matmul2x2.v` + docs

**Files:**
- Modify: `examples/matmul2x2.v`
- Modify: `examples/README.md`
- Modify: `docs/README.md` (one bullet linking typed-views spec)
- Create or short-update: note in `AGENTS.md` that `@vs` belongs in example sources

- [ ] **Step 1: Prepend annotations to matmul** (keep existing assigns):

```verilog
// @vs view matrix A rows=2 cols=2 cells=a00,a01;a10,a11
// @vs view matrix B rows=2 cols=2 cells=b00,b01;b10,b11
// @vs view matrix C rows=2 cols=2 cells=c00,c01;c10,c11
// @vs op matmul out=C left=A right=B
// @vs expr c00 = a00*b00+a01*b10
// @vs expr c01 = a00*b01+a01*b11
// @vs expr c10 = a10*b00+a11*b10
// @vs expr c11 = a10*b01+a11*b11
```

- [ ] **Step 2: Manual verify**

Run:

```bash
./build/vs run examples/matmul2x2.v --threads 2 --until 40 \
  --force a00=1 --force a01=2 --force a10=3 --force a11=4 \
  --force b00=5 --force b01=6 --force b10=7 --force b11=8 \
  --watch c00,c01,c10,c11 --trace build/trace_matmul.jsonl
head -n 1 build/trace_matmul.jsonl
```

Expected: first line contains `"views"` and `"matmul"` and `"c00"`.

- [ ] **Step 3: Update READMEs** — mention `@vs` typed views briefly.

- [ ] **Step 4: Commit**

```bash
git add examples/matmul2x2.v examples/README.md docs/README.md AGENTS.md CLAUDE.md
git commit -m "$(cat <<'EOF'
docs: annotate matmul2x2 with @vs matrix views

EOF
)"
```

---

### Task 6: vs-view pass-through + Typed views UI

**Files:**
- Modify: `tools/vs-view/src/server.ts` — include `views`/`ops`/`exprs` from meta in `/api/trace` body
- Modify: `tools/vs-view/src/client.ts` — types + `renderTypedViews` + click handler
- Modify: `tools/vs-view/public/index.html` — `<div id="typed-views">` under module board
- Modify: `tools/vs-view/public/style.css` — matrix grid styles (simple table, no card chrome)
- Modify: `tools/vs-view/README.md`

**Interfaces:**
- Consumes: meta JSON from Task 4/5
- Produces: UI section

- [ ] **Step 1: Extend server payload**

```ts
views: meta?.views ?? [],
ops: meta?.ops ?? [],
exprs: meta?.exprs ?? {},
```

Extend `TraceEvent` / payload types for `views`, `ops`, `exprs`.

- [ ] **Step 2: HTML** — after `#module-board`:

```html
<div id="typed-views" class="typed-views" hidden></div>
<div id="expr-expand" class="expr-expand" hidden></div>
```

- [ ] **Step 3: Implement `renderTypedViews(data, t)`**

- If no views, set `typed-views` hidden and return.
- Find `ops` with `kind === "matmul"`; for each, resolve matrix views by `name`, render:

```html
<div class="matmul-row">
  <div class="matrix" data-name="A">...</div>
  <span class="op">×</span>
  <div class="matrix" data-name="B">...</div>
  <span class="op">=</span>
  <div class="matrix" data-name="C">...</div>
</div>
```

- Each cell: `<button type="button" class="mcell" data-sig="c00">19</button>` with value from `valueAt`.
- Matrices not referenced by any op: render below as orphan grids.

- [ ] **Step 4: Click handler**

```ts
function expandExpr(data: TracePayload, sig: string, t: number): string {
  const raw = data.exprs?.[sig];
  const cell = valueAt(data.waves, sig, t);
  if (!raw) return `${sig} = ${cell}`;
  const substituted = raw.replace(/\b[a-zA-Z_][a-zA-Z0-9_]*\b/g, (id) => {
    if (data.waves[id]) return valueAt(data.waves, id, t);
    return id;
  });
  return `${substituted} = ${cell}`;
}
```

Show in `#expr-expand` (unhide). Re-render typed views on scrubber `input` alongside `renderModule`.

- [ ] **Step 5: CSS** — `.matrix table`, monospace cells, light borders consistent with existing green/neutral board (avoid purple/glow per project taste).

- [ ] **Step 6: Build and smoke**

```bash
cd tools/vs-view && npm run build
node dist/server.js --trace ../../build/trace_matmul.jsonl --port 8787
curl -s http://127.0.0.1:8787/api/trace | head -c 400
```

Expected: JSON includes `"views"`.

- [ ] **Step 7: Commit**

```bash
git add tools/vs-view
git commit -m "$(cat <<'EOF'
feat(vs-view): render matrix typed views and expr expand

EOF
)"
```

---

### Task 7: Full regression + spec status

**Files:**
- Modify: `docs/superpowers/specs/2026-09-17-typed-views-anno-design.md` — Status: implemented
- Modify: `docs/roadmap.md` — one checkbox line under P2-lite or new bullet

- [ ] **Step 1: Run full suite**

```bash
cmake --build build && ctest --test-dir build --output-on-failure
cd tools/vs-view && npm run build
```

Expected: all CTests PASS; `tsc` clean.

- [ ] **Step 2: Mark spec status `implemented`** and roadmap note.

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/specs/2026-09-17-typed-views-anno-design.md docs/roadmap.md
git commit -m "$(cat <<'EOF'
docs: mark typed-views annotation design implemented

EOF
)"
```

---

## Spec coverage checklist

| Spec item | Task |
|-----------|------|
| `// @vs view/op/expr` syntax | 1–2 |
| Pre-scan not in grammar | 2, 3 |
| Warn on bad lines | 2 |
| `meta.views/ops/exprs` | 4 |
| design→netlist pointer | 3 |
| matmul example annotated | 5 |
| Raw board unchanged + typed below | 6 |
| Click expand substitute + cell value | 6 |
| Backward compatible empty views | 4, 6 |
| Tests (unit + meta) | 1, 2, 4, 7 |

## Plan self-review

- No TBD placeholders in steps.
- Types `vs_anno_set_t` / matrix / op / expr consistent across tasks.
- `vs_anno_find_expr` defined Task 1, used Task 1 test.
- Meta JSON field names match spec (`views`, `ops`, `exprs`, matrix `kind`/`cells`).
