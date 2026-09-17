# Toy NPU Transformer Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship `examples/npu_transformer.v` — a clocked toy Transformer layer (hard attention + FFN/ReLU) that runs under `vs` and shows matrices in vs-view via `@vs`.

**Architecture:** Single module FSM (`phase` 0…8). Each rising edge after auto-start advances one phase; matmuls/argmax/ReLU use nested `for` and 2D arrays already supported. No new language features. Flattened element names `x_i_j` for `--force`/`--watch`.

**Tech Stack:** Existing `vs` (C11) + CTest + `@vs` / vs-view TypeScript.

**Spec:** `docs/superpowers/specs/2026-09-17-npu-transformer-design.md`

## Global Constraints

- No `case`, no hierarchy, no real softmax.
- Hard attention: per-row argmax, **lowest column wins ties**.
- Defaults `T=2`, `D=2`, `HF=2`, `W=8`; accumulator regs wide enough for products (use `[31:0]` or `[4*W-1:0]` for S/H/Y temps).
- Auto one-shot start after `rst` deasserts (do not rely on pulsing `start` via `--force`).
- Keep all existing CTests green; examples live in `examples/`.
- Follow `AGENTS.md` / directory READMEs.

## Golden vector (document in example header)

```
X=[[1,2],[3,4]]
Wq=Wk=Wv=W1=W2=I=[[1,0],[0,1]]

Q=K=V=X
S=Q·Kᵀ=[[5,11],[11,25]]
A=[[0,1],[0,1]]   # row argmax → col1
O=A·V=[[3,4],[3,4]]
H=ReLU(O·W1)=[[3,4],[3,4]]
Y=H·W2=[[3,4],[3,4]]
```

Expect watch: `done=1`, `y_0_0=3 y_0_1=4 y_1_0=3 y_1_1=4`.

## File map

| File | Role |
|------|------|
| `examples/npu_transformer.v` | DUT + `@vs` annotations |
| `tests/sim/run_npu_transformer.cmake` | Golden watch asserts |
| `CMakeLists.txt` | `sim.npu_transformer` |
| `examples/README.md`, `docs/roadmap.md` | Docs |
| Spec status → implemented | After green |

---

### Task 1: Failing CTest harness (red)

**Files:**
- Create: `tests/sim/run_npu_transformer.cmake`
- Modify: `CMakeLists.txt`
- Create stub: `examples/npu_transformer.v` (minimal empty/idle module so parse may fail or watch miss — **prefer** stub that parses but does not produce golden yet)

**Interfaces:**
- Consumes: `vs run` CLI (`--clock clk=10 --reset rst=20 --until … --force … --watch …`)
- Produces: red `sim.npu_transformer`

- [ ] **Step 1: Write cmake** asserting substrings in stdout:

```cmake
# VS, SRC required
execute_process(
  COMMAND ${VS} run ${SRC}
    --threads 2 --until 400
    --clock clk=10 --reset rst=20
    --force x_0_0=1 --force x_0_1=2 --force x_1_0=3 --force x_1_1=4
    --force wq_0_0=1 --force wq_0_1=0 --force wq_1_0=0 --force wq_1_1=1
    --force wk_0_0=1 --force wk_0_1=0 --force wk_1_0=0 --force wk_1_1=1
    --force wv_0_0=1 --force wv_0_1=0 --force wv_1_0=0 --force wv_1_1=1
    --force w1_0_0=1 --force w1_0_1=0 --force w1_1_0=0 --force w1_1_1=1
    --force w2_0_0=1 --force w2_0_1=0 --force w2_1_0=0 --force w2_1_1=1
    --watch done,phase,y_0_0,y_0_1,y_1_0,y_1_1
  OUTPUT_VARIABLE OUT RESULT_VARIABLE RC ...)
# require done=1 and y_0_0=3 y_0_1=4 y_1_0=3 y_1_1=4
```

- [ ] **Step 2: Add stub `examples/npu_transformer.v`** with ports `clk,rst` and `done` only (so run works but golden fails) **or** skip stub and let missing file fail — prefer stub with `done=0` forever.

- [ ] **Step 3: Wire CTest `sim.npu_transformer`; run — expect FAIL.**

- [ ] **Step 4: Commit** `test: add failing npu_transformer golden harness`

---

### Task 2: Implement FSM + datapath in `examples/npu_transformer.v`

**Files:**
- Replace: `examples/npu_transformer.v`

