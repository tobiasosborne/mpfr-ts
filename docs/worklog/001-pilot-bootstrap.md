# Worklog 001 — Pilot bootstrap (Steps 1–4 + Step 5 partial)

**Date:** 2026-05-23
**Session:** initial bootstrap
**Closed bd issues:** mpfr-ts-odi.1, mpfr-ts-odi.2, mpfr-ts-odi.3, mpfr-ts-odi.4, mpfr-ts-f6c
**In-progress at session close:** mpfr-ts-odi.5 (partial), mpfr-ts-637 (followup)

## Context

This is the first work session on mpfr-ts. The project is a warmup
to the larger goal of auto-porting C libraries (FLINT next, then
bigger) to pure TypeScript using a sonnet-L3 ralph loop driven
against a brutal golden-master harness. Lessons inherited from
`../auto-port-eval` (90-run FLINT eval, Pareto data, hard-won
HANDOFF.md traps).

Goal of the session: stand up the scaffolding (CLAUDE.md, memory,
beads, plan), then execute the Pilot's 10-step vertical slice
serially via subagents until `mpn_add_n` is graded end-to-end by
sonnet L3.

## What changed

1. **Scaffolding** — wrote `CLAUDE.md` (Laws + Rules + phase-aware
   workflow, synthesised from `../scientist-workbench` and
   `../BennettVM.jl`), `PHASE.md`, `docs/PILOT_PLAN.md`, `AGENTS.md`
   (pointer to CLAUDE.md). Set up persistent memory under
   `~/.claude/projects/.../memory/` with 8 entries indexed in
   `MEMORY.md`. Snapshotted to `docs/memory/` for cross-device pickup.

2. **Beads** — initialized (`bd init`), created epic `mpfr-ts-odi` +
   10 step issues with `bd dep add` chain so `bd ready` only shows
   the next step. Used `bd` as persistent cross-session tracker;
   `TaskCreate` as in-session view. Resolved AGENTS.md / CLAUDE.md
   conflict (bd-init template forbids TaskCreate + MEMORY.md; project
   uses both per the scoping in Rule 9).

