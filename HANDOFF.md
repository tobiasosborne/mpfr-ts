# Handoff — 122 ports + 2 shadow trials done; next: grader inequality ADR or third shadow

You are picking up mpfr-ts after a session that shipped all three
priorities the previous HANDOFF queued: flag-state API module
unblocking 4 predicate ports (Priority 1), gen_spec wired into
ralph.py's prep prompt (Priority 2 / step 6), and a second shadow-mode
trial validating both pieces live (Priority 3). State.db: **122 done,
5 blocked, 2 pending**. ADR 0001 has now held across 2 trials and 8
diverse functions at a 100% prediction rate.

The validation arc and its first live integration are complete. The
next session **either lands an ADR + extension to the grader for
approximation helpers**, **fixes mutate.py for trivial-body delegation
ports**, or **runs another shadow trial focused on misc-class data**
— see priority sequence below.

## ⚠ Three gotchas — read first

1. **`.gitignore` `mpfr/` pattern.** Anchored to `/mpfr/` since `cb65ebe`. If you add a directory whose name collides with an ignored pattern, audit the gitignore. Last bite: ~12 hours of silently-dropped substrate files in the 50→85 session.

2. **`bd` commands don't auto-export to JSONL.** `ralph.py --commit-batch` and `--ship` do this automatically. Manual `git commit` skips it. **Always run `bd export -o .beads/issues.jsonl` before manual commits**, or prefer `--commit-batch`/`--ship`. `mpfr-ts-i8e` tracks the pre-commit-hook fix.

3. **`PHASE.md` still says `Pilot`** despite 122 ports + 2 clean shadow trials + automation infra all green. Pilot's exit criterion (PIL.5: 10 functions clean in a single ralph-loop run with mutation-proved goldens) is comfortably exceeded. Rule 14 requires `docs/worklog/NNN-phase-transition.md` before flipping; this has been deferred across 4+ sessions. Not blocking — but now overdue enough that the next session should consider tackling it.

## TL;DR — first 10 minutes

```bash
git pull --rebase
cat PHASE.md                                          # → Pilot (still)
cat HANDOFF.md                                        # this file
cat docs/worklog/010-flags-step6-shadow2.md           # latest session
cat docs/reports/011-shadow-trial-2.md                # latest shadow data
cat docs/adr/0001-spec-merge-policy.md                # the still-load-bearing integration policy

sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|5 done|122 pending|2

bun test src/internal/mpfr/flags.test.ts              # 11 pass
cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q   # 114 pass
bash eval/golden_master/build.sh                      # all drivers compile
bun x tsc --noEmit | grep -v "eval/driver/mutators.ts" # clean (mutators.ts has pre-existing @types issues)

bd ready                                              # 13 issues; top: mpfr-ts-52u (P2 NEW)
```

## Next-session priority sequence

### Priority 1: `mpfr-ts-52u` (P2) — Grader inequality-output mode

Surfaced by shadow trial 2 when opus parked `mpfr_sqrt2_approx`. The
runner.ts `compareOutput` uses strict `===` on (value, ternary);
approximation helpers have contracts of the form "output lies in
[r0, r0+7]" — INEQUALITY, not equality. Affects ~5-10 functions in
upcoming batches (`mpfr_div2_approx`, various Newton-seed substrate
helpers, transcendental-class internal approximants).

**Three options to consider**:

- **(a) Extend the wire format**: add `output_range: [lo, hi]` as an
  alternative to `output: <exact>` in golden cases; runner accepts a
  result in the range. Requires changes to wire codec, runner, and
  potentially the mutate.py gate logic.
- **(b) Park them all**: formalise "approximation helpers are always
  parked" as ADR 0002. Loses class-coverage data on the substrate
  side but is the simplest change.
- **(c) Always-delegate**: the standalone-wire-form-with-delegate-to-
  unified-op pattern works when there's a unified TS public op the
  helper would feed into. Already validated for `mpfr_div_2`,
  `mpfr_sqrt1`, `mpfr_sqrt1n`. Limit: doesn't work when no public op
  subsumes the helper.

**Deliverable**: ADR 0002 documenting the decision + (if a) the wire-
format extension + runner update + at least one approximation helper
unparked. ~80-150 LOC code delta + 30 LOC tests + ADR.

Estimated effort: ~2-3 hours.

### Priority 2: `mpfr-ts-9di` (P3) — mutate.py for trivial-body ports

