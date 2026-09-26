# GPT-2 Host + NPU GEMM Tile Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Export a real GPT-2 layer0 `c_fc` int8-quantized tile from HuggingFace, run it through `blas_gemm` on `vs`, and CTest-match integer golden.

**Architecture:** Python hook on `transformer.h[0].mlp.c_fc` → slice → symmetric int8 → store as sign-extended 32-bit elements → combinational `gpt2_npu_gemm_tile` + existing `blas_gemm`. CI uses committed artifacts (no HF in CTest).

**Tech Stack:** Python3 + `torch`/`transformers` (export only); C11 `vs`; CMake/CTest; vs-view `@vs`.

**Spec:** `docs/superpowers/specs/2026-09-26-gpt2-npu-tile-design.md`

## Global Constraints

- Full GPT-2 stays on host; RTL only runs one GEMM tile.
- Golden = integer GEMM on quantized tiles (not float).
- CTest offline: commit `examples/gpt2_npu/*` generates.
- Default tile **N=4, K=32, M=32** (8×64×64 exceeds current `VS_MAX_PENDING` on combo eval; document; allow CLI override when regenerating).
- Sign-extended **32-bit** element storage so unsigned `*` in sim matches int8 signed math.
- No AI attribution in commits; per-dir READMEs; examples under `examples/gpt2_npu/`.
- Keep existing tiny_npu / npu / blas tests green.

## File map

| File | Role |
|------|------|
| `tools/gpt2_npu/export_tile.py` | HF forward, hook, quant, emit |
| `tools/gpt2_npu/requirements.txt` | torch, transformers |
| `tools/gpt2_npu/README.md` | how to regenerate |
| `examples/gpt2_npu/gemm_tile.v` | top + `@vs` |
| `examples/gpt2_npu/{a,b}_init.inc` | generated inits |
| `examples/gpt2_npu/expect_c.inc` | CTest expects |
| `examples/gpt2_npu/golden.json` | metadata |
| `examples/gpt2_npu/README.md` | demo commands |
| `tests/sim/run_gpt2_npu_gemm.cmake` | runner |
| `CMakeLists.txt` | `sim.gpt2_npu_gemm` |
| `examples/README.md`, `tools/README.md`, root `README.md` | pointers |

---

### Task 1: Export script + committed tile artifacts

**Files:**
- Create: `tools/gpt2_npu/export_tile.py`
- Create: `tools/gpt2_npu/requirements.txt`
- Create: `tools/gpt2_npu/README.md`
- Create: `examples/gpt2_npu/*` (via running the script)

**Interfaces:**
- Produces: `gemm_tile.v`, `a_init.inc`, `b_init.inc`, `expect_c.inc`, `golden.json`
- Consumes: HF `openai-community/gpt2`, prompt default `"Hello"`

- [ ] **Step 1: Implement `export_tile.py`**

Core logic (must handle GPT-2 `Conv1D` weight layout: `weight` is `[n_embd, 4*n_embd]` for `c_fc` — use `module.weight` as `W` with matmul `X @ W` matching HF):

```python
# Pseudocode structure — full file in repo
# quant_sym_i8(x) -> q_i8, scale
# slice X[row0:row0+N, k0:k0+K], W[k0:k0+K, m0:m0+M]
# A32/B32 = sign-extend q to int32
# C = A32 @ B32 (numpy int64/int32)
# emit initial lines: a[i][j] = 32'sd<v>;  (or 32'd for non-neg)
# expect: c_i_j=<decimal>
```

Defaults: `--n 4 --k 32 --m 32 --prompt Hello --layer 0`.

If `seq < N`, zero-pad rows before quant.

- [ ] **Step 2: Run export (needs network/cache once)**

```bash
pip install -r tools/gpt2_npu/requirements.txt
python3 tools/gpt2_npu/export_tile.py --out-dir examples/gpt2_npu
```

Expected: stderr mentions shapes; files written under `examples/gpt2_npu/`.

- [ ] **Step 3: Smoke integer check**

```bash
python3 -c "import json; g=json.load(open('examples/gpt2_npu/golden.json')); print(g['N'],g['K'],g['M'],g['C'][0][:4])"
```

- [ ] **Step 4: Commit** artifacts + tool (after Task 2 wires CTest if preferred one commit at end — prefer commit tool+artifacts here)

```bash
git add tools/gpt2_npu examples/gpt2_npu
git commit -m "feat: export GPT-2 c_fc tile for NPU GEMM demo"
```

---

### Task 2: RTL top + CTest

**Files:**
- Create/overwrite: `examples/gpt2_npu/gemm_tile.v` (export may emit full module; or thin wrapper — **export emits complete `gemm_tile.v`**)
- Create: `tests/sim/run_gpt2_npu_gemm.cmake`
- Modify: `CMakeLists.txt` (add test after tiny demos)
- Modify: READMEs

**`gemm_tile.v` shape:**

```verilog
module gpt2_npu_gemm_tile;
  reg  [31:0] a [0:3][0:31];
  reg  [31:0] b [0:31][0:31];
  wire [31:0] c [0:3][0:31];
  initial begin
`include "a_init.inc"  // ONLY if include works — vs may NOT support `include
  end
  blas_gemm #(.N(4),.K(32),.M(32),.WA(32),.WB(32),.CW(32)) u (.a(a),.b(b),.c(c));
endmodule
```

**If `include` unsupported:** export inlines all assigns inside `gemm_tile.v` (preferred; matches `tiny_npu` demos).

- [ ] **Step 1: Add CMake runner**

```cmake
# tests/sim/run_gpt2_npu_gemm.cmake
# vs run SRC BLAS/gemm.v --threads 1 --until 40 --watch <all c_i_j>
# file(STRINGS expect_c.inc) and require each pair in OUT
```

Watch list: all `c_0_0` … `c_3_31` (128 names) — generate watch string in export as `watch.txt` one line comma-separated, CMake `file(READ)`.

- [ ] **Step 2: Register CTest `sim.gpt2_npu_gemm`**

- [ ] **Step 3: Run**

```bash
cmake -B build && cmake --build build
ctest --test-dir build -R sim.gpt2_npu_gemm --output-on-failure
```

Expected: PASS. If pending overflow, re-export with `--n 4 --k 16 --m 16` and update artifacts.

- [ ] **Step 4: Docs** — `examples/README.md`, `tools/README.md`, short blurb in root `README.md` (screenshot optional follow-up).

- [ ] **Step 5: Full ctest + commit**

```bash
ctest --test-dir build --output-on-failure
git add examples/gpt2_npu tools/gpt2_npu tests/sim/run_gpt2_npu_gemm.cmake CMakeLists.txt README.md examples/README.md tools/README.md
git commit -m "feat: GPT-2 tile GEMM on vs with offline CTest"
```

---

### Task 3 (optional follow-up): vs-view screenshot

- Run with `--trace`, open vs-view, save `docs/images/gpt2-npu-gemm-vs-view.png`, link from README.

---

## Spec coverage

| Spec item | Task |
|-----------|------|
| Host GPT-2 + export | 1 |
| int8 quant + int32 GEMM golden | 1 |
| Tile defaults / shrink | 1 (4×32×32) |
| RTL + blas_gemm | 2 |
| Offline CTest | 2 |
| READMEs | 2 |
| Screenshot | 3 optional |
| gemm_s8 leaf | N/A — use WA=32 sign-extended |
| Phase-2 attn | out of scope |

## Self-review

- No TBD placeholders in steps.
- Pending limit documented; tile sized accordingly.
- GPT-2 `Conv1D` called out in Task 1.
