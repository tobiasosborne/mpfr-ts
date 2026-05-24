# Handoff — 85 ports landed; next: 30-function mega-batch

You are picking up mpfr-ts after a session that took port count from 50 to
**85** (state.db `done=85`, 4 cleanly blocked, all composite=1.0 against
libmpfr-derived goldens). The scale-out engine described in
`docs/worklog/005-scale-out-handoff.md` is built and validated;
`docs/worklog/006-scale-out-engine.md` documents the run and the
empirical limits.

**Your job: a single 30-function mega-batch, cost-disciplined**. Details
below; the live contract is `docs/worklog/006-scale-out-engine.md`
§"Addendum (end-of-session user directive)".

## TL;DR — first 10 minutes

```bash
# 1. Pick up state
cat PHASE.md                                       # → Pilot
cat HANDOFF.md                                     # this file
cat docs/worklog/006-scale-out-engine.md           # the live contract (read it ALL)
bun x tsc --noEmit                                  # must be clean
bun eval/acceptance/step5/run.ts                    # 5/5 expected
/home/tobias/.local/bin/pytest eval/driver/tests/   # 52/52 expected
sqlite3 eval/state.db "SELECT status, COUNT(*) FROM functions GROUP BY status"
# Expected: blocked|4, done|85

# 2. Read CLAUDE.md (re-read every session), then 006 cover-to-cover,
#    then 005 for the engine spec, then 004 for the 50-port arc context.

# 3. Read this file's §"Your contract" below.
```

## Your contract: 30-function mega-batch with cost-disciplined waves

The previous session's mega-batch was 15 functions, all sonnet L3
one-shot composite=1.0. The user has set the next target empirically:
**push to 30 in one prep, see where the architecture breaks.**

### Strategy

**One opus prep dispatch covering 30 functions**.

Cost envelope: ~700K opus tokens / ~75-90 min wall (linear extrapolation
from the 15-fn batch at ~387K / ~45 min). If opus hits context
exhaustion mid-prep, capture that signal — it informs whether the
next architecture step is "multiple parallel preps via SDK-direct".

**Sonnet dispatches in WAVES of 5-10 in parallel, not all 30 at once.**

