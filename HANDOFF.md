# Handoff — 115 ports landed; next: decide PHASE + push past 30 in a batch

You are picking up mpfr-ts after a session that took port count from 85
to **115** (state.db `done=115`, 4 cleanly blocked, all composite=1.0
against libmpfr-derived goldens). The 30-function mega-batch contract
set in worklog 006 is complete; `docs/worklog/007-30-fn-mega.md`
documents the run and the empirical findings.

The previous session **crashed during session close** after the work
was pushed. This HANDOFF + worklog 007 close that paperwork gap; no
code or DB state was lost.

## ⚠ Three gotchas — read first

1. **`.gitignore` `mpfr/` pattern.** Original `.gitignore` line 2 was
   `mpfr/` (no leading slash), which git matches at *any* depth —
   including `src/internal/mpfr/`. Five substrate files were silently
   dropped from commits for ~12 hours. **Fixed in commit `cb65ebe`**
   by anchoring to `/mpfr/`. Audit any new directory names that
   collide with ignored patterns.

2. **`bd` commands don't auto-export to JSONL.** `bd create/close/
   remember` write to local Dolt only. `.beads/issues.jsonl` (the
   git-tracked source of truth) refreshes only via `bd export -o
   .beads/issues.jsonl`. `ralph.py --commit-batch` and `--ship` do
   this automatically; manual `git commit` skips it. **Always run
   `bd export -o .beads/issues.jsonl` before manual git commits**,
   or prefer `--commit-batch`/`--ship`. `mpfr-ts-i8e` tracks the
   pre-commit-hook fix.

3. **`PHASE.md` still says `Pilot`** despite 115 ports landed. The
   pilot was the *first 10* per CLAUDE.md. The transition has been
   deferred across multiple sessions because Rule 14 requires a
   worklog phase-transition doc first. This is the most important
   decision in front of the next session — see §"First decision".

## TL;DR — first 10 minutes

```bash
# Pick up state
git pull --rebase
cat PHASE.md                                       # → Pilot (still)
cat HANDOFF.md                                     # this file
cat docs/worklog/007-30-fn-mega.md                 # the last run
cat docs/worklog/006-scale-out-engine.md           # engine spec
bun x tsc --noEmit                                 # must be clean
bun eval/acceptance/step5/run.ts                   # 5/5 expected
/home/tobias/.local/bin/pytest eval/driver/tests/  # 52/52 expected
sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|4, done|115

