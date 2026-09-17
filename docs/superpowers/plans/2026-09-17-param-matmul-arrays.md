# Parameterized Matmul (parameter + for + arrays) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `parameter`, procedural `for`, and unpacked arrays (1D then 2D) so `examples/matmul.v` can be a general matrix multiply with `@vs array=` typed views.

**Architecture:** Extend flex/bison AST → elab grows a module parameter env and array-shaped netlist signals (public element names `a_i_j`) → sim evaluates indexes and executes `for` → anno expands `array=` views using folded parameters. Ship in three gated phases; each phase must leave `ctest` green.

**Tech Stack:** C11, flex/bison, CMake/CTest, existing arena/diag, TypeScript vs-view (meta-compatible).

**Spec:** `docs/superpowers/specs/2026-09-17-param-matmul-arrays-design.md`

## Global Constraints

- No `generate` in this plan.
- No runtime-dynamic sizes.
- Arrays at most 2D; element tooling names are `base_i_j` (decimal).
- Keep `examples/matmul2x2.v` flat regression green.
- Follow `AGENTS.md` / directory READMEs; examples live under `examples/`.
- Prefer TDD: failing parse/sim test before feature code.
- Do not expand IEEE surface beyond what each phase needs.

## Scope note

This is three sequential subsystems. **After Phase 1 or 2 you may stop and merge.** Do not start Phase 3 until Phase 2 tests pass.

## File map

| File | Role |
|------|------|
| `src/front/lex.l`, `parse.y` | Tokens/grammar for parameter, integer, for, unpacked dims |
| `include/vs/ast.h`, `src/front/ast.c` | AST nodes |
| `src/front/dump_ast.c` | Dump new nodes |
| `include/vs/elab.h`, `src/elab/elab.c` | Param env, array signal expansion to `a_i_j` |
| `src/sim/sim.c` | `for` exec, index lvalue/rvalue → flat sig |
| `src/util/anno.c`, `include/vs/anno.h` | `array=` + param rows/cols expand |
| `examples/matmul.v` | Parameterized example (Phase 3) |
| `tests/parse/ok/*`, `tests/sim/*` | Per-phase regressions |
| `docs/grammar-v0.1.md`, `docs/roadmap.md` | Document subset growth |

---

# Phase 1 — `parameter` + constant folding

### Task 1: Failing parse test for `# (parameter W = 8)`

**Files:**
- Create: `tests/parse/ok/param_width.v`
- Create: `tests/parse/ok/param_width.ast.expected` (minimal placeholder; regenerate after dump works)
- Modify: `CMakeLists.txt` (add `parse.ok.param_width`)

**Interfaces:**
- Consumes: existing `tests/parse/run_ok.cmake` pattern
- Produces: red CTest proving header params not parsed yet

- [ ] **Step 1: Write fixture**

```verilog
module param_width #(
  parameter W = 8
)(
  input wire [W-1:0] x,
  output wire [W-1:0] y
);
  assign y = x;
endmodule
```

- [ ] **Step 2: Add CTest** mirroring `parse.ok.inv` with `SRC=tests/parse/ok/param_width.v`.

- [ ] **Step 3: Run and confirm FAIL** (parse error / missing expected).

```bash
cmake --build build && ctest --test-dir build -R param_width --output-on-failure
```

Expected: FAIL (cannot parse `parameter` / `#(`).

- [ ] **Step 4: Commit**

```bash
git add tests/parse/ok/param_width.v CMakeLists.txt
git commit -m "test: add failing parse case for module parameter W"
```

---

### Task 2: Lexer + AST for `parameter`

**Files:**
- Modify: `src/front/lex.l` — keywords `parameter`, `integer` (integer used in Phase 2; can add now)
- Modify: `include/vs/ast.h` — `VS_PARAM_DECL`, `vs_param_decl_t { name, value_expr }`; `vs_module_t` list of params
- Modify: `src/front/ast.c` — constructors
- Modify: `src/front/dump_ast.c` — dump params

