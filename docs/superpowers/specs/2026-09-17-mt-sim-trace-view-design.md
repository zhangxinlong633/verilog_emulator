# Design: Minimal Multi-thread Sim + Trace + Node View

**Date:** 2026-09-17  
**Status:** Approved plan — implementing as P2-lite-MT  
**Program:** `vs`

## Goals

1. `vs run` simulates a flat module (counter) with host `--clock` / `--reset`.
2. Scheduler uses a **pthread worker pool** inside each delta cycle (same path for `--threads 1`).
3. Emit **JSONL** process dump (`--trace`); `-v` mirrors key events to stderr.
4. `tools/vs-view` (Node) shows **event timeline + slim waveforms**.

## Non-goals (later)

Full elaborate/hierarchy, VCD, general `#delay`, lock-free scaling, full IEEE NBA proofs.

## Concurrency

- Main thread: time advance, sorted wake list, barrier, NBA commit, trace.
- Workers: evaluate woken processes; emit thread-local pending updates only.
- After join: main merges pending by stable process id order; conflicts → warning in trace.

## Trace JSONL

One JSON object per line: `t`, `d`, `tid`, `op`, plus op-specific fields (`proc`, `sig`, `val`, `msg`).

Ops: `eval`, `nba`, `ba`, `commit`, `clock`, `reset`, `warn`, `done`.

## CLI

```text
vs run <file.v> [--threads N] [--until T] [--trace path.jsonl]
              [--clock name=period] [--reset name=cycles] [--watch a,b]
              [-v]
```

## Components

| Path | Role |
|------|------|
| `src/elab/` | Flat symbol table + process list from AST |
| `src/sim/` | Values, scheduler, pool, trace |
| `tools/vs-view/` | HTTP UI over JSONL |

## Success

- CTest: threads 1 vs 4 same final watched values for counter.
- Manual: Node UI shows `q` incrementing.
