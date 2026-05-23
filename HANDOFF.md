# Handoff — Pilot in flight, Step 5 partial

You are picking up the **Pilot phase** of mpfr-ts. Read this top to
bottom before any other action. Then read `CLAUDE.md`. Then check
`bd ready`.

## TL;DR — where to start

```bash
# 1. Pick up state
cat PHASE.md                                            # → Pilot
bd ready                                                # → mpfr-ts-637 (Step 5 followup) + mpfr-ts-odi.5 (the original)
cat docs/PILOT_PLAN.md                                  # the 10-step plan

# 2. Hydrate memory if first time on this device (see docs/memory/README.md)
PROJ_KEY=$(pwd | sed 's|/|-|g')
mkdir -p "$HOME/.claude/projects/${PROJ_KEY}/memory"
cp docs/memory/*.md "$HOME/.claude/projects/${PROJ_KEY}/memory/"

# 3. Continue Step 5 — three files written, runner.ts missing
ls eval/harness/                                        # ast_check.ts, value_codec.ts, worker.ts
# Read each, audit for senior-TS quality, then write runner.ts per bd mpfr-ts-odi.5 brief
```

## Repo state at handoff

| Step | Status | Artifact |
|---|---|---|
| 1. Skeleton + system deps | ✅ done | `package.json`, `bunfig.toml`, `tsconfig.json`, `.gitignore`, dir tree, `mpfr/` cloned |
| 2. Locked schema `src/core.ts` | ✅ done | 456 LOC, fully JSDoc'd, `validate()` enforces every MPFR invariant |
| 3. State DB | ✅ done | `eval/state.db` + `eval/driver/schema.sql`; one row: `mpn_add_n\|substrate\|pending` |
| 4. Golden-driver `common.h` | ✅ done | `eval/golden_master/{common.h, build.sh, _smoke_driver.c}`; smoke driver kept as executable spec |
| 5. Harness skeleton | ⚠️ **partial** | `value_codec.ts` (19KB), `ast_check.ts` (9KB), `worker.ts` (6KB) written; **`runner.ts` MISSING**; acceptance NOT run |
| 6. `mpn_add_n` spec + golden | ⏳ pending | blocked by Step 5 |
| 7. Reference ports | ⏳ pending | blocked by Step 6 |
| 8. Mutation-prove the golden | ⏳ pending | blocked by Step 7 |
| 9. Prompt template + dry-run driver | ⏳ pending | blocked by Step 8 |
| 10. Live sonnet L3 attempt | ⏳ pending | blocked by Step 9 |

## Immediate work — finish Step 5

**Do this first.** The bd issue carrying the full brief is
`mpfr-ts-odi.5`; the followup is `mpfr-ts-637`. View both:

```bash
bd show mpfr-ts-odi.5
bd show mpfr-ts-637
```

The four pieces of Step 5 (per the bd brief):

1. `eval/harness/value_codec.ts` — ✅ written (19KB). **Audit before
   trusting.** Verify: `decodeMpfr` handles the NaN sentinel
   (`prec=0n, sign=1`); `compareOutput` treats NaN-on-NaN as equal;
   signed zero is *not* collapsed; ternary flag is compared exactly.
   See `docs/memory/mpfr_storage_traps.md`.