bd ready                                           # 8 P2-P4 issues
```

## First decision: PHASE.md

Two viable paths. Pick one before doing anything else.

**Path A — formalize Production.** Write `docs/worklog/008-phase-
transition.md` describing what 115 ports of Pilot proved (the engine
scales, sonnet L3 is the right porter, golden mutation-proving
catches regressions, the cost/throughput envelope) and what
auto-escalate caveats remain. Flip `PHASE.md` to `Production`. Enable
sonnet → opus L3 escalation in the ralph loop. ~45 min. Unlocks
unattended runs and the parked.md flow for the remaining ~410
functions.

**Path B — keep Pilot rules indefinitely.** The orchestrator has been
running a "Pilot+" workflow (halt-on-failure, no opus escalation)
successfully at 115 ports. If the user prefers continued human-in-
the-loop oversight over throughput, document that in worklog 008
and proceed. ~15 min.

**Recommendation**: Path A. The engine is proven; the cost of running
Pilot rules indefinitely is throughput, and the unattended-run mode
is what the project was designed for.

## Second decision: next batch size

Worklog 007 found that **opus prep cost is sublinear in batch size**
— 30 functions cost ~380K tokens, essentially the same as 15
functions at ~387K. The previous "linear extrapolation" was wrong;
the fixed-cost overhead (CLAUDE.md re-read, worked-example load)
dominates. Implication: the empirical ceiling is much higher than
30. The next batch should test it.

**Suggested target: 50–60 functions in a single opus prep**, dispatched
as 6–8 sonnet waves of 8–10 each. Cost envelope: ~450K opus tokens
+ ~6–8 wall hours of sonnet wave processing. If opus context
exhausts mid-prep, *that* is the discovery; capture it.

Coherent batch options (most user-facing surface first):

| Theme | Count available | Coherence | Difficulty |
|---|---:|---|---|
| Conversion family (set_q, set_str, get_str, set_si_2exp, get_si_2exp, set_d_2exp, set_f, get_f, ...) | ~30 | High | Mixed — `mpfr_get_str` is hardest |
| Modular / remainder (fmod, fmod_ui, modf, remainder, remquo, fmodquo, ...) | ~10 | High | Medium |
| Division variants (div_si, div_ui, si_div, ui_div, plus div1sp[2,2n,3]) | ~10 | High | Medium |
| Transcendentals (exp, log, trig, hyperbolic) | ~75 | High | **High** — first prec-extension-loop ports |

**Recommendation**: 50 conversion + modular + division variants. Save
transcendentals for a dedicated batch with a prec-extension-loop
prompt template; they're the first ports that genuinely exercise
MPFR's "compute at extended precision, round, check, retry"
algorithm.

## What works (don't change)

| component | path | notes |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen; never modify without ADR |
| Worker isolation | `eval/harness/worker.ts` | Solid |
| Grader | `eval/harness/runner.ts` | Solid |
| Codec | `eval/harness/value_codec.ts` | `compareField` now uses `Object.is` for object-shaped number fields (007 fix). Known gap: `mpfr-ts-2ls` (scalar strings) |
| AST gate | `eval/harness/ast_check.ts` | Import-strip landed pre-007 |
| Substrate | `src/internal/{mpn,mpfr}/` | 18 files (14 mpfr + 4 mpn) |
| Callgraph | `eval/driver/callgraph.py` | 525 fns; re-run if you touch mpfr/src/ |
| Driver | `eval/driver/ralph.py` | 5 modes. `_promote_port` now rewrites absolute imports inline (007). `--grade` substrate fallback now works (007) |
| State DB | `eval/state.db` | 119 rows; 115 done, 4 blocked, 146 runs |

## Engine still has rough edges (not blocking, worth fixing)

- **Post-batch tsc cleanup**. Sonnet ports emit unused locals/imports
  at ~1 issue per 5 ports under `noUnusedLocals/Parameters`. Last
  session ran a manual cleanup pass (commit `394ef02`). Either tighten
  the agent prompt to strip unused emit, or add a `tsc --noEmit` gate
  to `--ship` that fails on warnings. Not yet filed as bd.
- **Substrate ref_port paths**. Opus templates assume `src/ops/` for
  ref_port stubs even when the port is substrate-class. Last batch
  needed 5 ref_port path corrections post-promote. The opus template
  should consult state.db `class`. Not yet filed.
- **`mpfr-ts-i8e` git pre-commit hook**. Highest-recurrence-rate bd
  (every manual commit risks the dolt-sync gap). ~15 min to land.

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR.
- Flip `PHASE.md` from `Pilot` to `Production` without writing
  `docs/worklog/008-phase-transition.md` first (CLAUDE.md Rule 14).
- Disable harness gates to make a port pass. Fix the port instead.
- Skip mutation-prove (broken < 0.55, ideally < 0.45 per worklog 006
  learning #6 — multi-bug perturbations land < 0.30 reliably).
- Re-introduce the absolute-path import bug — `_promote_port` handles
  it; don't regress that code path.
- Dispatch all N sonnets simultaneously when N > 10. Waves of 6–10
  remain the cost-disciplined default; deviating wastes tokens on
  parallel failures.

## Pickup-on-different-device checklist

1. `git clone git@github.com:tobiasosborne/mpfr-ts.git`
2. `sudo apt install -y libmpfr-dev libgmp-dev sqlite3`
3. `curl -fsSL https://bun.sh/install | bash`
4. `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. `pip install pytest`
6. `bd bootstrap --yes && bd hooks install && bd import`
7. Smoke-check:
   `bun x tsc --noEmit && bun eval/acceptance/step5/run.ts && /home/tobias/.local/bin/pytest eval/driver/tests/`
8. Read CLAUDE.md → this file → `docs/worklog/007-30-fn-mega.md` →
   `006-scale-out-engine.md`.

## Open bd issues at session close (8 total, all P2–P4)

P2 — engine improvement (highest leverage):
- `mpfr-ts-i8e`: git pre-commit hook to run `bd export -o
  .beads/issues.jsonl` automatically.

P3 — harness / port-quality polish:
- `mpfr-ts-ai4`: runner `n_throw` conflates exceptions with mismatches.
- `mpfr-ts-e4j`: `expected_throw` codec for domain-error goldens
  (unblocks `mpfr_abort_prec_max`).
- `mpfr-ts-lq8`: `eval/golden_master/run_all.sh` wrapper.
- `mpfr-ts-sr4`: enforce Rule 7 tag minimums at grade time.
- `mpfr-ts-2ls`: `value_codec` scalar-string outputs.
- `mpfr-ts-d6o`: `callgraph.py` misses mpn_* substrate fns (GMP-sourced).

P4 — cosmetic:
- `mpfr-ts-c6b`: state.db topo_rank vs callgraph topo_rank drift.

`bd ready` and `bd list --status=open` for the live picture.

## Resolved this session (don't re-file)

- `mpfr-ts-NEW1` — `_promote_port` inline path rewrite. **Landed.**
- `mpfr-ts-2r8` — `--grade` substrate-class fallback. **Landed.**

## One final thing

The 85→115 push proved the engine scales: opus prep cost is sublinear,
sonnet waves of 6–10 are robust, and the `_promote_port`/`--grade`
fixes mean `--ship` is now genuinely end-to-end. The next milestone is
deciding whether to formalize Production rules and pushing batch size
past 30. Cost ceiling on opus prep is much higher than worklog 006
estimated — exploit it.

Good luck.