**Interfaces:**
- Produces: `vs_param_decl_new(arena, loc, name, expr)`; module stores `vs_param_decl_t *params` linked list

- [ ] **Step 1: Add tokens** `K_PARAMETER` `K_INTEGER` in `lex.l` / `%token` in `parse.y`.

- [ ] **Step 2: Add AST kinds + constructors** (zero-init lists on `vs_module_new`).

- [ ] **Step 3: Dump params** under module in `dump_ast.c`.

- [ ] **Step 4: Build** (`cmake --build build`) — expect success; parse test still fails until Task 3.

- [ ] **Step 5: Commit**

```bash
git commit -am "feat: AST and tokens for parameter/integer"
```

---

### Task 3: Parse module `#(...)` and body `parameter`

**Files:**
- Modify: `src/front/parse.y` — `module_decl` accepts optional parameter port list; `module_item` accepts `parameter ID = expr ;`

**Grammar sketch:**

```text
module_decl:
  K_MODULE IDENT param_port_list_opt '(' port_list_opt ')' ';'
  module_items K_ENDMODULE

param_port_list_opt:
  /* empty */ | '#' '(' param_port_items ')'

param_port_items:
  K_PARAMETER IDENT '=' expr
  | param_port_items ',' K_PARAMETER IDENT '=' expr
  | param_port_items ',' IDENT '=' expr   /* optional shorthand after first */
```

- [ ] **Step 1: Implement grammar** attaching params onto `vs_module_t`.

- [ ] **Step 2: Capture golden AST**

```bash
./build/vs --dump-ast tests/parse/ok/param_width.v > tests/parse/ok/param_width.ast.expected
```

- [ ] **Step 3: CTest `parse.ok.param_width` PASS**

- [ ] **Step 4: Commit**

```bash
git commit -am "feat: parse module parameter ports and body parameters"
```

---

### Task 4: Elab constant folding for packed ranges

**Files:**
- Modify: `include/vs/elab.h` — optional: document that widths are concrete ints after elab
- Modify: `src/elab/elab.c` — build `param_env` (name→int64) from module params in order; `eval_const(expr, env)`; use in `range_width`

**Interfaces:**
- Produces: `static int eval_const_expr(const vs_expr_t *e, /*env*/, int64_t *out);`
- Consumes: `VS_EXPR_NUMBER`, `VS_EXPR_IDENT` (param), `VS_EXPR_BINARY` `+ - * /`, unary `+/-`

- [ ] **Step 1: Write sim/parse integration fixture** `tests/sim/fixtures/param_passthru.v` (same as param_width) + cmake that runs:

```bash
./build/vs run tests/sim/fixtures/param_passthru.v --force x=7 --watch y --until 20
```

Expect `y=7` (fails until folding works if width wrong / elab error).

- [ ] **Step 2: Implement param env + fold in `range_width` / port widths.**

- [ ] **Step 3: CTest PASS; full `ctest` green.**

- [ ] **Step 4: Update `docs/grammar-v0.1.md`** — mark `parameter` as supported (header + body, const expr subset).

- [ ] **Step 5: Commit**

```bash
git commit -am "feat: elaborate parameters into packed range widths"
```

**Phase 1 gate:** `parameter` parse + width fold works; no arrays/`for` yet.

---

# Phase 2 — Procedural `for` + 1D unpacked arrays

### Task 5: Failing tests for `for` + 1D array

**Files:**
- Create: `tests/parse/ok/for_fill.v`
- Create: `tests/sim/fixtures/for_fill.v` (may share)
- Modify: `CMakeLists.txt`

```verilog
module for_fill #(
  parameter N = 4,
  parameter W = 8
);
  integer i;
  reg [W-1:0] mem [0:N-1];
  always @* begin
    for (i = 0; i < N; i = i + 1)
      mem[i] = i;
  end
endmodule
```

Sim check: after run, `--watch mem_3` expect `3` (public name `mem_3`).

- [ ] **Step 1: Add fixtures + CTests (`parse.ok.for_fill`, `sim.for_fill`) expecting FAIL.**

