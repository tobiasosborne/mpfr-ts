# 003 — Pilot complete (10/10 at composite=1.0)

## Context

After Step 5 (worklog 001) and Step 6–10 for `mpn_add_n` (worklog 002),
the user directed scale-out to ~50 functions, substrate-weighted with
topo-rank sorting and a quota of ≥3 mpfr_* publics per batch of 10. The
Pilot ethos (halt-on-failure, sonnet-only, no auto-escalate) is
preserved through the scale-out by default.

## What changed

Functions 2–10 of the Pilot completed serially:

| # | function       | class       | cases | sonnet iter | composite | gap   |
|---|----------------|-------------|-------|-------------|-----------|-------|
| 2 | mpn_sub_n      | substrate   | 152   | 1           | 1.0000    | 0.574 |
| 3 | mpn_cmp        | substrate   | 215   | 1           | 1.0000    | 0.411 |
| 4 | mpn_lshift     | substrate   | 165   | 1           | 1.0000    | 0.719 |
| 5 | mpfr_init2     | misc        | 156   | 2           | 1.0000    | 0.789 |
| 6 | mpfr_set_d     | misc        | 204   | 2           | 1.0000    | 0.982 |
| 7 | mpfr_get_d     | misc        | 167   | 1           | 1.0000    | 0.975 |
| 8 | mpfr_cmp       | arithmetic  | 374   | 1           | 1.0000    | 0.505 |
| 9 | mpfr_add       | arithmetic  | 212   | 2           | 1.0000    | 0.914 |
| 10| mpfr_mul       | arithmetic  | 210   | 2           | 1.0000    | 0.892 |

(Function #1 mpn_add_n was logged in worklog 002.)

**Total**: 10/10 Pilot functions, composite=1.0 each, 4 of 10
required ≥1 sonnet iteration to fix a genuine bug caught by the
golden (init2's relative-path import, set_d's subnormal off-by-one,
add's catastrophic-cancellation bit-length, mul's relative-path).

## Harness widening (during scale-out)

- `eval/golden_master/common.h` gained: `jl_output_scalar_int`,
  `jl_kv_double`, `jl_output_scalar_double`.
- `eval/harness/value_codec.ts`: `decodeInputValue` and
  `decodeExpectedOutput` now recognise NaN/+Infinity/-Infinity string
  tokens and finite double-shaped strings; `compareOutput`'s
  number-scalar branch uses `Object.is` (correct NaN-eq and
  +0≠-0 semantics).
- `eval/harness/runner.ts`: relative `--port` paths now resolve against
  `process.cwd()`, not `import.meta.url`.
- `eval/driver/prompts.py`: public-port prompts now embed
  absolute-path import guidance (avoids the `/tmp/`-resolution footgun
  for `../core.ts`). Sonnet still missed it twice (init2, mul) but
  recovered iter 2.
- 17 bd issues filed across the cycle; 12 already closed
  (9 audit fixes + 3 follow-up fixes + 1 path-resolution).

## Why these choices

- **Mutation-proving every golden** confirmed each port's golden is a
  real discriminator. Gaps ranged from 0.41 (mpn_cmp — LSB-first
  coincidentally agrees on many cases; deliberately documented) to
  0.98 (mpfr_set_d — always-zero-stub fails most cases).
- **Read restrictions on sonnet** (banned reading the reference
  port, broken port, and golden) keep the eval honest. mpn_add_n
  was acknowledged as a worked-example leak (worked example IS the
  target); for #2 onward the worked example differs from the
  target.
- **State DB updated per function**: `runs` row + `functions.status =
  done` + `best_correctness = 1.0`. `perf_grade` and `usd_est`
  set to 0.0 today (`mpfr-ts-6ps` tracks the schema-relaxation gap).
- **Mixed substrate + public** matches the user-confirmed approach (a):
  4 mpn substrate + 6 mpfr public covers both surfaces' edge cases
  early.

## Frictions surfaced (still open)

- `bd mpfr-ts-wli` (P2): ast_check flags `import { type MPFR }` mixed
  syntax. Workaround in place (separate `import type` + `import`
  lines). Should fix before the next batch of public ports.
- `bd mpfr-ts-???` (n_throw conflates exceptions with output
  mismatches): diagnostic-quality, not correctness.
- `bd mpfr-ts-upg` (worked-example-leak for function #1): self-resolves
  for #2 onward.
- `bd mpfr-ts-5a3` (runner.ts 1287 LOC): defer until throughput signal
  changes the picture.
- `bd mpfr-ts-6ps` (state.db perf_grade NOT NULL): schema gap.
- **Round/sticky/ternary helper now duplicated 4×** (set_d, get_d,
  add, mul). Threshold for extraction crossed; defer to start of next
  batch.

## Phase status

Per PIL.5, transition to Production requires "10/10 pilot functions
with composite ≥ 0.95 in a single clean ralph-loop run (no human
intervention mid-run)". The 10/10 are done; the "single clean run"
qualifier is interpreted loosely — orchestration involved filing
bd issues and small harness fixes between functions, but no port
was rewritten by the orchestrator and no golden was loosened to
make a port pass.

Per the user's directive to scale to 50 ports under halt-on-failure
(NOT auto-escalate), PHASE.md remains `Pilot` for now. Production
phase is reserved for the moment auto-escalate is enabled.

## Acceptance

- `bun x tsc --noEmit` → clean
- `bun eval/acceptance/step5/run.ts` → 5/5 PASS
- `sqlite3 eval/state.db "SELECT name, status, best_correctness FROM functions ORDER BY topo_rank"`:
  ```
  mpn_add_n  | done | 1.0
  mpn_sub_n  | done | 1.0
  mpn_cmp    | done | 1.0
  mpn_lshift | done | 1.0
  mpfr_init2 | done | 1.0
  mpfr_set_d | done | 1.0
  mpfr_get_d | done | 1.0
  mpfr_cmp   | done | 1.0
  mpfr_add   | done | 1.0
  mpfr_mul   | done | 1.0
  ```

## Next

Functions 11–20 (next batch of 10). Picking from easy publics
(neg, abs, sgn, set_nan/inf/zero, sub) plus a substrate or two
(mpn_mul_1, perhaps). Extract `round_raw` into substrate before
the next port that needs it (sub will compose add's
machinery and shares the rounding helper).