Hit live in shadow trial 2: `mpfr_sqrt1`'s pure-delegation body has no
applicable mutations after `mpfr-ts-omy`/`mpfr-ts-agn` regex fixes (no
`<`/`>` operators, no rnd dispatch, no value arithmetic). mutate.py
gate fails for "0 applicable mutations" — looks like a port problem
when it's a harness problem.

**Two paths**:

- Add a carve-out: gate passes if `composite >= 0.95 AND zero
  applicable mutations` (i.e., the gate explicitly recognises
  trivial-body ports rather than failing them).
- Synthesize delegation-targeting mutators: a "swap import target"
  mutator that changes `mpfr_sqrt` to `mpfr_set` (or similar)
  would give every delegation port a clean kill.

Path 1 is smaller. Path 2 is more elegant but adds a real mutator.

**Deliverable**: chosen path + tests + re-calibrate against the
sqrt1/sqrt1n + div_2 corpus.

Estimated effort: ~1-2 hours.

**Why P2 / P3 sequencing matters**: HANDOFF Priority 4 (first
replacement-mode trial) is gated on mutate.py providing reliable
discrimination signal without opus's broken-port deliverable. With
delegation ports failing the gate, replacement mode can't ship — keep
shadow mode (opus broken-port + mutate combined) until 9di lands.

### Priority 3: Third shadow trial — misc-class candidates