**Interfaces:**
- Ports: `clk`, `rst`; outputs `done`, `phase[3:0]`; inputs as 2D arrays (or flattened forces into arrays via ports named so elab emits `x_0_0` …).
- Match `examples/matmul.v` port style for 2D arrays.

**Implementation notes:**

1. Parameters `T=2,D=2,HF=2,W=8`.
2. Arrays: `x,wq,wk,wv,w1,w2` inputs; regs `q,k,v,s,a,o,h,y` (widths: products use at least 16–32 bits).
3. `reg started`; on `posedge clk`: if `rst` → clear; else if `!started` → `started<=1`, `phase<=1` (or go idle→1); else advance `phase` through 1…8.
4. Each phase body: nested `integer` loops for matmul / transpose-mul / argmax / relu (blocking assigns inside clocked block are OK if existing sim treats them like `sum_1_to_100` NBA — **prefer NBA `<=` for state**, and for matrix updates use same style as working examples).

**Critical:** Inspect how `matmul.v` (combo) vs `sum_1_to_100.v` (NBA) behave. For multi-cycle FSM, follow **NBA for `phase`/`done`/`started`**. For filling matrices in a phase, either:
- compute into temps with blocking then NBA to regs in same edge, or
- use `always @*` pure combo mats selected by `phase` (combo QKV always from X) while sequential only advances `phase`.

**Recommended simpler structure (fewer NBA hazards):**

```verilog
// Combo: always compute q,k,v,s,a,o,h,y from current x/weights (full pipeline combo)
// Seq: only phase counter + done latch for "display stage"
```

But spec wants **phased** fill for visualization. So:

```verilog
always @(posedge clk) begin
  if (rst) ...
  else begin
    if (phase == 1) begin /* compute q,k,v with for-loops using <= */ end
    else if (phase == 2) begin /* s */ end
    ...
    if (phase != 0 && phase != 8) phase <= phase + 1;
  end
end
```

Use **blocking** for loop-carried argmax temps (`best`, `best_j`) inside the edge; NBA for array elements if sim supports NBA to indexed lvalues — **verify in Task 2**. If indexed NBA unsupported, use blocking assigns to arrays (like matmul's `always @*` style) inside the clocked block.

- [ ] **Step 1: Implement full module with header golden comments + `@vs` views** for X,Q,K,V,S,A,O,Y (and weights optional).

```verilog
// @vs view matrix X array=x rows=T cols=D
// @vs view matrix Q array=q rows=T cols=D
// ...
// @vs op matmul out=S left=Q right=KT  /* if KT not a view, skip op or add view K_T */
```

If `Kᵀ` is awkward, omit some `@vs op` lines; views alone suffice.

- [ ] **Step 2: Manual run** with the cmake force list; confirm watch line.

```bash
./build/vs run examples/npu_transformer.v --threads 2 --until 400 \
  --clock clk=10 --reset rst=20 \
  ...forces... --watch done,phase,y_0_0,y_0_1,y_1_0,y_1_1
```

Expected: `done=1` and y values `3,4,3,4`.

- [ ] **Step 3: `ctest -R npu_transformer` PASS; full ctest green.**

- [ ] **Step 4: Commit** `feat: add toy NPU transformer example with hard attention`

---

### Task 3: Meta / vs-view smoke + docs

**Files:**
- Optionally: `tests/sim/run_meta_npu.cmake` asserting `"views"` in trace
- Modify: `examples/README.md`, `docs/roadmap.md`, spec status `implemented`
- Modify: `AGENTS.md` one line if needed

- [ ] **Step 1: Generate trace and confirm meta has views** (`head -1` + python/json).

- [ ] **Step 2: Optional CTest `sim.meta_npu`.**

- [ ] **Step 3: Docs + README how to run vs-view on `build/trace_npu.jsonl`.**

- [ ] **Step 4: Full ctest; commit** `docs: document toy NPU transformer demo`

---

## Spec coverage

| Spec item | Task |
|-----------|------|
| FSM phases 0–8 | 2 |
| Hard attention argmax | 2 |
| FFN + ReLU | 2 |
| Auto start after rst | 2 |
| Golden Y | 1–2 |
| `@vs` matrices | 2–3 |
| Docs / roadmap | 3 |

## Plan self-review

- Golden math fixed and testable.
- No new grammar work assumed.
- Stimulus avoids start pulse limitation.
- Indexed assign / NBA risk called out with verification step.
