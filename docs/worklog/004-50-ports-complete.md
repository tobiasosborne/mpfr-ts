# 004 — 50-port target complete

## Context

User directive: "if everything goes ok, note learnings, and widen the
porting harness, and work through serially more functions. Stop once
you hit about 50 successful ports."

Pilot completed at 10/10 (worklog 003). This shard covers ports 11–50
under the same halt-on-failure ethos (no auto-escalate).

## Result

**50/50 functions ported, all composite=1.0000** against libmpfr-derived
golden masters. State DB:

```sql
SELECT COUNT(*) FROM functions WHERE status='done';
-- 50
```

## Iteration histogram

| iter count | functions | percentage |
|-----------:|----------:|-----------:|
| 1 (one-shot) | 41 | 82% |
| 2          | 6  | 12% |
| 6 (full budget) | 3  | 6%  |

Functions requiring full 6-iteration budget: `mpfr_round`, `mpfr_div`,
`mpfr_sqrt` — all algorithmically dense (single-pass rint algorithm,
bigint quotient with sticky tracking, Newton's-method isqrt with
parity correction).

## Functions ported (in topo-rank order)

**Substrate (mpn_*):** `mpn_add_n`, `mpn_sub_n`, `mpn_cmp`, `mpn_lshift`
(4 — all from Pilot 10).

**Public surface (mpfr_*):** 46 functions across:
- Constructors: `init2`, `set_zero`, `set_nan`, `set_inf`, `set_d`,
  `set_si`, `set_ui`, `set_z`.
- Conversions: `get_d`, `get_si`, `get_ui`, `get_z`.
- Arithmetic: `add`, `sub`, `mul`, `div`, `sqrt`.
- Scale: `mul_2si`, `div_2si`.
- Sign/abs: `neg`, `abs`, `sgn`, `setsign`, `copysign`.
- Min/max: `min`, `max`.
- Integer rounding: `round`, `ceil`, `floor`, `trunc`.
- Comparison (returns int): `cmp`, `cmp_si`, `cmp_ui`, `cmp_d`.
- Predicates (returns boolean): `less_p`, `greater_p`, `lessequal_p`,
  `greaterequal_p`, `equal_p`, `lessgreater_p`, `unordered_p`.
- Kind/sign predicates: `nan_p`, `inf_p`, `zero_p`, `number_p`,
  `signbit`.

## Harness substrate built incrementally

The harness gained capabilities batch-by-batch as ports demanded them:

- `src/internal/mpfr/round_raw.ts` — extracted from add/mul once 4
  callers existed (set_d, get_d, add, mul); now used by ~25 ports.
- `src/internal/mpfr/cmp_raw.ts` — non-throwing `compareMPFR` returning
  `-1|0|1|null` (null for NaN). Used by `mpfr_cmp` (throws on null) and
  all 7 predicates (`null → false`).
- `eval/golden_master/common.h` gained: `jl_output_scalar_int`,
  `jl_output_scalar_i64`, `jl_output_scalar_double`,
  `jl_output_scalar_bool`, `jl_kv_double`, `jl_kv_bool`.
- `eval/harness/value_codec.ts` learned: NaN/+Infinity/-Infinity string
  tokens for doubles; finite-double-shaped strings; `Object.is` for
  number scalar comparison (correct NaN-eq and +0≠-0 semantics);
  boolean output type.
- `eval/harness/runner.ts`: relative `--port` paths resolve against
  `process.cwd()` (was `import.meta.url`).
- `eval/driver/prompts.py`: public ports get absolute-path import
  guidance to avoid the `/tmp/`-resolution footgun.

## Bd issues

29 issues filed across the cycle; 24 closed. Open at scale-out stop:

- `mpfr-ts-5a3` (P4): runner.ts golf candidate (1287 LOC)
- `mpfr-ts-6ps` (P3): state.db perf_grade NOT NULL gap
- `mpfr-ts-upg` (P3): worked-example-eval-leak (self-resolved post-#1)
- `mpfr-ts-wli` (P2): ast_check false-positive on `import { type X }`
  mixed syntax (workaround: split imports)
- One n_throw-vs-mismatch diagnostic note (low priority)

## Why this worked

- **Batching** (3–5 related functions per prep+sonnet dispatch) reduced
  orchestrator overhead by ~3×. Eval validity preserved by giving each
  function its own /tmp/ port path and its own grade.json.
- **Mutation-prove every golden** — every port has a deliberately-broken
  reference variant with gap ≥ 0.45. The goldens caught real bugs
  during iteration (sonnet's first cut on `mpfr_set_d` had a subnormal
  off-by-one; `mpfr_add` failed catastrophic-cancellation; `mpfr_div`
  had quotient-bit-length confusion).
- **Read restrictions** — sonnet could not access the reference port,
  the broken reference, the golden, or the golden_driver. Workd example
  in the prompt is `mpn_add_n`, which differs from the target for #2+.
- **Senior-TS quality bar** as the tiebreaker — uncompromising on
  immutability, no `any`, exhaustive switches, JSDoc with citations.
- **Sonnet-only** (per Pilot policy PIL.4) — no auto-escalate. Every
  port grades against the same harness, same goldens.

## Senior TS quality

50 production ports under `src/`:
- `src/core.ts` — locked schema (unchanged since Step 2).
- `src/internal/mpn/` — 4 substrate ports (add_n, sub_n, cmp, lshift).
- `src/internal/mpfr/` — 2 shared substrate helpers (round_raw,
  cmp_raw).
- `src/ops/` — 44 public ports.

All ports:
- Pass `bun x tsc --noEmit` cleanly (strict tsconfig with
  `noUncheckedIndexedAccess`, `exactOptionalPropertyTypes`, etc.).
- Import only from `src/core.ts` (public ports), `../internal/*` (when
  substrate composition is needed), or nothing (substrate ports).
- Use `readonly` on all inputs; never mutate.
- Use `unknown` + narrowing; no `any`.
- JSDoc citations to specific `mpfr/src/*.c` line ranges or GMP manual
  sections.

## Acceptance

- `bun x tsc --noEmit` → clean.
- `bun eval/acceptance/step5/run.ts` → 5/5 PASS.
- All 50 ports re-grade composite=1.0000 against their goldens (verified
  at every batch boundary; final 50-port regression clean).

## Pointers

- `eval/state.db` — full record (50 runs row, 50 functions row).
- `docs/worklog/` — 003 (Pilot complete), 002 (mpn_add_n vertical),
  001 (Step 5 harness), this shard (004).
- `eval/driver/{prompts.py, ralph.py}` — prompt template + dry-run.
- `src/ops/`, `src/internal/` — the port code.
- `eval/functions/<fn>/` — spec + driver per function (golden.jsonl
  gitignored, regenerable per MPFR_VERSION).

## Next (not in this session)

- Hit the open bd issues: import-syntax false positive, runner.ts golf,
  perf_grade schema.
- Build callgraph.py for true topo-rank-driven ralph loop.
- Stand up Production phase (auto-escalate enabled) for ports 51+.
- Cross-function integration suite under `eval/integration/` (Law 4
  Production exit criterion).
