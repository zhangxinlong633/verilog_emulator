# NPU + BLAS Hierarchy Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add minimal module instantiation + multi-file `vs run`, ship `examples/blas/{gemm,gemm_bt,relu,row_argmax}.v`, and refactor `examples/npu_transformer.v` to instantiate those leaves while keeping the golden `Y=[[3,4],[3,4]]`.

**Architecture:** Parse named-port instances into AST; elab recursively flattens children into the netlist with `inst_` prefixes; combo BLAS leaves are `always @*` like `matmul.v`; NPU FSM only schedules and latches leaf outputs. Top module = first module of the **first** CLI `.v` file; other files supply definitions.

**Tech Stack:** Existing flex/bison/C11 elab/sim; CTest; `@vs` unchanged on NPU arch regs.

**Spec:** `docs/superpowers/specs/2026-09-17-npu-blas-hierarchy-design.md`

## Global Constraints

- Named ports only in v1; whole 2D array port connections required for BLAS.
- Flatten only (no hierarchical sim scopes).
- First CLI file provides the top module; list BLAS files after it in tests.
- Keep existing CTests green; NPU golden unchanged.
- Follow `AGENTS.md` / per-directory READMEs; BLAS under `examples/blas/`.

## File map

| File | Role |
|------|------|
| `include/vs/ast.h`, `src/front/ast.c`, `dump_ast.c` | `VS_INSTANCE` AST |
| `src/front/parse.y` | instance grammar |
| `src/main.c` | multi-file `vs run` |
| `src/elab/elab.c`, `include/vs/elab.h` | recursive flatten |
| `examples/blas/*.v` + README | BLAS leaves |
| `examples/blas/gemm_top.v` | optional sole-top for gemm CTest |
| `examples/npu_transformer.v` | instantiate BLAS |
| `tests/sim/run_*.cmake`, `CMakeLists.txt` | multi-file SRC lists |
| `docs/grammar-v0.1.md`, roadmap | document instantiation |

---

### Task 1: Failing parse test for instantiation

**Files:**
- Create: `tests/parse/ok/inst_gemm.v` (parent + child in one file for parse-only)
- Modify: `CMakeLists.txt`

```verilog
module leaf #(parameter W = 8)(input wire [W-1:0] a, output wire [W-1:0] y);
  assign y = a;
endmodule
module top;
  wire [7:0] a, y;
  leaf #(.W(8)) u (.a(a), .y(y));
endmodule
```

- [ ] **Step 1: Add fixture + CTest `parse.ok.inst_gemm`** (expect FAIL until Task 2–3).

- [ ] **Step 2: Commit** `test: add failing module instantiation parse case`

---

### Task 2: AST + parse for `VS_INSTANCE`

**Files:** AST/parser/dump as in file map.

**AST sketch:**

```c
typedef struct vs_port_conn {
  vs_node_t base;
  const char *port_name;
  vs_expr_t *expr; /* usually ident of array base or scalar */
} vs_port_conn_t;

typedef struct vs_param_conn {
  vs_node_t base;
  const char *name;
  vs_expr_t *expr;
} vs_param_conn_t;

typedef struct vs_instance {
  vs_item_t base; /* kind VS_INSTANCE */
  const char *mod_name;
  const char *inst_name;
  vs_param_conn_t *params;
  vs_port_conn_t *ports;
} vs_instance_t;
```

**Grammar:**

```text
module_item:
  IDENT param_override_opt IDENT '(' named_port_list ')' ';'
param_override_opt: /* empty */ | '#' '(' named_param_list ')'
named_param_list: '.' IDENT '(' expr ')' (',' ...)*
named_port_list:  '.' IDENT '(' expr ')' (',' ...)*
```

- [ ] **Step 1: Implement AST constructors + dump.**

- [ ] **Step 2: Wire parse.y; regenerate `inst_gemm.ast.expected`.**

- [ ] **Step 3: `parse.ok.inst_gemm` PASS.**

- [ ] **Step 4: Commit** `feat: parse named-port module instantiations`

---

### Task 3: Multi-file `vs run`

**Files:** `src/main.c`

Today only one positional `.v` is kept. Change to collect up to N (e.g. 16) non-option paths into `paths[]`, pass `npaths` to `vs_parse_files`. Anno scan: scan **first** file (NPU) for `@vs` (good enough).

- [ ] **Step 1: Implement multi-file collection.**

- [ ] **Step 2: Manual** `./build/vs --dump-ast examples/matmul.v examples/inv.v` shows both modules in design.

- [ ] **Step 3: Commit** `feat: allow multiple .v files on vs run`

---

### Task 4: Elab recursive flatten

**Files:** `src/elab/elab.c` (largest task)

