---
name: reference-auto-port-eval
description: "Sibling project at ../auto-port-eval — FLINT→TS porting eval with 90 measured runs, Pareto plots, and the original golden-master harness this project extends."
metadata: 
  node_type: memory
  type: reference
  originSessionId: ae70f34d-7cbf-4f33-9344-c0dbf83add5c
---

Located at `/home/tobiasosborne/Projects/auto-port-eval`. This is the predecessor project; mpfr-ts is its scaled-up application.

**Files worth reading before changing the mpfr-ts harness:**

| Path | Purpose |
|---|---|
| `PLAN.md` | Original design doc, function selection rationale |
| `RESULTS.md` | 90-run measurements, Pareto frontier, extrapolation to all-of-FLINT |
| `HANDOFF.md` | Hard-won traps: Cyrillic homoglyphs, no per-test timeout, C-macro out-args, multi-valid outputs |
| `eval/harness/runner.ts` | The TS runner+grader template. mpfr-ts will fork & extend (worker isolation, perf grading, NaN handling) |
| `eval/golden_master/common.h` | xorshift64 PRNG, JSONL emit helpers, now_ns timing |
| `eval/driver/prompts.py` | L1/L2/L3 templates (note: hardcoded `/home/tobias/...` path bug — wrong username here) |
| `eval/driver/finalize.py` | Pattern for extracting TS from agent reply, grading, appending to runs.jsonl |
| `eval/driver/manifest.py` | Cartesian product of (model × effort × function × seed) → run_ids |
| `eval/functions/n_xgcd/spec.json` | Example of struct-output spec (vs scalar like n_pow) |
| `eval/reference_ports/correct/*.ts` | Hand-written reference ports — sanity check that harness is non-tautological |

**Key empirical findings to keep in mind:**

- sonnet L3 = $0.04/port median, 99.9% grade — the Pareto winner for porting
- TDD (L3) lifts haiku from 0.84 → 0.997; barely moves opus
- One bug survived sonnet L1 AND L2 but died at L3 (n_xgcd): verification > capability
- Per-test worker isolation was a TODO in auto-port-eval — should be built into mpfr-ts from day one given infinite-loop risk in transcendental porting

**How to apply:** When designing an mpfr-ts harness component, check first if auto-port-eval has it. Fork the pattern; don't reinvent. Lessons in HANDOFF.md (the seven "things I learned the hard way") apply directly.