3. **Step 1 (closed)** — repo skeleton: `package.json` (engines
   bun≥1.3, node≥22; zero deps), `bunfig.toml`, `tsconfig.json`
   (strict + noUncheckedIndexedAccess + exactOptionalPropertyTypes
   + verbatimModuleSyntax + isolatedModules + every other catch
   flag), `.gitignore` (with explicit `eval/state.db` NOT ignored;
   bd-init's `*.db` glob caught and replaced), full dir tree, MPFR
   cloned to `./mpfr/` (15MB). Subagent caught the `*.db` issue —
   filed as `mpfr-ts-f6c` (closed).

4. **Step 2 (closed)** — `src/core.ts`: the **locked schema**.
   `MPFR` interface (immutable, 4-kind discriminant, MSB-aligned
   mantissa), `RoundingMode` string union (5 modes, no RNDNA, no
   RNDF), `Ternary`, `Result`, `MPFRError` (with `code`
   discriminant), `validate()` enforcing every invariant including
   MSB-set and `mant < 2^prec`, helper constructors (`posInf`,
   `negInf`, `posZero`, `negZero`, `NAN_VALUE`), type guards
   (`isFinite`, `isNaN`, `isInf`, `isZero`, `isNormal`). Multi-paragraph
   value-model docstring at top citing `mpfr/src/mpfr.h`. No `any`,
   no `as` casts (one `satisfies` for `NAN_VALUE`'s `Object.freeze`).
   ~456 LOC.

5. **Step 3 (closed)** — `eval/driver/schema.sql` (197 LOC) +
   `eval/state.db` (created, seeded with `mpn_add_n` row).
   Four-table schema (`functions`, `runs`, `cases`, `meta`) with
   CHECK constraints on every enum, FK with CASCADE, indexes for the
   queries in CLAUDE.md §Common queries, schema version stamp in
   `meta`, WAL journal mode. Best-run circular FK resolved by leaving
   it nullable + no constraint (the more useful `runs.fn_name →
   functions.name` FK catches the bugs that matter; deferred-FK
   approach would require every consumer to set `defer_foreign_keys`).

6. **Step 4 (closed)** — `eval/golden_master/common.h` (~290 LOC),
   `build.sh`, `_smoke_driver.c` (kept as executable spec). MPFR-aware
   helpers built on top of the auto-port-eval pattern: named-key
   `inputs` object (vs FLINT's positional array), `jl_kv_limbs` for
   GMP arrays, `jl_kv_mpfr` for MPFR values matching `src/core.ts`,
   `jl_output_result` for the canonical `{value, ternary}` shape,
   rounding-mode-as-string. Subagent surfaced three load-bearing
   facts about MPFR's storage:
   - `mpfr_get_z_2exp` already de-pads (no manual shift needed).
   - `exp_ts = exp_2 + prec = MPFR_GET_EXP(f)`.
   - NaN sentinel TS-side is `prec=0n, sign=1`; diverges from C.
   Saved as `mpfr_storage_traps.md` memory entry — load-bearing for
   Steps 5, 6, and 10.

7. **Step 5 (in-progress)** — delegated, subagent stopped mid-stream
   by session end. Files written: `value_codec.ts` (19KB),
   `ast_check.ts` (9KB), `worker.ts` (6KB). Files NOT written:
   `runner.ts`. Acceptance tests NOT executed. State preserved in bd
   `mpfr-ts-odi.5` (notes) + followup `mpfr-ts-637`. See HANDOFF.md.

## Why these choices

- **Idiomatic immutable API over faithful C contract** — user
  explicit, captured in `decision_api_shape.md`. The cost is
  per-function harness adapters; the benefit is a usable library.
- **Faithful substrate before idiomatic public fns** — every
  divergence from libmpfr output is debuggable line-by-line.
- **Bun over Node for harness** — user's stack; faster TS execution,
  built-in test runner, `Bun.spawn`. Guardrail: `src/` stays
  portable (no `Bun.*` or `node:*` imports) so Node consumers can
  install the published library.
- **SQLite as state DB** — file-based, queryable, checked in
  (Rule 14). Queries in CLAUDE.md §Common queries are the API.
- **Per-test worker isolation from day one** — the single largest
  scar inherited from auto-port-eval. Without it, transcendental
  ports with infinite loops kill the whole grade run.
- **Halt-on-failure during Pilot** — throughput is irrelevant when
  the harness isn't proven. Auto-escalate (sonnet→opus) only kicks
  in for Production.
- **Locked `src/core.ts` schema + grader AST enforcement (Law 4)** —
  the end-state must be a *usable* library, not 600 isolated functions
  with incompatible types.

## Frictions surfaced

- **`<env>` reported `Is a git repository: false`** — but `.git/`
  existed. Subagent caught it. Trust filesystem over env metadata.
- **bd-init `.gitignore` template had `*.db`** — would have silently
  excluded the load-bearing `eval/state.db`. Filed `mpfr-ts-f6c`
  for the historical record.
- **bd-init template forbids `TaskCreate` and `MEMORY.md`** —
  conflicts with the scientist-workbench convention this project
  inherits. Resolved by rewriting `AGENTS.md` as a pointer to
  `CLAUDE.md` and scoping `bd` to cross-session, `TaskCreate` to
  in-session.
- **Subagent worktree isolation requires git** — first delegation
  failed because `claude` subagent triggers a worktree hook that
  needs a git repo. Switched to `general-purpose` subagent (no
  hook); all subsequent delegations worked.
- **`/tmp` sandboxing on Bun** — Step 4 subagent reported Bun
  couldn't read its own `/tmp` files from inside the sandbox. Used
  repo-local paths; documented in HANDOFF.md.
- **`sudo` blocks the harness** — `libmpfr-dev` install required
  prompting the user via `! sudo apt install -y libmpfr-dev`.
  Documented as session zero step.

## Acceptance

- `bun --version` 1.3.9, `gcc --version` 13.3, `sqlite3` 3.45,
  `pkg-config --cflags --libs mpfr` → `-lmpfr -lgmp`, `ls mpfr/src/add.c`
  ✓.
- `src/core.ts` typechecks under strict + all flags; `validate()`
  rejects bad precision / mis-aligned mantissa / wrong sentinel for
  NaN; smoke test passes.
- `eval/state.db` has 4 tables, 1 seeded row, schema_version=1, FK
  + CHECK enforcement live.
- `eval/golden_master/common.h` compiles with `-Werror`; smoke
  driver emits 3 valid lines; every MPFR-shaped value passes
  `validate()` from `src/core.ts`.
- 5/10 pilot steps in some state; 4 closed + 1 in-progress.

## Pointers

- Plan: `docs/PILOT_PLAN.md`
- Decisions: `docs/memory/decision_*.md`
- MPFR storage traps: `docs/memory/mpfr_storage_traps.md`
- Pickup: `HANDOFF.md`
- Predecessor: `/home/tobiasosborne/Projects/auto-port-eval/HANDOFF.md`

## Next

`bd ready` will show `mpfr-ts-637` (Step 5 followup) and
`mpfr-ts-odi.5` (the original). The next agent finishes Step 5
(audit 3 files, write `runner.ts`, run full acceptance suite),
then proceeds serially through Steps 6–10 per the plan.
