---
name: decision-perf-gate
description: Perf is a separate grade dimension. Time-gate is moderate-to-strict. Slow-but-correct fns re-attempted later for optimization. Per-function speed visibility is required.
metadata: 
  node_type: memory
  type: project
  originSessionId: ae70f34d-7cbf-4f33-9344-c0dbf83add5c
---

**Decision:** The grader produces a perf grade SEPARATE from correctness, tiered by function class:

- **Arithmetic (add/sub/mul/div/sqrt/fma):** strict gate ~50× C time
- **Conversion (set_*, get_*, init/clear/cmp):** strict gate ~50× C time
- **Transcendental (exp/log/sin/cos/atan/pow):** moderate gate ~200× C time
- **Misc (string I/O, formatting, rare ops):** lenient gate ~500× C time

A function with composite correctness ≥ 0.95 but failing the perf gate is **not parked** — it's marked `correct-but-slow` and re-queued for an optimization pass later. The ralph loop has a separate "optimize slow fns" mode.

**Why:** User explicitly said: "I want the pass to be moderate to strict, the grade should be noted and subpar functions we get reattempted later. It must be clear which functions are doing well, and which are slow." This rules out the lenient gate but also rules out parking slow fns — they're a follow-on workload, not a failure.

**How to apply:**
- `spec.json` per function MUST include a `class` field: `"arithmetic" | "conversion" | "transcendental" | "misc"`. The grader picks the gate from this.
- `grade.json` must report `correctness`, `edge_correctness`, `perf_grade`, `composite_correctness` (= 0.6 corr + 0.4 edge_corr), and `composite_overall` (correctness × perf_grade).
- The ralph loop's "next function" picker uses `composite_correctness >= 0.95` to decide done-ness. `perf_grade < 1.0` queues the function for later optimization, not parking.
- The state DB needs a `perf_status` column: `'fast' | 'slow' | 'unmeasured'`.
- Dashboard / report must surface counts: "412 correct & fast / 87 correct but slow / 23 parked".
