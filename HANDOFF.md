# Handoff — 122 ports + Phase = Production; next: first Production mega-batch

You are picking up mpfr-ts after a short, high-leverage session that
closed all three priorities the previous HANDOFF queued: ADR 0002
resolving the approximation-helper grading question (Priority 1),
mutate.py vacuous-pass carve-out for trivial-body ports (Priority 2,
option (a)), and the long-overdue Pilot → Production transition
(Priority 4). State.db: **122 done, 5 blocked, 2 pending** (unchanged
— this session was architecture, not new ports). PHASE.md now reads
`Production`.

This session also exposed that HANDOFF framings can drift from
architecture: Priority 1's "wire-format inequality extension" turned
out to be unneeded once the live `mpfr_div2_approx` port (composite=
1.0, 129 cases) was inspected. Always check the state DB and the
live ports before committing to a HANDOFF-suggested approach.

## ⚠ Three gotchas — read first

1. **`.gitignore` `/mpfr/` pattern.** Anchored to `/mpfr/` since
   `cb65ebe`. If you add a directory whose name collides with an
   ignored pattern, audit the gitignore. Last bite: ~12 hours of
   silently-dropped substrate files in the 50→85 session.

2. **`bd` commands don't auto-export to JSONL.** `ralph.py
   --commit-batch` and `--ship` do this automatically. Manual `git
   commit` skips it. **Always run `bd export -o .beads/issues.jsonl`
   before manual commits**, or prefer `--commit-batch`/`--ship`.
   `mpfr-ts-i8e` tracks the pre-commit-hook fix.

3. **Phase is now Production.** Ralph loop dispatches should run as
   `python3 eval/driver/ralph.py --phase production --parallel 8`.
   PIL.* rules no longer apply. Cost cap ($50/session) and auto-
   escalate failure-rate cap (10%/24h) ARE in effect — instrument
   them on the first mega-batch.

## TL;DR — first 10 minutes

```bash
git pull --rebase
cat PHASE.md                                          # → Production
cat HANDOFF.md                                        # this file
cat docs/worklog/011-phase-transition.md              # latest session
cat docs/adr/0002-approximation-helper-grading.md     # the new ADR
cat docs/adr/0001-spec-merge-policy.md                # still load-bearing

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|5 done|122 pending|2

bun test src/internal/mpfr/flags.test.ts              # 11 pass
cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q   # 119 pass (up from 114)
bash eval/golden_master/build.sh                      # all drivers compile
bun x tsc --noEmit | grep -v "eval/driver/mutators.ts" # clean

# Smoke-check the carve-out is live:
python3 eval/driver/mutate.py --function mpfr_swap --port src/ops/swap.ts \
  --golden eval/functions/mpfr_swap/golden.jsonl
# Expected: gate_passed: True (vacuous)

