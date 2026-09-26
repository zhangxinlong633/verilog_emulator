# Design: GPT-2 Host + NPU GEMM tile on `vs`

Date: 2026-09-26  
Status: approved (pending implementation)  
Related: `docs/superpowers/specs/2026-09-17-npu-blas-hierarchy-design.md`, `examples/tiny_npu/`, `examples/blas/`

## Goal

Run **real GPT-2 (124M)** on the host, and simulate an **NPU GEMM kernel** in `vs` on a **real activation/weight tile** exported from that forward pass. Prove bit-exact (integer) agreement between Python golden and RTL `blas_gemm`.

This is **not** “full GPT-2 inside Verilog.” Host owns the model; RTL owns one hardware-style matmul on live GPT-2 data.

## Approved choices

| Topic | Choice |
|-------|--------|
| Product shape | Host GPT-2 + RTL tile kernel (approach A) |
| Model | `openai-community/gpt2` (124M) |
| Hook | Layer **0** MLP `c_fc`: `X · W` |
| Tile (default) | `N=8, K=64, M=64` from fixed offsets into real tensors |
| Quantization | Symmetric **int8** per-tensor; accumulate **int32** |
| Golden | Integer GEMM on the same int8 A/B (not float reference) |
| RTL style | Combinational top + `blas_gemm` (optional thin FSM later) |
| CI | Commit generated RTL inits + expects; CTest offline (no HF download) |

## Non-goals

- Embedding full GPT-2 / tokenizer / sampling loop in Verilog
- Floating-point softmax / LayerNorm / residual in RTL (phase 1)
- Multi-layer or multi-head attention end-to-end in `vs` (phase 2+)
- Changing vs-view architecture (reuse `@vs view` / `@vs op`)
- Requiring network access in CTest

## Architecture

```text
┌──────────── Host (Python) ─────────────────────────┐
│  transformers GPT-2 forward (full model)            │
│  hook block0.mlp.c_fc → float X, W                  │
│  slice tile → quantize int8 → emit Verilog + golden │
└────────────────────┬────────────────────────────────┘
                     │ examples/gpt2_npu/*
                     ▼
┌──────────── vs / RTL ───────────────────────────────┐
│  gemm_tile.v  +  blas_gemm                          │
│  initial A/B  →  C[N×M] int32                       │
│  vs-view matrices A, B, C                           │
└─────────────────────────────────────────────────────┘
```

## Quantization contract

For a float tensor `X`:

1. `s = max(abs(X)) / 127` (if max==0, `s=1`)
2. `q = round(clip(X/s, -127, 127))` → int8
3. RTL and Python golden both compute `C[i][j] = Σ_k A[i][k] * B[k][j]` in **int32** on these `q` values
4. Store `s_a`, `s_b` in `golden.json` for documentation only; **expects use int32 C**, not dequantized float

Same scale rule independently for A (activations) and B (weights). Weight matrix layout must match `blas_gemm` (`B[K×M]`).

## Tile selection

- After hook, activation `X_full` shape `[seq, 768]`, weight `W_full` shape `[768, 3072]` (GPT-2 `c_fc`).
- Defaults: `row0=0`, `col_k0=0`, `col_m0=0`, `N=8`, `K=64`, `M=64`.
- If `seq < N`, pad activation rows with zeros **before** quantize (document in JSON).
- Script flags: `--n/--k/--m/--offsets` to shrink if sim pending limits bite (prefer smaller tile over semantic change).

## Directory layout

```text
tools/gpt2_npu/
  README.md
  export_tile.py          # HF forward, hook, quant, emit
  requirements.txt        # torch, transformers (pinned lightly)

examples/gpt2_npu/
  README.md
  gemm_tile.v             # top: regs A/B, wire C, blas_gemm
  a_init.inc              # generated initial assigns (or inline)
  b_init.inc
  expect_c.inc
  golden.json
  forces.txt              # optional; prefer initial blocks

examples/blas/gemm.v      # reused

tests/sim/run_gpt2_npu_gemm.cmake
```

Root / `examples/README.md` / `docs/images/` updated when screenshot exists.

## RTL behavior

- Module `gpt2_npu_gemm_tile`:
  - `reg signed [7:0] a [0:N-1][0:K-1]`
  - `reg signed [7:0] b [0:K-1][0:M-1]`
  - `wire signed [31:0] c [0:N-1][0:M-1]` (or unsigned wire + two’s complement interpret; match existing `blas_gemm` signedness — **extend `blas_gemm` to signed int8 if current leaf is unsigned-only**)
- `initial` loads A/B from generated assigns
- Instantiate `blas_gemm #(.N(8),.K(64),.M(64),.WA(8),.WB(8),.CW(32))`
- `@vs` annotations for A/B/C + matmul op

**Signedness:** Phase 1 must support negative int8. If `blas_gemm` today treats operands as unsigned, add a signed variant or document truncation rules and quantize to **uint8** in `[0,255]` instead. **Preferred:** keep symmetric int8 and teach `blas_gemm` (or `blas_gemm_s8`) to sign-extend multiplies into int32. Decision at implement time: inspect current `blas_gemm`; if unsigned-only, add `examples/blas/gemm_s8.v` rather than breaking existing tests.

## Export script

`tools/gpt2_npu/export_tile.py`:

1. Load tokenizer + `GPT2LMHeadModel` from `openai-community/gpt2`
2. Encode fixed prompt (default `"Hello"`), `model.eval()`, no grad
3. Forward hook on `transformer.h[0].mlp.c_fc` capturing input activation and using `module.weight`
4. Slice, quantize, integer GEMM golden
5. Write `examples/gpt2_npu/` artifacts + `golden.json` (shapes, scales, offsets, commit-friendly)

Pin note in README: regenerating needs network/cache for weights once; committed artifacts make CI offline.

## Testing

| Test | Check |
|------|--------|
| `sim.gpt2_npu_gemm` | `vs run gemm_tile.v blas/gemm*.v --threads 1 --until 40 --watch c_*` contains every `expect_c.inc` line |
| Manual | Optional re-export then diff expects |

Do not download HF inside CMake/CTest.

## Success criteria

1. Documented story: full GPT-2 on host; RTL runs real layer0 `c_fc` int8 tile
2. CTest green offline with committed generates
3. vs-view shows A/B/C; screenshot under `docs/images/gpt2-npu-gemm-vs-view.png` when demo’d
4. Existing `tiny_npu` / `npu_transformer` / blas tests still pass

## Risks

| Risk | Mitigation |
|------|------------|
| `8×64×64` blows `VS_MAX_PENDING` / runtime | Script defaults overridable; shrink to `4×32×32` |
| Unsigned GEMM vs negative weights | `gemm_s8` leaf or uint8 quant path |
| HF API / weight layout (`Conv1D` vs Linear) | Handle GPT-2 `Conv1D` weight transpose explicitly in export |
| Large binary-ish inits in git | Accept for demo size; keep tile modest |

## Phase 2 (out of scope now)

- Tile from `c_attn` or `QKᵀ` / attention softmax
- Clocked NPU FSM streaming multiple tiles
- Residual + LayerNorm toys fed by host activations
