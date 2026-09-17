# Design: vs — Verilog Simulator

**Date:** 2026-09-17  
**Status:** Spec written; awaiting user review before implementation plan  
**Program name:** `vs` (Verilog Simulator)

## 1. Goals

Build a **from-scratch**, long-term Verilog simulator in the spirit of a small QEMU: clear stages, interpretive execution first, and a path toward IEEE 1364 completeness.

| Decision | Choice |
|----------|--------|
| Ambition | Long-term completeness (Icarus-class semantics over time) |
| Language | Pure **C11** |
| Execution model | **Interpreted**, event-driven (not Verilator-style compile-to-C) |
| Frontend | **flex + bison** (mature, grammar-centric) |
| First milestone (P0) | Solid **parser + AST**; dump via `vs` |
| First sim subset (P2 target) | Minimal RTL: `wire`/`reg`, `assign`, `always`/`initial`, blocking + NBA, edge events |

Non-goals for P0:

- Running simulation
- Preprocessor (`` `define `` / `` `include ``)
- SystemVerilog
- Timing/`#delay`/SDF
- Full IEEE grammar

## 2. Product surface

### 2.1 Binary

Single CLI: **`vs`**.

```text
vs [options] <file.v> [<file.v> ...]
```

P0 options:

| Option | Meaning |
|--------|---------|
| `--dump-tokens` | Print token stream (lexer debug) |
| `--dump-ast` | Print normalized AST |
| `-h` / `--help` | Usage |
| `-V` / `--version` | Version string |

Later (P2+): default action becomes simulate; dump flags remain for debug.

### 2.2 Exit codes

| Code | Meaning |
|------|---------|
| 0 | Success (no errors) |
| 1 | Lex/parse (or later semantic) errors |
| 2 | Usage / I/O / internal failure |

## 3. Architecture (QEMU-like stages)

```text
  .v source
      │
      ▼
  ┌─────────┐   flex        ┌─────────┐   bison       ┌─────┐
  │  Lexer  │──────────────▶│ Parser  │──────────────▶│ AST │
  └─────────┘               └─────────┘               └──┬──┘
                                                         │
                    P0 stops here (dump / tests)         │
                                                         ▼
                                                   ┌──────────┐
                                                   │ Elaborate│  P1
                                                   └────┬─────┘
                                                        │
                                                        ▼
                                               ┌────────────────┐
                                               │ Event scheduler│  P2
                                               │ + evaluators   │
                                               └────────┬───────┘
                                                        │
                                                        ▼
                                                 results / VCD
```

**Ownership rules:**

- Frontend owns tokens → AST construction.
- Elaborate owns name binding, widths, hierarchy (consumes AST, produces netlist/IR).
- Sim owns time wheel / event queue and value change propagation.
- Stages communicate only through well-defined IR headers under `include/vs/`.

## 4. Repository layout

```text
vs/                          (repo root: verilog_emulator)
├── CMakeLists.txt
├── README.md
├── include/vs/
│   ├── ast.h
│   ├── diag.h
│   ├── arena.h
│   └── strtab.h
├── src/
│   ├── front/
│   │   ├── lex.l
│   │   ├── parse.y
│   │   ├── ast.c
│   │   ├── dump_ast.c
│   │   └── dump_tokens.c
│   ├── elab/                (P1)
│   ├── sim/                 (P2)
│   ├── util/
│   │   ├── arena.c
│   │   ├── diag.c
│   │   └── strtab.c
│   └── main.c               (vs CLI)
├── tests/
│   └── parse/
│       ├── ok/
│       └── fail/
├── docs/                    (detailed docs; see §9)
└── docs/superpowers/specs/  (this file)
```

Generated files (`lex.yy.c`, `parse.tab.c`, `parse.tab.h`) are **not** committed; CMake runs flex/bison.

## 5. Frontend (flex + bison)

### 5.1 Tooling

- **flex** with `%option reentrant bison-bridge bison-locations noyywrap nounput noinput`
- **bison** ≥ 3.0 with `%define api.pure full`, `%locations`, `%define parse.error verbose`
- Scanner type `yyscan_t` passed explicitly; parser remains reentrant for future multi-file compile

### 5.2 Semantic values

- `%union` holds only small POD / pointers (`ast_node_t *`, `const char *`, integers for sizes)
- Real structures allocated via `ast_*_new(arena, ...)`
- Identifiers interned in `strtab` at lex time

### 5.3 Locations

Every AST node stores `vs_loc_t { filename, first_line, first_column, last_line, last_column }` for diagnostics and future source maps.

### 5.4 Error handling

