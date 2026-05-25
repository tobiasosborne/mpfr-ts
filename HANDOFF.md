# Handoff — 124 ports, 0 pending, Production validated; next: callgraph re-seed + larger batch

You are picking up mpfr-ts after a short Production-validation
session that shipped both pending rows the previous HANDOFF queued
(`mpfr_frac` and `mpfr_rint_trunc`), proving the Production-mode
discipline works at small scale. State.db: **124 done, 5 blocked,
0 pending.** All known approximation helpers parked under ADR 0002;
all known runtime-system stubs (`mpfr_allocate_func` family) parked
as no-ops in the immutable TS surface.

The session was the first true Production-phase batch. Cost burn
≈ $0.88 on subagent dispatches; auto-escalate count 0/2 (both
green on first sonnet attempt). Process was serial-orchestrator
plus per-step subagent dispatch, with the orchestrator running the
verification + state.db steps directly. See worklog 012 for the
full risk-monitoring write-up.

## ⚠ Three gotchas — read first

1. **0 pending rows in state.db.** The natural next move is to
   re-run callgraph (`python3 eval/driver/callgraph.py`) to seed
   more functions into the pending pool. Without this, the ralph
   loop has nothing to pick. There are ~470 MPFR functions in
   `mpfr/src/` not yet in state.db.

2. **`.gitignore` `/mpfr/` and `eval/functions/*/golden.jsonl`
   are anchored.** Per worklog 011 gotcha 1, audit if you add
   colliding paths. Goldens regenerate locally via
   `bash eval/golden_master/run_all.sh --filter <fn>`.

3. **`bd` doesn't auto-export to JSONL on manual commits.** Run
   `bd export -o .beads/issues.jsonl` before `git commit` or use
   `ralph.py --commit-batch`/`--ship`. Tracked by `mpfr-ts-i8e`.

## TL;DR — first 10 minutes

```bash
git pull --rebase
cat PHASE.md                                          # → Production
cat HANDOFF.md                                        # this file
cat docs/worklog/012-first-production-batch.md        # latest session
cat docs/worklog/011-phase-transition.md              # the phase-transition log
cat docs/adr/0002-approximation-helper-grading.md     # the load-bearing ADR

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|5 done|124

cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q   # 119 pass
bash eval/golden_master/build.sh                      # all drivers compile
bun x tsc --noEmit | grep -v "eval/driver/mutators.ts" # clean

# Smoke-check the carve-out is still live:
python3 eval/driver/mutate.py --function mpfr_swap --port src/ops/swap.ts \
  --golden eval/functions/mpfr_swap/golden.jsonl
# Expected: gate_passed: True (vacuous)

# Verify both newly-shipped ports:
bun eval/harness/runner.ts --function mpfr_frac --port src/ops/frac.ts \
  --golden eval/functions/mpfr_frac/golden.jsonl --output /tmp/v1.json
bun eval/harness/runner.ts --function mpfr_rint_trunc --port src/ops/rint_trunc.ts \
  --golden eval/functions/mpfr_rint_trunc/golden.jsonl --output /tmp/v2.json
# Both: composite=1.0000, 153/153.

bd ready                                              # 12 issues
```

## Next-session priority sequence

### Priority 1: Re-run callgraph to seed more pending rows

State.db is empty of pending work. Before any further Production
batches can run, the callgraph needs to be re-extracted so more
functions enter the pending queue.

```bash
cd /home/tobiasosborne/Projects/mpfr-ts
python3 eval/driver/callgraph.py --update-state-db
```

(Confirm the exact CLI; previous session may have had a slightly
different invocation. The script is at `eval/driver/callgraph.py`,
525 fns documented per HANDOFF as of worklog 010.)

**Expected outcome**: ~20-50 new pending rows seeded into state.db,
covering the next tier of arithmetic, conversion, misc, and
substrate-class functions. The topo-rank should naturally pick
small dependency-satisfied functions first.

**Deliverable**: state.db re-seeded, dashboard query confirms
non-zero pending. Estimated effort: ~15 minutes if the script
already works; ~1-2 hours if it needs updating for the current
state.db schema.

### Priority 2: Second Production batch (5-8 functions, serial)

Worklog 012's recommendation: one more serial batch to triangulate
cost burn at moderate scale, then switch to `ralph.py --parallel 8`
for the bulk of Production. The second batch should include:

- At least one substrate-class function (to test that golden-driver-
  substitute pattern is alive in the live pipeline — currently only
  validated via `mpfr_div2_approx` from before ADR 0002).
- At least one transcendental-class function if any are ready (to
  exercise the auto-escalate path, which has 0 actual tests in
  Production data so far).
- The remaining picks chosen by topo-rank.

**Risk monitoring to continue**:
- Cyrillic check on every generated file.
- Cost burn running total.
- Auto-escalate count (now 0 across batch 1; if any function
  escalates, document why).
- Mutate gate must be `killed` or `vacuous`. `survived` ports get
  flagged for human golden review (not auto-shipped).