2. `eval/harness/ast_check.ts` — ✅ written (9KB). **Audit.** Verify:
   Cyrillic homoglyph detection runs (the auto-port-eval scar);
   import-from-core enforcement matches Law 4; substrate-port exemption
   works (mpn_* ports don't import core).

3. `eval/harness/worker.ts` — ✅ written (6KB). **Audit.** Verify:
   Bun Worker semantics (`new Worker(new URL(...), import.meta.url)`);
   the worker imports the port ONCE and processes cases until killed;
   no per-case import cost.

4. `eval/harness/runner.ts` — ❌ **MISSING**. Write per the spec in
   `bd show mpfr-ts-odi.5`. Critical details:
   - CLI: `bun eval/harness/runner.ts --function X --port P --golden G --output O [--class C] [--workers N]`
   - Per-test timeout via `Promise.race(workerResult, setTimeout)`;
     on timeout call `worker.terminate()` and respawn.
   - Aggregate composite per CLAUDE.md formula (correctness, edge,
     time_gate, perf_grade, composite_correctness).
   - `astCheck` runs BEFORE workers are spawned; failure → composite=0 + `schema_violation` field + exit 0.

Then **run the full acceptance suite** from the brief — stub port
(pass), broken port (fail), infinite-loop port (timeouts in <30s,
not >60s), schema violator (composite=0 + schema_violation),
NaN-equality (passes). Only mark `mpfr-ts-odi.5` closed once all
five pass.

## What was decided (and is now binding)

All locked in `~/.claude/projects/.../memory/` AND `docs/memory/`:

- **API shape**: idiomatic immutable `op(...args, prec, rnd) → {value, ternary}`.
- **Substrate**: faithful port of GMP `mpn_*` + MPFR helpers FIRST,
  public fns on top.
- **Failure policy**: sonnet L3 → auto-escalate to opus L3 once → park.
  Pilot is currently **halt-on-failure** (Rule PIL.1).
- **Perf gate**: moderate-to-strict, perf grade SEPARATE from
  correctness, slow-but-correct fns re-attempted later.
- **Library coherence**: locked `src/core.ts` schema; grader AST-rejects
  redeclarations; integration suite gates Production exit. **Law 4.**
- **Runtime**: Bun ≥1.3 for harness; published `src/` portable to Node
  ≥22 (no `Bun.*` or `node:*` imports in `src/`).
- **Quality bar**: "uncompromising senior TS expert" resolves
  ambiguity; spawn a review subagent after every substantial step.

If you find yourself rethinking any of these, write an ADR under
`docs/adr/NNNN-<topic>.md` first; the next agent will need the
trail.

## Hard-won facts about MPFR (don't relearn)

From `docs/memory/mpfr_storage_traps.md`:

1. **`mpfr_get_z_2exp` already de-pads** to a `prec`-bit MSB-aligned
   mantissa. Don't try to shift raw `_mpfr_d` limbs.
2. **TS `exp` = MPFR `EXP(f)` = `mpfr_get_z_2exp`'s returned `exp_2`
   + `prec`.** Off-by-`prec` is the easy bug.
3. **NaN sentinel diverges from C**: TS uses `prec=0n, sign=1`; C
   carries originating precision. Codec and grader must handle this.

## Pickup-on-different-device checklist

1. `git clone git@github.com:<owner>/mpfr-ts.git`
2. Install system deps (Ubuntu): `sudo apt install -y libmpfr-dev libgmp-dev`
3. Install Bun: `curl -fsSL https://bun.sh/install | bash`
4. Clone MPFR upstream: `git clone --depth 1 https://gitlab.inria.fr/mpfr/mpfr.git ./mpfr`
5. Hydrate memory: see `docs/memory/README.md`.
6. `bd bootstrap --yes` (per AGENTS.md / bd-init convention; non-destructive — does NOT use `bd init --force`).
7. `bd ready` to find next work.
8. Read `CLAUDE.md` top to bottom.

The state DB (`eval/state.db`) is checked in (Rule 14) so you inherit
the seeded `mpn_add_n` row and the schema without re-applying.

## Notes / odds & ends

- `AGENTS.md` is a pointer; the source of truth is `CLAUDE.md`.
- The bd-init `.gitignore` template had `*.db` which would have
  silently excluded `eval/state.db`. Fixed in Step 1; see
  `mpfr-ts-f6c` (closed). Don't re-introduce the glob.
- `docs/memory/` is the cross-device memory snapshot, not the live
  memory. Re-snapshot if you modify memory mid-session; see
  `docs/memory/README.md`.
- Step 5 subagent reported it had trouble using `/tmp` from Bun
  inside its sandbox. If you hit the same: use repo-local paths
  (`./tmp/` gitignored, or `eval/reports/`).
- The Step 5 sub-brief lives in `bd show mpfr-ts-odi.5` (preserved
  in the issue description); read it in full, don't shortcut.

## Session-close protocol (when YOU finish)

Per CLAUDE.md §Session close + AGENTS.md mandatory workflow:

```bash
# 1. Refresh memory snapshot if changed
cp ~/.claude/projects/-home-tobiasosborne-Projects-mpfr-ts/memory/*.md docs/memory/

# 2. Export bd to JSONL for cross-device sync
bd export -o .beads/issues.jsonl

# 3. Update worklog if a meaningful chunk closed
$EDITOR docs/worklog/NNN-<topic>.md

# 4. Commit + push
git add -A
git commit -m "<scope>: <summary>"
git push

# 5. Verify
git status                # → up to date with origin/main
bd ready                  # → next work for the next session
```

The `.beads/issues.jsonl` snapshot is the cross-device sync vehicle
for issue state. Without it, a fresh clone has no issue history.

— Good luck. The Pilot's hardest step is in your queue.