- Lexer reports illegal characters / bad numbers immediately via `vs_diag`
- `yyerror` forwards bison messages + location into `vs_diag`
- Limited `error` token recovery so one mistake does not cascade endlessly
- Tests that expect success require **zero** errors; AST dump compared only then

## 6. AST (P0)

Tagged-union nodes, arena-allocated.

### 6.1 Design root

- `vs_design_t`: list of modules (P0: typically one file → one or more modules)

### 6.2 Module items (v0.1)

| Kind | Role |
|------|------|
| `VS_PORT` / port decls | `input`/`output`/`inout` + optional width |
| `VS_NET_DECL` | `wire` (+ width, name list) |
| `VS_REG_DECL` | `reg` (+ width, name list) |
| `VS_CONT_ASSIGN` | `assign` LHS = RHS |
| `VS_GATE_INST` | Primitive gates (parse OK; elab later) |
| `VS_MODULE_INST` | Instance stub (parse OK; bind in P1) |
| `VS_ALWAYS` / `VS_INITIAL` | Procedural blocks |
| Statements | `begin`/`end`, `if`/`else`, `=` / `<=`, event control |
| Expressions | ident, based number, unary/binary, bit/part select, concat |

### 6.3 Dump format

Normalized, deterministic text (stable field order, no pointer addresses) so golden tests can `diff`. Exact grammar of the dump is specified in `docs/ast.md` and frozen by tests.

## 7. Language subset v0.1 (parser must accept)

Supported:

- `module` / `endmodule`, port lists, `input`/`output`/`inout`
- `wire`, `reg`, packed range `[msb:lsb]`
- `assign`, blocking `=`, nonblocking `<=`
- `always`, `initial`, `begin`/`end`, `if`/`else`
- Event control: `@(posedge x)`, `@(negedge x)`, `@*`, `@(a or b or ...)`
- Expressions: arithmetic, bitwise, logical, compares, `{}` concat, bit/part select
- Numbers: decimal/hex/oct/bin with optional size and base (`8'hFF`, etc.)

Deferred:

- `` `define `` / `` `include `` / `` `ifdef ``
- `case` / loops / `task` / `function`
- `#delay`, `specify`, UDP
- `integer`/`real`/`time`, net types beyond `wire`
- Executing `$display` / `$finish` (may parse as syscalls later)

Full tables and examples: `docs/grammar-v0.1.md`.

## 8. Roadmap

| Phase | Deliverable | Done when |
|-------|-------------|-----------|
| **P0** | flex+bison, AST, `vs --dump-ast`, docs, parse golden tests | All `tests/parse` green |
| **P1** | Elaborate: bind ports/nets, widths, hierarchy | Simple counter elaborates; bad widths diagnosed |
| **P2** | Event-driven sim: sensitivity, BA/NBA, `initial`/`always` | Counter/FF simulate correctly |
| **P3+** | `#delay`, VCD, sys tasks, `case`/`for`, preprocessor, richer grammar | Toward IEEE 1364 |

## 9. Documentation set (detailed)

| Path | Content |
|------|---------|
| `docs/README.md` | Doc index |
| `docs/architecture.md` | Stages, boundaries, data flow |
| `docs/grammar-v0.1.md` | Subset + examples |
| `docs/ast.md` | Node field manual + dump format |
| `docs/frontend-flex-bison.md` | Reentrant setup, build, debugging |
| `docs/diagnostics.md` | Message format, exit codes, recovery |
| `docs/testing.md` | Golden test conventions |
| `docs/roadmap.md` | Phases and non-goals |
| `docs/contributing-syntax.md` | How to add a syntax production |
| `README.md` (root) | Quick start for `vs` |

## 10. Testing (P0)

- `tests/parse/ok/*.v` + `*.ast.expected`
- `tests/parse/fail/*.v` + `*.diag.expected` (substring match)
- CTest invokes `vs --dump-ast` / parse-only failure paths
- Minimum ok corpus: empty module, ports, wire/reg/assign, posedge always + NBA, if/begin, selects/concat

## 11. Risks and mitigations

| Risk | Mitigation |
|------|------------|
| Verilog grammar ambiguity in bison | Keep v0.1 small; prefer explicit productions; document conflicts |
| flex/bison version drift | Pin minimum versions in docs and CI later |
| AST shaped wrong for sim | Review AST against P2 needs now; avoid parse-only toy nodes |
| Scope creep to full IEEE in P0 | Hard defer list in grammar doc |

## 12. Approval record

- Goal: long-term complete, C11, interpretive (QEMU-like)
- Frontend: flex + bison
- P0: parse-first; program name **`vs`**
- Design sections 1–4 approved in brainstorming (2026-09-17)
- Naming update: CLI `vs` (not `ve_parse`)