- [ ] **Step 2: Commit** `test: add failing for/array fill cases`

---

### Task 6: Parse `integer`, `for`, unpacked dims, `mem[i]`

**Files:**
- Modify: `parse.y` / `lex.l`
- Modify: `ast.h` / `ast.c` / `dump_ast.c`

**AST:**
- `VS_INTEGER_DECL` — name list
- `VS_STMT_FOR` — `{ init, cond, step, body }` (init/step as stmts or blocking assigns)
- Unpacked dims on `VS_NET_DECL` / `VS_REG_DECL` / ports: `vs_range_t *unpacked_dims` list (1 dim in this task)
- `VS_EXPR_INDEX` — `{ base, index }` distinct from bit `VS_EXPR_SELECT` when base is array (elab decides; parser may emit INDEX for `ident[expr]` when followed by assign context — **simpler approach:** parser always builds `VS_EXPR_SELECT` for `ident[expr]`; elab rewrites/tags if base is unpacked array)

**Recommended:** introduce `VS_EXPR_INDEX` explicitly from grammar `primary '[' expr ']'` when we need multi-index later: `primary '[' expr ']' ('[' expr ']')*` → chain of INDEX nodes.

- [ ] **Step 1: Grammar for integer decl, for-stmt, unpacked `[const:const]` after name, index chain.**

- [ ] **Step 2: Golden AST for `for_fill.v`.**

- [ ] **Step 3: Parse CTest PASS; sim still FAIL.**

- [ ] **Step 4: Commit** `feat: parse integer, for, and 1D unpacked arrays`

---

### Task 7: Elab expands 1D arrays to `name_i` signals + integer locals

**Files:**
- Modify: `include/vs/elab.h` — extend `vs_signal_t` with:

```c
typedef enum { VS_SIG_SCALAR = 0, VS_SIG_ARRAY1D, VS_SIG_ARRAY2D } vs_sig_shape_t;
/* For expanded elements, keep scalar signals named name_i / name_i_j.
   Optional parent metadata unused in v1 if we only emit flat scalars. */
```

**Strategy (v1):** At elab, for `reg [W-1:0] mem [0:N-1]`, create N scalar signals `mem_0` … `mem_{N-1}` each width W. Rewrite AST index expressions in processes to refer to those names **or** resolve at sim-time via helper `resolve_index(base, i)->sig`. Prefer **sim-time resolve** to avoid mutating AST: store on netlist an array registry:

```c
typedef struct vs_array {
  const char *base;
  int ndim; /* 1 */
  int lens[2];
  int width;
  int first_sig_index; /* contiguous scalars mem_0.. */
} vs_array_t;
```

- [ ] **Step 1: Implement array registry + scalar creation `snprintf(name, "%s_%d", base, i)`.**

- [ ] **Step 2: Integer vars:** either as 32-bit regs omitted from ports, stored in `sim` side table keyed by name, or as internal signals `i`. Prefer **sim integer table** (`vs_sim` map name→int64) filled by integer decls on the process.

- [ ] **Step 3: Unit-style check via `sim.for_fill` still failing until Task 8.**

- [ ] **Step 4: Commit** `feat: elab 1D unpacked arrays into flat mem_i signals`

---

### Task 8: Sim `for` + index read/write

**Files:**
- Modify: `src/sim/sim.c` — `exec_stmt` case `VS_STMT_FOR`; `eval_expr` / `lvalue_sig` handle `VS_EXPR_INDEX`

**For loop semantics (combinational @*):**

```c
exec init;
while (eval(cond)) {
  exec body;
  exec step;
}
```

Guard: max iterations e.g. 1<<20 then `vs_diag_error` (infinite loop protection).

**Index:** evaluate index expr; lookup array registry; compute `sig = first + index`; bounds-check.

- [ ] **Step 1: Implement for + index.**

- [ ] **Step 2: `sim.for_fill` PASS** (`mem_3=3`).

- [ ] **Step 3: Full ctest green.**