Cost discipline framing (the user's directive):
> The goal isn't so much optimising time as cost atm. Once the prep is
> done I would spawn sonnets up to about 5-10 in parallel waves. Then
> commit and push.

Why: spawning all 30 sonnets simultaneously risks 2-3 burning tokens
with no port produced (each failed sonnet is ~50K wasted tokens).
Waves of 5-10 let each wave's signal inform the next dispatch and
preserve cost when a prompt issue surfaces.

### Wave sequencing (3-6 waves total)

```
Wave 1: dispatch 5-10 parallel sonnets  → await all → spot-check + --grade subset
Wave 2: dispatch next 5-10              → await all → spot-check + --grade subset
...
Wave N: final batch
Then:    --ship --message "..." <30 function names>
         (or manual --grade + commit-batch if --ship's path-rewrite isn't landed)
```

If a wave shows a problem (e.g. one sonnet hits 4 iterations),
diagnose before launching the next wave. Cheap waves of recovery cost
~30K tokens; an unbroken 30-wide dispatch with 3 failures costs ~150K.

### Picking the 30 (orchestrator decides)

Bias toward coherent themes — the opus prep gets economies of scale
when the 30 share structural subtleties. Three viable groupings:

| Theme | Functions (sample) | Coherence | Difficulty |
|---|---|---|---|
| Conversion family | mpfr_set_q, set_str, get_str, set_si_2exp, set_ui_2exp, get_si_2exp, set_d_2exp variants, set_f, get_f, ... | High | Mixed — mpfr_get_str is hardest |
| Modular/remainder | mpfr_fmod, fmod_ui, modf, remainder, remquo, fmodquo, ... | High | Medium |
| Division variants | mpfr_div_si, div_ui, si_div, ui_div, plus div1sp[1n,2,2n,3] | High | Medium |
| Sqr family | mpfr_sqr_1, sqr_1n, sqr_2 (existing batch1 unblocks), plus sqrt approximations | Medium | Medium |
| Mixed accessors + arithmetic | leftover get/set + add/sub/mul/div variants | Lower | Mixed |

**Recommendation**: 30 conversion-family functions. Highest coherence,
highest opus-prep amortization, fills out a large user-facing surface
area at once.

### What "done" looks like

- 30 ports at composite=1.0 in state.db (or N<30 done + clean
  deferrals for the rest, with a bd issue per deferral).
- TSC clean, acceptance 5/5, ralph tests still 52/52 (or more if engine
  extended).
- Commit + push.
- `docs/worklog/007-30-fn-mega.md` with throughput data, empirical
  ceiling findings, any new bd issues.

## What works (don't change)

| component | path | notes |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen; never modify without ADR |
| Worker isolation | `eval/harness/worker.ts` | Solid |
| Grader | `eval/harness/runner.ts` | Solid |
| Codec | `eval/harness/value_codec.ts` | Known gap: `mpfr-ts-i5z` (scalar strings) |
| AST gate | `eval/harness/ast_check.ts` | Import-strip landed last session |
| Substrate | `src/internal/{mpn,mpfr}/` | 9 files |
| Callgraph | `eval/driver/callgraph.py` | 525 fns; re-run if you touch mpfr/src/ |
| Driver | `eval/driver/ralph.py` | 5 modes; +800 LOC last session |
| State DB | `eval/state.db` | 89 rows; 85 done, 4 blocked |

## High-leverage prep work BEFORE dispatching the mega-batch

The first 30-60 min should be engine cleanup so the wave-based dispatch
runs smoothly. Three P2 bds in order:

1. **`mpfr-ts-NEW1` — make `--ship` rewrite absolute paths inline.**
   Sonnet writes `/tmp/eval_<fn>/port.ts` with absolute imports
   (`/home/tobias/Projects/mpfr-ts/src/core.ts`). On promote to
   `src/ops/<short>.ts` or `src/internal/...`, the orchestrator must
   rewrite these to relative paths. Last session did this manually via
   a python script (worked cleanly; see worklog 006 §"Tooling fixes in
   this batch"). Move that into `_promote_port` so a single `--ship`
   command works end-to-end. Cost: ~30 min with TDD.

2. **`mpfr-ts-2r8` — `--grade` resolve_port_path fallback for
   substrate.** When `/tmp/eval_<fn>/port.ts` is absent, `--grade`
   falls back to `src/ops/<short>.ts` only. Substrate ports
   (`src/internal/mpfr/<short>.ts` and `src/internal/mpn/<short>.ts`)
   are missed. Read state.db's `class` column to pick the canonical
   path. Cost: ~15 min.

3. **(Optional) `mpfr-ts-akk` — `--next` pending-deps policy.** Less
   urgent; only matters when the orchestrator wants to hand it more
   freedom to seed batches automatically. Cost: ~30 min if pursued.

After 1+2 (~45 min), the mega-batch's commit step is a single
`--ship --message "..." <30 fn names>` call.

## What the next agent must NOT do

- Modify `src/core.ts` without an ADR.
- Flip `PHASE.md` from `Pilot` to `Production` without writing
  `docs/worklog/NNN-phase-transition.md` first (CLAUDE.md Rule 14).
- Disable harness gates to make a port pass. Fix the port instead.
- Skip mutation-prove (broken < 0.55, ideally < 0.45 per worklog 006
  learning #6 — multi-bug perturbations land cleanly < 0.30).
- Dispatch all 30 sonnets simultaneously. The user explicitly wants
  **waves of 5-10**.
- Re-introduce the absolute-path import bug after `mpfr-ts-NEW1`
  lands.

## Pickup-on-different-device checklist

1. `git clone git@github.com:tobiasosborne/mpfr-ts.git`
2. `sudo apt install -y libmpfr-dev libgmp-dev sqlite3`
3. `curl -fsSL https://bun.sh/install | bash`
4. `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. `pip install pytest` (or use whatever Python env)
6. `bd bootstrap --yes && bd hooks install && bd import`
7. Smoke-check: `bun x tsc --noEmit && bun eval/acceptance/step5/run.ts && /home/tobias/.local/bin/pytest eval/driver/tests/`
8. Read CLAUDE.md → this file → `docs/worklog/006-scale-out-engine.md`.

## Open bd issues at session close (prioritized for the next session)

P2 — engine improvements (do these first):
- `mpfr-ts-NEW1` (or hunt by title): --ship inline path rewrite
- `mpfr-ts-2r8`: --grade substrate fallback
- `mpfr-ts-akk`: --next pending-deps policy
- `mpfr-ts-710`: --next public-API priority

P3 — harness improvements (worth scheduling but not blocking):
- `mpfr-ts-79u`: expected_throw codec (unblocks abort_prec_max +
  domain-error variants)
- `mpfr-ts-0t3`: eval/golden_master/run_all.sh wrapper
- `mpfr-ts-7em`: enforce Rule 7 tag minimums at grade time
- `mpfr-ts-i5z`: value_codec scalar-string outputs
- `mpfr-ts-9li`: mpn_* substrate fns not in callgraph
- `mpfr-ts-0dw`: state.db topo_rank vs callgraph

P3 — port quality:
- `mpfr-ts-3ka`: runner n_throw conflation
- `mpfr-ts-6ps`: state.db perf_grade NOT NULL but unused
- `mpfr-ts-upg`: worked-example-eval-leak for #1

P4 — cosmetic / not-blocking-anything:
- `mpfr-ts-5a3`, `mpfr-ts-0pq`, `mpfr-ts-6s9`, `mpfr-ts-ggv`,
  `mpfr-ts-NEW-marginal-gap`

`bd ready` and `bd list --status=open` for the full live picture.

## One final thing

The 50→85 push proved the engine works. The 85→115 push (this next
session) proves the engine **scales** — specifically that cost-
disciplined waves of sonnet ports off one big opus prep is the right
shape for the rest of Pilot. If 30 works cleanly, the recipe holds for
50+ in subsequent sessions; if 30 hits a ceiling (context exhaustion in
prep, parallel-dispatch failures in sonnet waves), the next step is
SDK-direct multi-prep parallelism (worklog 005 §7).

Good luck.
