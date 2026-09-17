# Design: NPU + BLAS library via module instantiation

Date: 2026-09-17  
Status: implemented  
Related: `docs/superpowers/specs/2026-09-17-npu-transformer-design.md`, `docs/roadmap.md` (P1 hierarchy)

## Goal

Refactor the toy NPU so it **instantiates** a small BLAS-style RTL library instead of inlining matmul/argmax/ReLU loops. This clarifies the NPU (FSM + wiring) and makes ops reusable for slightly more realistic sizes.

## Approved choices

| Topic | Choice |
|-------|--------|
| Integration | **Real module instantiation** (A) |
| Layout | Independent directory `examples/blas/` |
| Library surface | **gemm + gemm_bt + relu + row_argmax** (tier 3) |
| Elab strategy | Minimal recursive flatten with `inst_` prefixes (approach 1) |

## Non-goals

- Full hierarchical sensitivity / nested process semantics
- Complete BLAS / floating point / DMA
- `generate` / `include`
- Changing vs-view (reuse existing `@vs` on NPU arch regs)

## Directory layout

```text
examples/
  blas/
    README.md
    gemm.v         // C = A · B
    gemm_bt.v      // C = A · Bᵀ
    relu.v         // Y = max(X, 0) elementwise
    row_argmax.v   // A = row-wise one-hot argmax(S)
  npu_transformer.v  // FSM top; instantiates blas_* leaves
```

Optional later: `examples/blas/gemm_wrap.v` as a tiny standalone top for unit tests.

## BLAS leaf contracts (combinational)

All leaves: `parameter` sizes + 2D unpacked ports; body `always @*` + `for` (same style as `examples/matmul.v`).

### `blas_gemm`

`C[N×M] = A[N×K] · B[K×M]`

### `blas_gemm_bt`

`C[N×M] = A[N×K] · Bᵀ` with `B[M×K]` so `c[i][j] += a[i][kk]*b[j][kk]`

### `blas_relu`

`Y[N×M]` from `X[N×M]`; clear when MSB set (same convention as current NPU) or `if (x < 0)` if signed compare is reliable — **document chosen rule**; prefer MSB-of-wide-reg temp like today.

### `blas_row_argmax`

`A[T×T]` from `S[T×T]`; per row lowest-index wins ties.

## NPU top

Keep clocked FSM phases; replace inline loops with instantiations whose combo outputs feed wires `q_w`, `k_w`, …; on the matching `phase`, NBA-capture into arch regs `q`, `k`, …

Example shape:

```verilog
blas_gemm #(.N(T), .K(D), .M(D), .W(W)) u_q (
  .a(x), .b(wq), .c(q_w)
);
// ...
always @(posedge clk) begin
  if (phase == 1) begin
    // copy q_w,k_w,v_w into q,k,v (for-loops of NBA/blocking to elements)
    phase <= 2;
  end
  ...
end
```

`@vs` annotations remain on **architectural** arrays (`x,q,k,v,s,a,o,y`), not instance internals.

## Language / elab work

1. **Parse** module instantiation items:
   - `mod_name #( .PARAM(expr), ... ) inst_name ( .port(expr), ... );`
   - Positional ports optional later; **named ports required in v1**.
2. **Multi-file**: `vs_parse_files` already takes N paths; `vs run a.v b.v …` must load all. Elab finds leaf modules by name in the design.
3. **Elab**:
   - Choose top: module with `clk`+`rst` among loaded modules, or the last file’s first module — **decision: top = first module in the first CLI file** (keep today’s behavior) while other files contribute definitions.
   - For each instance: bind parameters; map ports (support **whole 2D array** port connections); recursively elaborate child into netlist with signal rename `inst_localname` / element `inst_base_i_j`.
   - Child combo processes become netlist processes (names prefixed).
4. **Sim**: unchanged if netlist stays flat after expand.

Public naming for child internals: `u_q_c_0_0` style if needed; NPU should wire leaves to parent-visible regs so demos rarely watch instance guts.

## CLI / tests

```bash
./build/vs run examples/npu_transformer.v \
  examples/blas/gemm.v examples/blas/gemm_bt.v \
  examples/blas/relu.v examples/blas/row_argmax.v \
  --clock clk=10 --reset rst=20 ...
```

CMake may pass an explicit file list or a small `examples/npu_transformer.files` — **prefer explicit list in `run_npu_transformer.cmake`**.

Tests:

| Test | Expect |
|------|--------|
| `sim.blas_gemm` | Tiny A×B via a one-module wrapper or gemm as sole top |
| `sim.npu_transformer` | Same golden `Y=[[3,4],[3,4]]` as today |
| Existing suite | Still green |

Optional smoke: `T=D=4` with small integers; assert `done` + a few `y_*` (not required for v1 merge).

## Risks

| Risk | Mitigation |
|------|------------|
| Array-port connection complexity | Reuse elab array registry; connect by matching base names / shapes |
| Process wakeups after flatten | Child `always @*` must list/star-sense flattened signals |
| Multi-file top selection | Document: first file = top |
| Large rename bugs | Unit-test gemm alone first |

## Success criteria

1. `examples/blas/*.v` exist and gemm has a CTest  
2. NPU instantiates BLAS leaves (no duplicated matmul loops in NPU body)  
3. Original NPU golden still passes  
4. Grammar/roadmap note module instantiation (minimal)  
5. Spec marked implemented after landing  

## Decisions log

- Instantiation: real (user A)  
- Directory: independent `examples/blas/`  
- Ops: gemm, gemm_bt, relu, row_argmax (user 3)  
- Elab: flatten with prefixes (recommended approach 1)