- [ ] **Step 4: Grammar doc — `for`, `integer`, 1D unpacked.**

- [ ] **Step 5: Commit** `feat: simulate procedural for and 1D array indexing`

**Phase 2 gate:** can fill a 1D array with `for`; still no 2D / matmul.

---

# Phase 3 — 2D arrays + matmul + `@vs array=`

### Task 9: 2D arrays in parse/elab/sim

**Files:**
- Modify: parser allow second unpacked dim; `VS_EXPR_INDEX` chain `a[i][k]`
- Modify: elab create `a_i_k` scalars; registry `ndim=2`, `lens[0]=N`, `lens[1]=K`, row-major `first + i*cols + j`
- Modify: sim resolve two-level index

- [ ] **Step 1: Failing fixture `tests/sim/fixtures/array2d_write.v`** — write `m[1][0]=5`, watch `m_1_0`.

- [ ] **Step 2: Implement 2D.**

- [ ] **Step 3: Test PASS; commit** `feat: support 2D unpacked arrays as base_i_j`

---

### Task 10: `examples/matmul.v` + CTest

**Files:**
- Create: `examples/matmul.v` (target from spec; `@vs` may wait for Task 11)
- Create: `tests/sim/run_matmul_param.cmake`
- Modify: `CMakeLists.txt`, `examples/README.md`

```bash
./build/vs run examples/matmul.v --threads 2 --until 40 \
  --force a_0_0=1 --force a_0_1=2 --force a_1_0=3 --force a_1_1=4 \
  --force b_0_0=5 --force b_0_1=6 --force b_1_0=7 --force b_1_1=8 \
  --watch c_0_0,c_0_1,c_1_0,c_1_1
# expect 19 22 43 50
```

- [ ] **Step 1: Add example + failing CTest.**

- [ ] **Step 2: Fix any elab/sim gaps until PASS.**

- [ ] **Step 3: Commit** `feat: add parameterized matmul example and test`

---

### Task 11: `@vs` `array=` expansion

**Files:**
- Modify: `src/util/anno.c` — parse `array=a rows=N cols=K` (rows/cols int or param name)
- Modify: call site after elab (main or sim): `vs_anno_resolve(set, param_env)` expands cells to `a_0_0` …
- Modify: `examples/matmul.v` annotations
- Modify: `tests/sim/run_meta_views.cmake` or new test reading matmul meta for `"views"`

**Interfaces:**
- Produces: `void vs_anno_resolve_params(vs_anno_set_t *set, vs_arena_t *a, int (*lookup)(void*, const char*, int64_t*), void *ctx);`

- [ ] **Step 1: Failing meta test on `examples/matmul.v`.**

- [ ] **Step 2: Implement resolve + wire from `vs` run after elab (param env from elab).**

- [ ] **Step 3: Manual vs-view smoke on matmul trace.**

- [ ] **Step 4: Commit** `feat: expand @vs matrix views from array= and parameters`

---

### Task 12: Docs + roadmap closeout

**Files:**
- Modify: `docs/grammar-v0.1.md`, `docs/roadmap.md`, `docs/README.md`, `AGENTS.md` (brief: array element naming)
- Modify: spec status already `approved` → set `implemented` when done

- [ ] **Step 1: Doc updates.**

- [ ] **Step 2: Full `ctest` + `npm run build` in `tools/vs-view`.**

- [ ] **Step 3: Commit** `docs: document parameter/for/arrays and parameterized matmul`

---

## Spec coverage

| Spec item | Tasks |
|-----------|-------|
| parameter + fold | 1–4 |
| procedural for | 5–8 |
| 1D arrays | 5–8 |
| 2D arrays | 9 |
| examples/matmul.v | 10 |
| @vs array= | 11 |
| docs | 4, 8, 12 |
| keep matmul2x2 | all phases (regression) |

## Plan self-review

- No TBD placeholders in task steps.
- Public naming `a_i_j` consistent with spec.
- Phase gates prevent starting 2D before `for` works.
- vs-view unchanged if meta cells expanded in C.