The two `pending` rows in state.db (`mpfr_frac` rank=198,
`mpfr_rint_trunc` rank=420) are natural candidates left over from the
P3 seed in this session. They sit in the misc class — the class report
009 flagged as mutate.py-weak. Picking them up would extend mutate.py
calibration data to the previously-undersampled misc-with-loops
shape (rather than misc-with-flags from shadow 1's predicates).

**Candidates** (next-rank pending or callgraph-implicit):
- `mpfr_frac` — misc; deps satisfied; fractional part of MPFR value
- `mpfr_rint_trunc` — misc; deps satisfied; truncate-toward-zero
- Additional candidates from `mpfr/src/rint.c`: `mpfr_rint`,
  `mpfr_round`, `mpfr_floor`, `mpfr_ceil` (all have port files
  already in `src/ops/` — check if they're done, would need
  status flip)

**Deliverable**: standard shadow-mode pattern (opus prep + sonnet wave
+ parallel gen_spec/mutate analysis + report 012).

Estimated cost: ~250-400K tokens. Same shape as shadow trials 1 + 2.

### Priority 4: PHASE.md transition (Rule 14)

Now 4+ sessions overdue. With 122 ports + 2 clean shadow trials +
clean infra, Pilot exit criterion (PIL.5) is comfortably exceeded.
Required: `docs/worklog/NNN-phase-transition.md` describing what the
Pilot has now proved (the engine works at 120+ scale, ADR 0001 holds
under live load, shadow mode reliably surfaces architectural gaps)
and what auto-escalate caveats remain (mostly mutate.py limitations,
documented in P3 above).

**Deliverable**: phase-transition worklog + flip `PHASE.md` from
`Pilot` to `Production`.

Estimated effort: ~30 minutes.

### Priority 5: First replacement-mode trial

Gated on Priority 2 (`mpfr-ts-9di`). Drop opus's broken-port deliverable
on a 3-5 function batch; rely on mutate.py alone for mutation-prove.
Conservative criterion: replacement mode is OK if mutate.py gate
agrees with the eventual ship decision on ≥80% of trial functions.

## What's working now (don't change)

| Component | Path | Notes |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen; never modify without ADR |
| Worker isolation | `eval/harness/worker.ts` | Solid |
| Grader | `eval/harness/runner.ts` | Solid; needs inequality-mode extension (mpfr-ts-52u) |
| AST gate | `eval/harness/ast_check.ts` | Solid |
| Substrate | `src/internal/{mpn,mpfr}/` | 19 files (was 18; +flags.ts this session) |
| Callgraph | `eval/driver/callgraph.py` | 525 fns; re-run if you touch `mpfr/src/` |
| State DB | `eval/state.db` | 129 rows; 122 done, 5 blocked, 2 pending |
| gen_spec | `eval/driver/gen_spec.py` | 207 LOC; live-validated by shadow trials 1+2 |
| **NEW**: gen_spec wired into prep prompt | `eval/driver/ralph.py` `_render_prep_prompt` | Step 6 done; verbatim ADR addendum + per-fn JSON scaffold |
| Flag-state register | `src/internal/mpfr/flags.ts` | 88 LOC + 11 tests; unblocked 4 predicates this session |
| mutators | `eval/driver/mutators.ts` | 185 LOC; 7 mutations; KNOWN WEAKNESS on pure-delegation ports (mpfr-ts-9di) |
| mutate orchestrator | `eval/driver/mutate.py` | 242 LOC; import-rewrite + module-init-failed detection |
| validate_specs | `eval/driver/validate_specs.py` | 167 LOC; gen_spec vs curator diff tool |
| calibrate | `eval/driver/calibrate.py` | 149 LOC |
| run_all.sh | `eval/golden_master/run_all.sh` | 85 LOC; --filter mode used live this session |
| ADR 0001 | `docs/adr/0001-spec-merge-policy.md` | Validated 17/17 prediction across 2 trials |

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR
- Flip `PHASE.md` from `Pilot` to `Production` without writing `docs/worklog/NNN-phase-transition.md` first (Rule 14)
- Disable harness gates to make a port pass. Fix the port instead
- Skip mutation-prove (broken < 0.55 ideally < 0.30 per worklog 006 #6)
- Re-introduce the absolute-path import bug (handled by `_promote_port`)
- Dispatch all N sonnets simultaneously when N > 10. Waves of 6-10 remain the cost-disciplined default
- **NEW**: Add dead code to port files purely to satisfy mutate.py. Surfaced in shadow trial 2 (sqrt1 had always-false post-condition added by sonnet). Gaming the gate destroys its signal value. Fix is in mutate.py (`mpfr-ts-9di`), not in port style.
- Drop opus's broken-port deliverable BEFORE replacement-mode trial validates the gate (Priority 5). Current data justifies keeping it; sqrt1 + sqrt1n demonstrate why
- Modify the shipped infrastructure tools (gen_spec, mutators, mutate, validate_specs, calibrate, prep-prompt wiring) without bd-driven justification — all validated and load-bearing

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
   - `cd eval/driver && /home/tobiasosborne/.local/bin/pytest tests/ -q`  # 114 pass
   - `bash eval/golden_master/build.sh`  # all drivers compile
8. Read CLAUDE.md → this file → `docs/worklog/010-flags-step6-shadow2.md` → `docs/reports/011-shadow-trial-2.md` → `docs/adr/0001-spec-merge-policy.md`

## Open bd issues at session end (13 total)

P2 — block scale-out:
- `mpfr-ts-52u` — **NEW**: grader inequality-output mode for approximation helpers (Priority 1 above)
- `mpfr-ts-i8e` — git pre-commit hook for bd auto-export

P3 — harness polish (one blocks replacement-mode trial):
- `mpfr-ts-9di` — **HIT LIVE** in shadow trial 2: mutate.py gate must pass trivial-body ports
- `mpfr-ts-18x` — comparison-swap multi-site
- `mpfr-ts-2ls` — value_codec scalar strings
- `mpfr-ts-ai4` — runner n_throw conflation
- `mpfr-ts-d6o` — callgraph misses mpn_* substrate fns
- `mpfr-ts-e4j` — expected_throw codec for domain-error goldens
- `mpfr-ts-sr4` — enforce Rule 7 tag minimums at grade time

P4 — cleanup:
- `mpfr-ts-00m`, `mpfr-ts-bqq`, `mpfr-ts-c6b`, `mpfr-ts-6zg`

`bd ready` for the live picture.

## One final thing

This session validates the validation infrastructure under live load.
Shadow trial 1 surfaced the flag-state gap (closed this session);
shadow trial 2 surfaced the inequality-grader gap (filed
`mpfr-ts-52u`). Each trial costs ~250K tokens and buys a major
architectural finding that would otherwise hit a 10+-function batch
at full cost. The pattern is reliable enough to bank on.

ADR 0001 has now held under 2 trials × 8 functions at 100% prediction
rate. The integration is production-ready. The blockers between here
and full Production scale-out are well-localised: the grader
inequality mode (Priority 1), the mutate.py delegation carve-out
(Priority 2), and a PHASE.md transition worklog (Priority 4).

Priority 1 (`mpfr-ts-52u`) is the most-leverage next move: ~80-150
LOC + ADR 0002 unblocks ~5-10 substrate/transcendental helpers in
the next mega-batch. Start there.

Good luck.
