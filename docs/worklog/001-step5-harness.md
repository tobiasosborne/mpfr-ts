# 001 — Pilot Step 5: harness skeleton + acceptance

## Context

Pilot Step 5 was the harness skeleton: `eval/harness/{value_codec, ast_check,
worker, runner}.ts` plus a RED→GREEN acceptance suite that proves the runner's
contract against five scenarios. The Pilot is in halt-on-failure mode, so this
step had to land clean before any function ports begin.

Prior session left three of four harness files written (`value_codec`,
`ast_check`, `worker`) and runner.ts missing. The acceptance suite did not
yet exist.

## What changed

- Audited the three written files (one subagent per file, read-only).
  Surfaced 9 small bugs across 3 files (1 major, 5 minor, 3 quality).
  Each filed as a `bd` issue and linked as a dep of `mpfr-ts-637`.
- Batch-fixed all 9 in one targeted edit pass. tsc clean post-fix.
  Bugs all closed (mpfr-ts-a4a, wl6, 9fk, gbn, 4hp, 8j4, eep, 5gz, zzr).
- Side fix: `tsconfig.json` had `esModuleInterop: false` triggering a TS 7.0
  deprecation warning; flipped to `true` (recommended default).
- Wrote the RED acceptance scaffold under `eval/acceptance/step5/`: 5 stub
  ports (correct / broken / infloop / schema_violator / nan_equality), 5
  hand-generated goldens (deterministic regen via `goldens/_gen.ts`), a
  `spec.json`, and `run.ts` that invokes the (then-missing) runner and
  asserts per-scenario expectations.
- Wrote `eval/harness/runner.ts` (1287 lines, GREEN). Worker pool of N=4
  with init-timeout (2s) and per-case class-tier hard wall
  (substrate/arithmetic=50ms, transcendental=200ms, misc=1s). Pre-flight
  ast_check; composite formula `0.6·corr + 0.2·edge + 0.2·mined`;
  grade.json output with the documented shape.
- All five acceptance scenarios PASS independently.

## Why these choices

- **One audit subagent per file** (not three in parallel): user explicitly
  asked for serial orchestration. Trade-off: slower; benefit: orchestrator
  can react between steps if anything systemic surfaces. Nothing did.
- **Fixes batched into one subagent**: each fix was 1–5 lines, isolated
  to one of three files, and the audit reports were self-contained
  specs. A single subagent dispatch with explicit per-fix instructions
  was more efficient than nine round-trips.
- **RED-then-GREEN TDD**: the acceptance scaffold was written *before*
  runner.ts. Spec was concrete (5 scenarios, exact pass criteria, exact
  `grade.json` shape) and could be cited in the runner prompt verbatim.
- **No third-party deps**: `runner.ts` and the scaffold are pure ESM +
  Bun APIs only, per Rule 12. Maintains zero-runtime-deps for the
  published lib and Bun-native for the harness.

## Frictions surfaced

1. **`runner.ts` weight (1287 LOC)**. Heavier than my "300–500 reasonable"
   guidance. Worth golfing once we have real-port throughput signal, but
   premature now. Filed `mpfr-ts-5a3` (P4).
2. **Composite formula's 0/0 = 1.0 convention**. The infloop scenario
   gets composite=0.2 because mined and edge buckets are vacuously
   perfect (no cases of those tags exist). Documented in the runner;
   real goldens (per Rule 7) will have all 5 tag classes, so this won't
   matter post-acceptance.
3. **Substrate vs arithmetic budget**. The Step 5 brief and CLAUDE.md
   Rule 4 set both at 50ms. Substrate `mpn_*` ops on multi-limb BigInts
   may bench slower under V8/JSC than the C reference; if so, lift the
   substrate budget before scale-out begins. Flagged in `mpfr-ts-5a3`.
4. **Init timeout = 2s**. Picked as a round number — a port with a
   top-level `await import('mpfr/...')` or top-level loop will hit
   this and the runner produces a clean `n_infloop = n_cases` grade
   instead of hanging. Verified working in scenario (c).

## Acceptance

- `bun x tsc --noEmit` → clean (0 errors)
- `bun eval/acceptance/step5/run.ts` → 5/5 PASS
  - (a) correct: composite=1.0, n_pass=10/10
  - (b) broken: composite=0.0, first_error="ternary mismatch: expected 0, got 1"
  - (c) infloop: n_infloop=5/5, wall_ms ≈ 160ms (well under 30s ceiling)
  - (d) schema_violator: schema_violation=true, composite=0, n_cases=0,
        3 schema errors (missing core import + 2 redeclarations)
  - (e) nan_equality: composite=1.0 on 5 NaN-input cases

## Pointers

- `eval/harness/runner.ts` — main grader
- `eval/harness/worker.ts` — per-case isolation worker
- `eval/harness/value_codec.ts` — wire ↔ runtime codec
- `eval/harness/ast_check.ts` — schema-violation gate
- `eval/acceptance/step5/` — acceptance suite, run via `bun ./run.ts`
- `bd show mpfr-ts-odi.5`, `bd show mpfr-ts-637` — issue history
- `bd show mpfr-ts-5a3` — open follow-up (runner golf candidate)

## Next

Pilot Step 6: `mpn_add_n` spec.json + golden_driver.c + golden.jsonl,
≥150 cases across all 5 tag classes per Rule 7.
