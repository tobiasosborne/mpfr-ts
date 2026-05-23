---
name: decision-failure-policy
description: Ralph loop tries sonnet L3 first; on failure after N iterations auto-escalates to opus L3 once; on second failure parks the function with a failure record.
metadata: 
  node_type: memory
  type: project
  originSessionId: ae70f34d-7cbf-4f33-9344-c0dbf83add5c
---

**Decision:** Two-tier failure policy in the ralph loop:

1. **Attempt 1:** sonnet L3 (TDD), up to 6 iterations. If composite ≥ 0.95 → mark done.
2. **Attempt 2 (auto-escalation):** opus L3, up to 6 iterations. If composite ≥ 0.95 → mark done (with escalation flag).
3. **Park:** record in `parked.md` with last grade.json, blocked dependents, root cause snippet.

**Why:** User explicitly chose "Auto-escalate to opus L3 once, then park" over halt-for-review and over park-immediately. Reasoning: RESULTS.md showed opus saturates earlier (opus L1 ≈ sonnet L3), so opus L3 should have meaningful odds of fixing what sonnet L3 cannot. Costs ~5× more on failed fns but bounded since it's once-per-fn.

**How to apply:**
- Loop never halts on a single failure. Throughput is paramount once the harness is proven.
- Each parked function blocks all dependents until human review. The call graph ordering ([[project-mpfr-goal]]) should minimize this.
- Failure records must capture: model+effort attempted, iteration count, last grade.json, first_error string, blocked dependent fns.
- Estimated cost ceiling per parked fn: ~$0.30 (sonnet ~$0.05 + opus ~$0.25). With a 600-fn library and ~5% failure rate, parked-fn cost should stay under $10.
- During pilot phase ([[project-mpfr-goal]]), failures should halt for human review — switch to auto-escalate policy only after the harness is proven sound on 10-20 functions.