**Algorithm:**

1. Index all modules in design by name.
2. Elaborate top = `design->modules` (first of first file — ensure parse appends in file order; first file’s first module must be top).
3. Existing logic for params/ports/decls/processes of current module.
4. When seeing `VS_INSTANCE`:
   - Lookup child module; build child param env (parent consts + overrides).
   - For each `.port(expr)`: if expr is ident naming a parent array base or scalar, record binding `child_port → parent_signal_base`.
   - Recursively elaborate child decls into netlist:
     - Child internal signals → rename `snprintf("%s_%s", inst, local)` (elements `inst_base_i_j`).
     - Child ports → **do not duplicate**; map uses of port idents in child AST to parent bound signals (rewrite expr idents while cloning processes, or maintain alias table in sim — **prefer rewrite on clone** into new stmt/expr trees in arena).
   - Clone child `always`/`assign` into netlist processes with prefixed names.

**v1 simplification if rewrite is too hard:** require BLAS outputs/inputs only connect parent arrays with the **same element naming**, and elaborate child as if ports were continuous assigns `parent_c_i_j = child_internal...` — still need clone.

**Pragmatic v1:** Inline-expand by elaborating child with a **name prefix** and **port alias map** consulted in `find_sig` during elab of child expressions (pass alias map into eval helpers used when building sensitivity — actually sim uses flat names only). So at elab time, when emitting child processes, rewrite IDENT `a` → parent connection expr’s name `x` (array base).

- [ ] **Step 1: Unit path** — single-file `top`+`leaf` from Task 1 must `vs run` and force/watch through instance (`--force a=3 --watch y` expect 3) if leaf is assign. May need tiny combo top.

- [ ] **Step 2: Implement flatten + alias rewrite.**

- [ ] **Step 3: CTest `sim.inst_leaf` green.**

- [ ] **Step 4: Commit** `feat: elaborate module instances by flattening into netlist`

---

### Task 5: BLAS library leaves + gemm test

**Files:**
- Create: `examples/blas/README.md`, `gemm.v`, `gemm_bt.v`, `relu.v`, `row_argmax.v`
- Create: `examples/blas/gemm_top.v` wrapping gemm as top for CTest **or** run gemm.v alone if it is the only module in first file
- Create: `tests/sim/run_blas_gemm.cmake`

`blas_gemm` mirrors `matmul.v` ports `a,b,c` with params `N,K,M,W` (c width `2*W` or 32).

- [ ] **Step 1: Write four leaves + README.**

- [ ] **Step 2: CTest forces 2×2 like matmul; expect 19/22/43/50.**

- [ ] **Step 3: Commit** `feat: add examples/blas gemm/relu/argmax leaves`

---

### Task 6: Refactor NPU to instantiate BLAS

**Files:**
- Rewrite: `examples/npu_transformer.v`
- Modify: `tests/sim/run_npu_transformer.cmake` (+ meta) to pass BLAS file list after NPU

NPU body: declare wire/reg arrays `q_w`…; instantiate:

| Instance | Module | Role |
|----------|--------|------|
| `u_q` | gemm | x·wq → q_w |
| `u_k` | gemm | x·wk → k_w |
| `u_v` | gemm | x·wv → v_w |
| `u_s` | gemm_bt | q·kᵀ → s_w |
| `u_a` | row_argmax | s → a_w |
| `u_o` | gemm | a·v → o_w |
| `u_h` | gemm | o·w1 → h_pre_w |
| `u_relu` | relu | h_pre → h_w |
| `u_y` | gemm | h·w2 → y_w |

FSM latches `*_w` into arch regs each phase (same phase map as before).

- [ ] **Step 1: Refactor NPU; update cmake file lists.**

- [ ] **Step 2: `sim.npu_transformer` + `sim.meta_npu` PASS; full ctest green.**

- [ ] **Step 3: Commit** `feat: npu_transformer instantiates examples/blas leaves`

---

### Task 7: Docs + roadmap

- Update `docs/grammar-v0.1.md` (instance syntax), `docs/roadmap.md` (minimal hierarchy checkbox), `examples/README.md`, mark spec `implemented`.

- [ ] **Step 1: Docs.**

- [ ] **Step 2: Commit** `docs: document blas library and module instantiation`

---

## Spec coverage

| Spec item | Tasks |
|-----------|-------|
| Parse instance | 1–2 |
| Multi-file run | 3 |
| Flatten elab | 4 |
| examples/blas/* | 5 |
| NPU uses BLAS | 6 |
| Golden + docs | 6–7 |

## Plan self-review

- Top-file rule matches main.c change.
- Array port connection called out as required.
- NPU golden preserved.
- No vs-view changes required.