bd ready                                              # 12 issues (was 13; -52u closed)
```

## Next-session priority sequence

### Priority 1 (recommended): First Production mega-batch

Ralph loop on auto-escalate, ~6-10 functions, instrument cost burn
and auto-escalate rate per CLAUDE.md Production caveats. Pickup
candidates from state.db pending rows:

- `mpfr_frac` (rank 198, misc)
- `mpfr_rint_trunc` (rank 420, misc)
- Plus next-rank picks from the topo-sorted ready list.

**Deliverable**: a clean batch landing (~6-10 ports), worklog 012
documenting the first Production-mode run, mutate.py + ast_check
+ AST schema enforcement all green. First real measurement of the
auto-escalate rate caveat noted in CLAUDE.md.

Estimated cost: ~$5-15 (single mega-batch). Estimated effort:
1-2 hours.

### Priority 2: `mpfr-ts-9di` remainder (option b or c)

mutate.py carve-out shipped option (a) — zero-applicable carve-out.
Applied-but-survived cases (sqrt1, set_inf, get_d1) remain
`gate_status='survived'`. This is correct classification but flags
ports as "needs human review" rather than auto-passing. If
Production mega-batches start hitting these patterns at scale,
land option (b) (complexity floor: <2 mutations exempt and flag for
human golden review) or option (c) (per-spec `mutation_prove_exempt:
true`).

**Defer** unless Production batches show >2-3 survived-status ports
per batch.

Estimated effort: 1-2 hours if needed.

### Priority 3: `mpfr-ts-i8e` (git pre-commit auto-export)

A `.git/hooks/pre-commit` shim that runs `bd export -o
.beads/issues.jsonl` before any manual commit. The "gotcha 2"
above; persistent QoL fix worth ~15 minutes.

### Priority 4: Other open P3 issues from previous HANDOFF

Still open, still optional:

- `mpfr-ts-18x` — comparison-swap multi-site
- `mpfr-ts-2ls` — value_codec scalar strings
- `mpfr-ts-ai4` — runner n_throw conflation
- `mpfr-ts-d6o` — callgraph misses mpn_* substrate fns
- `mpfr-ts-e4j` — expected_throw codec for domain-error goldens
- `mpfr-ts-sr4` — enforce Rule 7 tag minimums at grade time

None block scale-out. Pick up only if a Production batch surfaces
one of these as a live blocker.

## What's working now (don't change)

| Component | Path | Notes |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen; never modify without ADR |
| Worker isolation | `eval/harness/worker.ts` | Solid |
| Grader | `eval/harness/runner.ts` | Strict equality; ADR 0002 ratifies this |
| AST gate | `eval/harness/ast_check.ts` | Solid |
| Substrate | `src/internal/{mpn,mpfr}/` | 19 files; div2_approx is the canonical golden-driver-substitute reference |
| Callgraph | `eval/driver/callgraph.py` | 525 fns; re-run if you touch `mpfr/src/` |
| State DB | `eval/state.db` | 129 rows; 122 done, 5 blocked, 2 pending |
| gen_spec | `eval/driver/gen_spec.py` | 207 LOC; live-validated by shadow trials 1+2 |
| gen_spec wired into prep prompt | `eval/driver/ralph.py` `_render_prep_prompt` | Step 6 stable |
| Flag-state register | `src/internal/mpfr/flags.ts` | 88 LOC + 11 tests |
| mutators | `eval/driver/mutators.ts` | 185 LOC; 7 mutations |
| **mutate.py (updated)** | `eval/driver/mutate.py` | now reports gate_status ∈ {killed, vacuous, survived}; vacuous = zero-applicable carve-out per bd `mpfr-ts-9di` |
| validate_specs | `eval/driver/validate_specs.py` | 167 LOC |
| calibrate | `eval/driver/calibrate.py` | 149 LOC |
| run_all.sh | `eval/golden_master/run_all.sh` | 85 LOC |
| ADR 0001 | `docs/adr/0001-spec-merge-policy.md` | 17/17 prediction across 2 trials |
| **ADR 0002 (new)** | `docs/adr/0002-approximation-helper-grading.md` | Golden-driver-substitute pattern formalized; inequality grading not built |

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR.
- Modify ADR 0001 or 0002 without writing a successor ADR.
- **Revive the inequality-grader wire format extension** unless a
  concrete divergent-algorithm port (not faithful-substitute)
  emerges. ADR 0002 §Revisit documents the exact conditions.
- Disable harness gates to make a port pass. Fix the port instead.
- Skip mutation-prove. Now that the vacuous carve-out exists,
  there's no excuse for trivial ports to fail the gate — and any
  applied-but-survived port should be human-reviewed.
- Add dead code to port files purely to satisfy mutate.py
  (carried over from worklog 010). Gaming the gate destroys signal
  value. The vacuous carve-out is the *legitimate* way out for
  truly trivial bodies.
- Dispatch all N sonnets simultaneously when N > 10. Waves of 6-10
  remain the cost-disciplined default.
- Modify the shipped infrastructure tools (gen_spec, mutators,
  mutate, validate_specs, calibrate, prep-prompt wiring) without
  bd-driven justification.
- Flip `PHASE.md` away from `Production` without writing
  `docs/worklog/NNN-phase-transition.md` (Rule 14).

## Pickup-on-different-device checklist

1. `git clone git@github.com:tobiasosborne/mpfr-ts.git`
2. `sudo apt install -y libmpfr-dev libgmp-dev sqlite3`
3. `curl -fsSL https://bun.sh/install | bash`
4. `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. `pip install pytest`
6. `bd bootstrap --yes && bd hooks install && bd import`
7. Smoke-check:
   - `bun x tsc --noEmit | grep -v "eval/driver/mutators.ts"` (clean)
   - `bun test src/internal/mpfr/flags.test.ts`  # 11 pass
   - `cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q`  # 119 pass
   - `bash eval/golden_master/build.sh`  # all drivers compile
   - `python3 eval/driver/mutate.py --function mpfr_swap --port src/ops/swap.ts --golden eval/functions/mpfr_swap/golden.jsonl` # gate_passed: True (vacuous)
8. Read CLAUDE.md → this file → `docs/worklog/011-phase-transition.md`
   → `docs/adr/0002-approximation-helper-grading.md` → `docs/adr/0001-spec-merge-policy.md`

## Open bd issues at session end (12 total)

P3 — harness polish:
- `mpfr-ts-9di` — **PARTIAL CLOSURE**: option (a) shipped; option (b)/(c) deferred
- `mpfr-ts-i8e` — git pre-commit hook for bd auto-export
- `mpfr-ts-18x` — comparison-swap multi-site
- `mpfr-ts-2ls` — value_codec scalar strings
- `mpfr-ts-ai4` — runner n_throw conflation
- `mpfr-ts-d6o` — callgraph misses mpn_* substrate fns
- `mpfr-ts-e4j` — expected_throw codec for domain-error goldens
- `mpfr-ts-sr4` — enforce Rule 7 tag minimums at grade time

P4 — cleanup:
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`

Closed this session:
- `mpfr-ts-52u` — ADR 0002 supersedes (inequality framing was a misread)

`bd ready` for the live picture.

## One final thing

This session's most valuable output isn't the ADR or the carve-out —
it's the validation that **HANDOFF framings need verification before
execution**. The "inequality grader extension" P1 looked load-bearing
in the previous HANDOFF; one investigation pass against the live
state.db and the `div2_approx` port revealed it was unnecessary. The
right move was to write an ADR explaining why, not implement an
unneeded ~150 LOC feature. Carry this discipline into Production: the
HANDOFF is a starting hypothesis, not a spec.

Production scale-out is ready. The harness is sound, the
discipline holds, the ADRs are in place. The natural next move is to
pick up `mpfr_frac` / `mpfr_rint_trunc` and run the first Production
mega-batch with full instrumentation.

Good luck.
