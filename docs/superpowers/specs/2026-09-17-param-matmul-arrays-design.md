# Design: Parameterized matrix multiply (parameter + for + arrays)

Date: 2026-09-17  
Status: approved  
Related: `docs/superpowers/specs/2026-09-17-typed-views-anno-design.md`, `docs/grammar-v0.1.md`

## Goal

Support a **general** matrix multiply in RTL and in vs-view, not only a hand-unrolled 2×2:

- `parameter` for dimensions and element width
- Procedural `for` inside `always @*`
- Unpacked arrays (1D then 2D)
- `@vs` matrix views bound to arrays (auto-expand cells from parameters)

## Non-goals (this design)

- `generate for` / `generate if`
- Runtime-dynamic sizes (N changing during sim)
- Arrays deeper than 2D, queues, associative arrays
- Full IEEE parameter elaboration (defparam, hierarchical overrides beyond module `#(...)`)
- Evaluating `@vs expr` in C (unchanged: strings to vs-view)

## Approved choices

| Topic | Choice |
|-------|--------|
| Generality | True parameterized RTL (not script-unrolled forever) |
| Loops | Procedural `for` in `always` / `initial` (not required: `generate`) |
| Storage | Real unpacked arrays (not packed flat bus) |
| Delivery | **Three phases** (parameter → 1D+for → 2D+matmul+`@vs`) |

## Target example (end state)

```verilog
// @vs view matrix A array=a rows=N cols=K
// @vs view matrix B array=b rows=K cols=M
// @vs view matrix C array=c rows=N cols=M
// @vs op matmul out=C left=A right=B
module matmul #(
  parameter N = 2,
  parameter K = 2,
  parameter M = 2,
  parameter W = 8
)(
  input  wire [W-1:0] a [0:N-1][0:K-1],
  input  wire [W-1:0] b [0:K-1][0:M-1],
  output reg  [2*W-1:0] c [0:N-1][0:M-1]
);
  integer i, j, k;
  always @* begin
    for (i = 0; i < N; i = i + 1)
      for (j = 0; j < M; j = j + 1) begin
        c[i][j] = 0;
        for (k = 0; k < K; k = k + 1)
          c[i][j] = c[i][j] + a[i][k] * b[k][j];
      end
  end
endmodule
```

Default parameters keep the existing numeric check: A=[[1,2],[3,4]], B=[[5,6],[7,8]] → C=[[19,22],[43,50]].  
`examples/matmul2x2.v` remains as the flat regression; new `examples/matmul.v` is the parameterized form.

## Phase 1 — `parameter` + constant folding

### Language

- Module-header `# ( parameter ID = const_expr , ... )` and/or body `parameter ID = const_expr;`
- `const_expr`: integer literals and references to already-defined parameters (same module), plus `+ - * /` needed for `2*W`

### Elab / sim

- Parameter table on the elaborated module
- Fold parameter refs in packed ranges (`[W-1:0]`) and later in for-bounds / array bounds
- Unknown / non-constant → diagnostic error (or warning + fail elab)

### Tests

- Parse ok: module with `# (parameter W=8)` and `input [W-1:0] x`
- Sim: existing counter unchanged; tiny module that uses `W` in a range

## Phase 2 — Procedural `for` + 1D unpacked arrays

### Language

- `integer` declarations (loop indices)
- `for (init; cond; step) statement` with blocking assigns in init/step
- Unpacked 1D: `reg [W-1:0] mem [0:N-1];` (bounds constant / parameter-folded)
- Indexing: `mem[i]` as rvalue and lvalue (element, not bit-select of a vector)

### Distinguishing select kinds

- Packed bit/part select on a scalar vector (today) vs unpacked element index (new) — elab tags signals with `kind=scalar|array1d` and width/length metadata

### Sim

- Extend `exec_stmt` with `VS_STMT_FOR`
- Array storage: contiguous elements in sim state (one logical signal with `nelem`, or `nelem` sibling values keyed by base name + index — prefer **one signal descriptor + element value array** for clearer `@vs` later)

### Tests

- `for` that fills `mem[i]=i` and watches last element
- Parse fail: `for` without integer / non-const bound where required

## Phase 3 — 2D arrays + matmul + `@vs`

### Language

- `reg [W-1:0] a [0:N-1][0:K-1];` and `a[i][k]`
- Ports may use the same unpacked dimensions (ANSI style as in target example)

### Example + CTest

- `examples/matmul.v` with default 2×2; forces via expanded element names or a small host helper
- CTest: same expected products as `sim.matmul2x2`
- Optional smoke: `N=3,K=3,M=3` with a few forced entries (or keep 2×2 only if force CLI stays flat-name based)

### `@vs` extension

Keep existing:

```text
// @vs view matrix A rows=2 cols=2 cells=a00,a01;a10,a11
```

Add:

```text
// @vs view matrix A array=a rows=N cols=K
```

- `rows`/`cols` may be integer literals **or parameter names** resolved at meta emit time
- C expands `cells` to element names using a stable naming scheme for the viewer/forces, e.g. `a[0][1]` in meta **or** flattened `a_0_1` if the sim public name is flat — **decision: public element name is `a[i][j]` in waves/meta**; CLI `--force` should accept the same spelling (may need argv quoting) **or** provide `a_0_1` alias. Prefer **`a_0_1` aliases** for shell-friendly forces while meta `cells` lists `a_0_1` consistently with today.

**Resolved naming:** unpacked element `a[i][j]` is exposed as signal `a_i_j` (decimal indices) for force/watch/trace compatibility with the current flat CLI. `@vs array=` expands to those names. Document in `examples/README.md`.

### vs-view

- No change required if meta still carries expanded `cells` grids
- Optional later: understand `array=` without expansion (not required if C expands)

## Architecture sketch

```text
parse  → AST (param, for, array dims, index exprs)
elab   → netlist signals (scalar | array1d | array2d) + param env
sim    → eval index → element slot; exec for
anno   → resolve array= + param rows/cols → cells[][]
trace  → meta.views as today
vs-view → matrix UI as today
```

### Force / watch CLI (phase 3)

Until richer force syntax exists, use flattened names:

```bash
./build/vs run examples/matmul.v --force a_0_0=1 --force a_0_1=2 ...
```

## Risks

| Risk | Mitigation |
|------|------------|
| Scope creep into full IEEE | Hard non-goals; phase gates |
| Bit-select vs array-index ambiguity | Elab typing on base identifier |
| Force CLI and `a[0][1]` | Use `a_0_1` public names |
| Large N blows meta/forces | Soft cap warn (e.g. >16×16) |

## Success criteria

1. Phase 1–3 tests green; existing suite still green  
2. `examples/matmul.v` (defaults 2×2) matches `[[19,22],[43,50]]`  
3. Typed view still shows A×B=C for the parameterized example  
4. Grammar / roadmap docs updated for the new subset  

## Decisions log

- User: generality = parameterized RTL (B)  
- User: procedural `for` (3), not generate-first  
- User: real arrays (A)  
- Delivery: three-phase hybrid (recommended approach 2)  
- Element naming for tooling: `a_i_j`