**Deliverable**: 5-8 ports shipped, worklog 013, HANDOFF refresh.
Estimated cost: ~$3-7 in subagent dispatches.

### Priority 3: Switch to `ralph.py --parallel 8` for batch 3

After two clean serial batches confirm the discipline holds, shift
to auto-pilot for throughput. Production caveats (cost cap, escalate
rate) are instrumented via state.db queries against the `runs`
table.

### Priority 4-N: Continue picking up bd P3 issues opportunistically

Same backlog as worklog 011:

- `mpfr-ts-i8e` — git pre-commit hook for bd auto-export
- `mpfr-ts-9di` — mutate.py option (b)/(c) for applied-but-survived
  ports (still optional)
- `mpfr-ts-18x`, `mpfr-ts-2ls`, `mpfr-ts-ai4`, `mpfr-ts-d6o`,
  `mpfr-ts-e4j`, `mpfr-ts-sr4` — harness polish

None block scale-out.

## What's working now (don't change)

| Component | Path | Notes |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen |
| Worker isolation | `eval/harness/worker.ts` | Solid |
| Grader | `eval/harness/runner.ts` | Strict equality |
| AST gate | `eval/harness/ast_check.ts` | Solid |
| Substrate | `src/internal/{mpn,mpfr}/` | 19 files |
| Callgraph | `eval/driver/callgraph.py` | **stale; re-run for batch 2** |
| State DB | `eval/state.db` | 129 rows; 124 done, 5 blocked, 0 pending |
| gen_spec | `eval/driver/gen_spec.py` | 207 LOC; arg order `(c_source_path, function_name)` |
| mutate.py | `eval/driver/mutate.py` | gate_status: killed/vacuous/survived |
| ADR 0001, 0002 | `docs/adr/` | Both load-bearing |
| **NEW**: mpfr_frac port + golden | `src/ops/frac.ts`, `eval/functions/mpfr_frac/` | 153 cases, killed |
| **NEW**: mpfr_rint_trunc port + golden | `src/ops/rint_trunc.ts`, `eval/functions/mpfr_rint_trunc/` | 153 cases, killed |

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR.
- Modify ADR 0001 or 0002 without writing a successor ADR.
- Skip `bd export -o .beads/issues.jsonl` before `git commit` (or
  use the `--commit-batch`/`--ship` ralph.py paths instead).
- Add dead code to port files to satisfy mutate.py. The vacuous-
  pass carve-out exists for genuinely trivial bodies. Survived
  status flags golden insufficiency — the curator's job, not the
  port's.
- Run `ralph.py --parallel N` with N > 10. Stay at <=8 for cost
  discipline.
- Dispatch the same subagent prompt verbatim if it disconnected
  mid-stream — tighten the prompt first (worklog 012 lesson 2).

## Pickup-on-different-device checklist

1. `git clone git@github.com:tobiasosborne/cs/mpfr-ts.git`
2. `sudo apt install -y libmpfr-dev libgmp-dev sqlite3`
3. `curl -fsSL https://bun.sh/install | bash`
4. `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. `pip install pytest`
6. `bd bootstrap --yes && bd hooks install && bd import`
7. Smoke-check:
   - `cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q` # 119 pass
   - `bash eval/golden_master/build.sh` # all drivers compile
   - `bun eval/harness/runner.ts --function mpfr_frac --port src/ops/frac.ts --golden eval/functions/mpfr_frac/golden.jsonl --output /tmp/v.json` # composite=1.0
   - `python3 eval/driver/mutate.py --function mpfr_swap --port src/ops/swap.ts --golden eval/functions/mpfr_swap/golden.jsonl` # vacuous pass
8. Read CLAUDE.md -> this file -> `docs/worklog/012-first-production-batch.md` -> `docs/worklog/011-phase-transition.md`.

## Open bd issues at session end (12 total)

Same as worklog 011 — no new issues filed this session.

P3 — harness polish:
- `mpfr-ts-9di` — partial closure (option (a) shipped; (b)/(c) deferred)
- `mpfr-ts-i8e` — git pre-commit hook
- `mpfr-ts-18x`, `mpfr-ts-2ls`, `mpfr-ts-ai4`, `mpfr-ts-d6o`,
  `mpfr-ts-e4j`, `mpfr-ts-sr4`

P4 — cleanup:
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`

`bd ready` for the live picture.

## One final thing

The first Production batch worked. Two functions shipped, both
green first try, both gates killed legitimately. The discipline
holds at small scale; the next batch tests it at moderate scale
(5-8 functions). The session-opener task is unglamorous but
prerequisite: re-run callgraph to seed the pending queue. After
that, batches can run as fast as cost discipline allows.

Worklog 012 includes specific lessons about subagent prompt sizing
(tighter is better — the first frac TS port subagent disconnected
on a long prompt; a tighter retry succeeded immediately) and about
the serial-orchestrator pattern (right for n=2, switch to `ralph.py
--parallel 8` once batch 3 confirms the discipline at scale).

Good luck.
