# Design: Toy NPU — single-layer Transformer (hard attention)

Date: 2026-09-17  
Status: implemented  
Related: `docs/superpowers/specs/2026-09-17-param-matmul-arrays-design.md`, `docs/superpowers/specs/2026-09-17-typed-views-anno-design.md`

## Goal

Add a **teaching-scale NPU demo**: one Verilog module that runs a **tiny Transformer layer** on `vs`, with clocked phases and `@vs` matrix boards in vs-view.

Pipeline:

`X → Q,K,V → S=Q·Kᵀ → hard-attn (row argmax one-hot) → O=A·V → H=O·W1 → ReLU(H) → Y=H·W2`

## Approved choices

| Topic | Choice |
|-------|--------|
| Product shape | Mini inference core (A), evolved to toy Transformer layer (C) |
| Softmax | **Hard attention**: per-row argmax → one-hot (1) |
| Delivery | Single module + **clocked state machine** (no hierarchy) |
| Activation | ReLU on FFN hidden |

## Non-goals

- Real softmax / exp / division pipelines
- Multi-head, positional encoding, multi-layer stacks
- Module instantiation (elab has no hierarchy yet)
- New language features beyond what matmul already needs (`parameter`, `for`, 2D arrays, `if/else`)
- `case` statements (use `if/else` phase chain)

## Geometry (defaults)

| Param | Default | Meaning |
|-------|---------|---------|
| `T` | 2 | tokens (sequence length) |
| `D` | 2 | model dim |
| `H` | 2 | FFN hidden dim (= `D` for v1 simplicity, or separate `HF`) |
| `W` | 8 | element width for activations/weights (accumulators wider, e.g. `2*W` or `3*W`) |

Public element naming (existing convention): `x_i_j`, `q_i_j`, …, `y_i_j`.

## Data path

1. **Inputs (forced or regs after load):** `X[T×D]`, `Wq[D×D]`, `Wk[D×D]`, `Wv[D×D]`, `W1[D×HF]`, `W2[HF×D]`
2. **QKV:** `Q=X·Wq`, `K=X·Wk`, `V=X·Wv`
3. **Scores:** `S=Q·Kᵀ` (`S[T×T]`)
4. **Hard attention:** for each row `r`, find `c* = argmax_c S[r][c]` (ties → lowest index); `A[r][c*] = 1`, else 0
5. **Attend:** `O = A·V` (`O[T×D]`)
6. **FFN:** `H = O·W1` → `H = max(H, 0)` → `Y = H·W2`

## Control

Ports: `clk`, `rst`, `start`; outputs `done`, `phase[3:0]`.

| phase | Name | Action |
|------:|------|--------|
| 0 | idle | wait for `start` |
| 1 | qkv | compute Q, K, V |
| 2 | score | `S = Q · Kᵀ` |
| 3 | hard | build one-hot `A` |
| 4 | attn | `O = A · V` |
| 5 | ffn1 | `H = O · W1` |
| 6 | relu | elementwise ReLU on `H` |
| 7 | ffn2 | `Y = H · W2`; `done=1` |
| 8 | hold | hold until `rst` or new `start` |

One phase per rising `clk` after `start` (combinational work for that phase completes in the same cycle via blocking/`always @*` helper regs, or computed inside the clocked `always` with blocking assigns — match existing sim style used by `sum_1_to_100` / counters).

**Reset:** clears `done`, `phase←0`, optionally zeros outputs.

## Example file

`examples/npu_transformer.v` — annotated with `@vs view matrix` for at least:

`X, Q, K, V, S, A, O, Y` (optional `H`).

Ops may use multiple `@vs op matmul` lines where useful (e.g. score, attn, ffn).

## Host stimulus (demo)

```bash
./build/vs run examples/npu_transformer.v --threads 2 --until <enough> \
  --clock clk=10 --reset rst=20 \
  --force start=...   # or pulse via short force window if supported \
  --force x_0_0=... (and weights) \
  --watch done,phase,y_0_0,y_0_1,y_1_0,y_1_1 \
  --trace build/trace_npu.jsonl
```

If `start` cannot be pulsed easily with current `--force` (constant hold), use **`start` high for first N cycles via reset-release pattern** or an internal `start` tied to `!rst` for one-shot after reset — document the chosen stimulus in the example README.

**Preferred v1 stimulus:** after reset deasserts, automatic one-shot start on first cycles (`reg started`), so demo needs only clock/reset + data forces.

## Golden test

Hand-computed tiny integers for default `T=D=HF=2` with fixed X/weights (document numbers in example header comments). CTest `sim.npu_transformer` asserts `done=1` and expected `y_*`.

Keep all existing tests green.

## Visualization

- vs-view Typed views for matrices; scrubber shows fill-in across phases as values commit.
- Timeline shows `phase` / `done` if watched and traced (commit events).

## Risks

| Risk | Mitigation |
|------|------------|
| Bit-width overflow | Use wide accumulators; pick small golden values |
| Argmax tie-break | Spec: lowest column index wins |
| Force cannot pulse start | Auto one-shot after reset |
| Large `@vs` meta | Soft-cap already; T=D=2 is fine |

## Success criteria

1. `examples/npu_transformer.v` simulates under `vs run`
2. CTest checks golden `Y` and `done`
3. Trace meta includes matrix views; vs-view shows them
4. Spec/plan/docs linked from `examples/README.md` and roadmap bullet

## Decisions log

- NPU shape: toy Transformer layer (user C)
- Softmax substitute: hard attention / row argmax (user 1)
- Architecture: clocked FSM in one module (recommended approach 2)
- ReLU included in FFN
