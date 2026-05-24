# 005 — Handoff: build the scale-out engine

> You are picking up mpfr-ts after a session that took the port from 0
> functions to 50 (state.db `done = 50`, all composite=1.0). The
> harness is solid. Your task is to build a **scale-out engine** that
> lets the next 50 → 200 → 600 function ports happen with minimal
> orchestrator overhead.
>
> Read `CLAUDE.md` and `PHASE.md` first. Then this file top to bottom.

## TL;DR

**Done this session:** 50/50 functions ported, all composite=1.0 against
libmpfr-derived goldens. Iter histogram: 41 one-shot (82%), 6 two-iter
(12%), 3 six-iter (6% — `round`, `div`, `sqrt`). Pushed in 10 commits.
Worklog shards 001–004 cover the cycle.

**Your job:** Build the scale-out engine — turn the orchestrator's
~10 commands/function pattern into a tighter ~2 commands/function
loop, and add the missing pieces (`callgraph.py`, batched-prep
templates, auto state.db row, push-cadence automation).

**Target:** Take the next agent from 50 → 100 functions in a single
orchestrator session under halt-on-failure (same policy as the 50).
PHASE.md remains `Pilot` until auto-escalate is enabled.

## What works (don't change)

| component | path | status |
|---|---|---|
| Locked schema | `src/core.ts` | Frozen since Step 2; never modify without ADR |
| Worker isolation | `eval/harness/worker.ts` | Solid; ports 50/50 ran clean |
| Grader | `eval/harness/runner.ts` | Solid (1287 LOC; see `mpfr-ts-5a3` for golf candidate) |
| Codec | `eval/harness/value_codec.ts` | Handles Result, MPFR, scalar (bigint/number/boolean), struct, NaN/Inf tokens |
| AST gate | `eval/harness/ast_check.ts` | Solid except `import { type X }` false positive (workaround documented) |
| Wire helpers | `eval/golden_master/common.h` | Has `jl_kv_*` + `jl_output_scalar_*` for every type we've hit |
| Substrate | `src/internal/{mpn,mpfr}/` | 6 files: 4 mpn ports + `round_raw` + `cmp_raw` |
| Prompt renderer | `eval/driver/prompts.py` | 35–47 KB prompts, includes worked example + absolute-path guidance |
| Dry-run driver | `eval/driver/ralph.py` | `--function <fn> --dry-run` + `--list-pending`; needs the scale-out additions below |
| State DB | `eval/state.db` | 50 functions + 50 runs rows; schema unchanged from Step 3 |

## Throughput data (concrete)

From this session (10 batches, 50 ports):

- **Sonnet success rate:** one-shot 82%, ≤2 iter 94%, full 6-iter budget 6%.
- **Prep subagent token cost:** ~100–250 K per batch (varies with batch size).
- **Sonnet subagent token cost:** ~40–100 K per batch.
- **Orchestrator wall time:** ~10–20 min per batch end-to-end.
- **Batch size:** 3–5 functions worked well; 1 (the Pilot) was slow; >5 untested.
- **Trivial-port latency** (predicates, constructors): ~5 min orchestrator time each.
- **Algorithmic-port latency** (arithmetic, rounding): ~15 min each.
- **Hardest** (`round`, `div`, `sqrt`): full 6-iter sonnet budget, ~30 min each.

Per-function commands from the orchestrator side, in the current pattern:

1. `sqlite3 eval/state.db "INSERT INTO functions ..."` — seed row
2. `Agent(prep)` — write spec + driver + port + refs + spot-grade
3. `python3 ralph.py --function X --dry-run > /tmp/eval_X/prompt.txt`
4. `Agent(sonnet, prompt-file)` — port to /tmp + grade
5. `bun runner.ts ...` — independent re-verify
6. `sqlite3 eval/state.db "INSERT INTO runs ...; UPDATE functions ..."`
7. (every batch) `git add -A && git commit && git push`
8. (every batch) `bd export -o .beads/issues.jsonl`

That's ~7–10 orchestrator actions per batch of 3–5 functions. The
scale-out engine should compress this to ≤3.

## What "scale-out engine" should do

**Goal:** orchestrator dispatches one prep, one sonnet, runs one script.
The script handles seeding, prompt rendering, grading, DB updates,
git commit, push.

### Components to build

1. **`eval/driver/callgraph.py`** — extract function-call graph from
   `mpfr/src/*.c` (and GMP refs where applicable). Output is a JSON
   file of `{function: {deps: [...], class: '...', priority: int}}`
   that lets ralph.py auto-pick the next pending function by
   topological rank. Doesn't have to be perfect — heuristic regex over
   `mpfr_*(` and `mpn_*(` callsites is fine; clang AST is overkill for
   Pilot but worth flagging for Production.

2. **`scripts/next-batch.sh`** (or extend ralph.py) — picks N pending
   functions of similar class/complexity, seeds state.db rows, renders
   prompts, prints to stdout the prep-subagent prompt skeleton with
   per-function deliverable paths interpolated. The orchestrator just
   dispatches one prep agent with that skeleton.

3. **`scripts/grade-batch.sh`** (or extend ralph.py) — takes a batch
   manifest (function names + class + n_cases each), grades each port
   at `/tmp/eval_<fn>/port.ts`, inserts run rows, updates function
   rows, commits + pushes if all pass. If any fails, halts and prints
   diagnostic.

4. **Auto state.db seeding in the prep prompt** — the prep template
   should produce a port + ALSO emit a `state_seed.sql` file the
   orchestrator can pipe into sqlite3. Removes step (1) from the loop.

5. **`bd dep add` automation** — for every newly-completed function,
   if its `deps` field references already-done ports, do nothing;
   else flag the missing prerequisite. Today this is implicit.

6. **Push cadence policy** — current convention is push every batch.
   Codify in ralph.py: `--auto-push` flag that batches commits and
   pushes after N functions or T minutes.

7. **(Optional) Anthropic SDK direct path** — for true autonomous
   ralph loops outside the orchestrator's subagent dispatch. Today
   the orchestrator IS the dispatcher; if we want headless overnight
   runs, ralph.py needs an SDK path with API key from env. Defer
   until orchestrator-driven scaling proves insufficient.

### Suggested ralph.py CLI surface (extension)

```
python3 eval/driver/ralph.py \
  --next [--batch-size N] [--filter class=arithmetic]
  # → prints next N pending functions by topo_rank; seeds rows; renders prompts

python3 eval/driver/ralph.py --grade <fn1> <fn2> ...
  # → spot-grades each port at /tmp/eval_<fn>/port.ts; INSERTs runs; UPDATEs functions
  # → exit 0 if all >= 0.95; exit 1 with diagnostics on any fail

python3 eval/driver/ralph.py --commit-batch <msg>
  # → bd export + git add + commit + push; one orchestrator command
```

## Open bd issues affecting scale-out

| id | priority | what | impact |
|---|---|---|---|
| `mpfr-ts-wli` | P2 | ast_check flags `import { type X }` mixed syntax | Workaround in place (split into `import type` + `import` lines). Fix would let prompts.py use the standard mixed form. |
| `mpfr-ts-6ps` | P3 | state.db `runs.perf_grade` and `usd_est` NOT NULL but we don't measure | Currently set to 0.0; any perf-grade-based query is noise. Schema relax via ADR. |
| `mpfr-ts-5a3` | P4 | runner.ts is 1287 LOC | Cosmetic; defer until throughput signal changes. |
| `mpfr-ts-upg` | P3 | worked-example-eval-leak for function #1 only | Self-resolved (worked example is `mpn_add_n`; for #2+ the target differs). |
| n_throw conflation | P3 | Output mismatches counted as `n_throw` alongside real exceptions | Diagnostic-quality only; composite still correct. |

None block scale-out; all should land before Production-phase auto-escalate.

## Architectural decisions to inherit (do NOT relitigate)

These are locked by prior worklog and memory:

- **Immutable TS API** with `Result {value, ternary}` shape. No `mpfr_t` rop handles.
- **Substrate split** under `src/internal/`. Substrate ports do NOT import `core.ts`; public ports MUST.
- **`bigint` everywhere** for prec, exp, mant, signed/unsigned long types. Never `number` for those.
- **Five rounding modes**, no `RNDNA`.
- **Signed zero is observable** in arithmetic ops; NOT observable in `mpfr_*_p` predicates (per IEEE 754 / mpfr_equal_p).
- **NaN throws in cmp-family** (returns int), returns false in predicates (returns boolean). This is the divergence-from-C documented per port.
- **Public ports throw `MPFRError`** for malformed input (EPREC/EROUND/EDOMAIN); never for routine MPFR behaviour (overflow → ±Inf, etc.).
- **NaN has prec=0n, sign=1** by canonical schema. Don't propagate libmpfr's NaN sign-bit.
- **mpn limbs are LITTLE-ENDIAN** (`limbs[0]` is LSB).
- **Halt-on-failure** during the 50→100 push. Don't enable auto-escalate without an explicit phase transition.

## Hard-won lessons from this session

Documented per-port in the JSDoc; consolidated here for fast lookup.

### Wire format / harness

1. **`runner.ts` rejects `--class conversion`** — the 4 valid classes are
   `substrate | arithmetic | transcendental | misc`. Use `misc` for
   conversion ops (1000ms budget; cheap to fit set_d-style work).
2. **`Number.isFinite` ≠ `Number.isInteger`** — `BigInt(3.14)` throws. Use `Number.isInteger` when coercing a `number` to `bigint`.
3. **`Object.is` for number scalar comparison** — handles NaN==NaN and
   +0≠-0 correctly. Plain `===` is wrong for both.
4. **`looksLikeMpfr` check requires bigint fields** — port outputs use
   bigint mant/exp/prec; wire outputs use string decimal-encoded bigint.
   Don't auto-decode port outputs through `decodeInputValue`.
5. **Relative `--port path/to/file.ts` resolves against `process.cwd()`** —
   was originally resolving against `import.meta.url` (the runner's
   directory). Fixed in `pathToUrl`. Don't regress.
6. **Public ports under `/tmp/` cannot use `../core.ts`** — `/tmp/core.ts`
   doesn't exist. prompts.py emits absolute-path import guidance; the
   orchestrator rewrites paths when promoting to canonical location.
   Sonnet occasionally misses this on iter 1; it recovers iter 2.

### Algorithmic traps

7. **Catastrophic cancellation requires post-cancel bit-length recompute** —
   subtracting two near-equal MPFRs can produce a value with many
   leading zeros. The result exponent must be computed from
   `bitLength(post_subtract)`, not from the pre-subtract `extPrec`.
   Caught by adversarial cases in `mpfr_add`'s golden.
8. **`mpfr_round` uses ties-AWAY-from-zero**, not ties-to-even.
   Different from `RNDN` semantics in arithmetic ops.
9. **`mpfr_round` is a SINGLE-PASS algorithm** — `round(4.7, prec=2) = 4`,
   not 6. Don't compute "nearest integer" then quantize to prec —
   that gives wrong answers. Read `mpfr/src/rint.c`.
10. **`mpfr_div`: quotient bit-length is `prec+1` or `prec+2`**,
    determined dynamically. Sticky bit = `(remainder !== 0n) ? 1n : 0n`
    ORed with bits below the round bit. Encode as
    `fullQ = (q_top << 1) | sticky; srcPrec = prec + 2`.
11. **`mpfr_sqrt`: parity adjustment is in the SHIFT, not pre-mult** —
    if `x.exp` is odd, you can either pre-multiply mantissa by 2 OR
    adjust the bit-shift `k` by 1. The latter avoids losing a bit
    (sonnet's first cut chose the former and lost precision).
12. **`isqrt` for bigints**: no stdlib; write Newton's method.
    Invariants: `root^2 <= n < (root+1)^2`, `remainder = n - root^2`.
13. **Subnormal-double offset is `-1074 + cnt`, not `-1022 + cnt`** —
    set_d's subnormal exponent extraction caught this. The 52-bit
    fraction normalises to a 53-bit significand; the `+1` in the shift
    count matters.
14. **`mpfr_init2(prec)` returns posZero(prec)** in the TS port — the C
    function leaves mantissa undefined; the TS surface picks a
    deterministic zero. Documented as divergence-from-C.

### Prompting

15. **The worked example (`mpn_add_n`) leaks for function #1 only** —
    sonnet for `mpn_add_n` essentially transcribed. For #2 onward the
    target differs from the worked example; sonnet must algorithmically
    port. Don't read function-#1 results as evidence of porting capability.
16. **Read-restrict the sonnet agent**: ban reading
    `src/ops/<fn>.ts`, `eval/reference_ports/{correct,broken}/mpfr_<fn>.ts`,
    `eval/functions/<fn>/golden.jsonl`, `eval/functions/<fn>/golden_driver.c`.
    Without restrictions sonnet would file-copy.
17. **Sonnet sometimes misses the absolute-path import guidance** —
    despite it being in the rendered prompt. Costs 1 iteration of 6.
    Acceptable cost; could be reduced by moving the guidance higher.

### Prep agent

18. **Batch prep agents handle 3–5 related functions well** — token
    cost ~150–250K per batch; produces full deliverables. >5 was
    untested but may hit context limits.
19. **PREC_MAX cases allocate megabytes** — `mpfr_init2(PREC_MAX)` from
    libmpfr allocates ~256 MB of mantissa storage. Cap golden cases at
    4096-bit precision; the TS port's prec-validation gate is
    implicitly verified by the validateArgs path.
20. **Broken-port calibration is brittle near 0.5** — gap-to-0.45 cases
    (cmp predicates, set_si) need explicit adversarial inflation. If a
    broken port scores 0.48, document it; if it scores 0.52, fail the
    prep and re-tune.

## The 50 functions (alphabetical)

Substrate: `mpn_add_n`, `mpn_cmp`, `mpn_lshift`, `mpn_sub_n`.

Public surface (46):

`mpfr_abs`, `mpfr_add`, `mpfr_ceil`, `mpfr_cmp`, `mpfr_cmp_d`,
`mpfr_cmp_si`, `mpfr_cmp_ui`, `mpfr_copysign`, `mpfr_div`,
`mpfr_div_2si`, `mpfr_equal_p`, `mpfr_floor`, `mpfr_get_d`,
`mpfr_get_si`, `mpfr_get_ui`, `mpfr_get_z`, `mpfr_greater_p`,
`mpfr_greaterequal_p`, `mpfr_inf_p`, `mpfr_init2`, `mpfr_less_p`,
`mpfr_lessequal_p`, `mpfr_lessgreater_p`, `mpfr_max`, `mpfr_min`,
`mpfr_mul`, `mpfr_mul_2si`, `mpfr_nan_p`, `mpfr_neg`, `mpfr_number_p`,
`mpfr_round`, `mpfr_set_d`, `mpfr_set_inf`, `mpfr_set_nan`,
`mpfr_set_si`, `mpfr_set_ui`, `mpfr_set_z`, `mpfr_set_zero`,
`mpfr_setsign`, `mpfr_sgn`, `mpfr_signbit`, `mpfr_sqrt`, `mpfr_sub`,
`mpfr_trunc`, `mpfr_unordered_p`, `mpfr_zero_p`.

## Next 50 — suggested roster (by class, easy-to-hard)

To get from 50 → 100 functions:

**Easy wins (~15 functions, single prep batch + ~2 sonnet batches):**
- Accessors: `mpfr_get_prec`, `mpfr_get_exp`, `mpfr_set_prec` (`mpfr_set_prec_raw` is internal)
- More set/get integer types: `mpfr_set_si_2exp`, `mpfr_set_ui_2exp`, `mpfr_get_si_2exp` (returns int + exp)
- Sign helpers: `mpfr_swap` (in immutable TS: returns [b, a] — divergent), `mpfr_set` (copy with prec change)
- More integer rounding: `mpfr_rint_round`, `mpfr_rint_ceil`, `mpfr_rint_floor`, `mpfr_rint_trunc` (rounded with explicit rnd mode)
- Distance ops: `mpfr_dim` (positive difference), `mpfr_diff` (variant)
- Constants: `mpfr_set_zero` already done; add `mpfr_nan` (alias)

**Medium (~20 functions, 4–5 batches):**
- Arithmetic with primitives: `mpfr_add_si`, `mpfr_add_ui`, `mpfr_sub_si`, `mpfr_sub_ui`, `mpfr_mul_si`, `mpfr_mul_ui`, `mpfr_div_si`, `mpfr_div_ui`
- Power of 2: `mpfr_mul_2ui`, `mpfr_div_2ui` (similar to `_2si`)
- Next-representable: `mpfr_nextabove`, `mpfr_nextbelow`, `mpfr_nexttoward`
- Modular: `mpfr_fmod`, `mpfr_modf`, `mpfr_remainder`, `mpfr_remquo`
- More substrate: `mpn_zero`, `mpn_copyi`, `mpn_copyd`, `mpn_rshift`, `mpn_mul_1`, `mpn_addmul_1`, `mpn_submul_1`

**Hard (~15 functions, 5–6 batches with iteration budget headroom):**
- Transcendentals (new ground): `mpfr_exp`, `mpfr_log`, `mpfr_log2`, `mpfr_log10`, `mpfr_exp2`, `mpfr_exp10`
- Trigonometric: `mpfr_sin`, `mpfr_cos`, `mpfr_tan`, `mpfr_atan`, `mpfr_atan2`
- Hyperbolic: `mpfr_sinh`, `mpfr_cosh`, `mpfr_tanh`
- Power: `mpfr_pow`, `mpfr_pow_si`, `mpfr_pow_ui`

**Transcendentals are the next real test.** They use MPFR's
prec-extension loop: compute at extra prec, check if rounding is
guaranteed correct via the round-bit-equals-half-test, retry at more
prec if not. No current port exercises this pattern. The prompt should
embed the prec-extension idiom explicitly when the first transcendental
is attempted; expect 6-iter sonnet runs and potentially some functions
that don't converge under sonnet alone.

## Step-by-step plan for your first session

1. **Read these files** (in order): `CLAUDE.md`, `PHASE.md`, this
   worklog, `docs/PILOT_PLAN.md`, `docs/worklog/004-50-ports-complete.md`,
   `eval/driver/prompts.py`, `eval/driver/ralph.py`, `eval/harness/runner.ts`,
   `src/internal/mpfr/round_raw.ts`, `src/internal/mpfr/cmp_raw.ts`.

2. **Verify state**: `bun x tsc --noEmit` clean, `bun eval/acceptance/step5/run.ts` 5/5, `sqlite3 eval/state.db "SELECT COUNT(*) FROM functions WHERE status='done'"` = 50.

3. **Fix the cheap blocker** (`mpfr-ts-wli`): strip `import` statements
   before applying `REDECL_PATTERNS` in ast_check.ts (mirror the
   comment/string strip already there for `ANY_PATTERNS`). Re-grade a
   public port (e.g. `mpfr_init2`) — must still pass. This lets future
   prompts use the standard `import { type X, Y, Z }` syntax.

4. **Build `eval/driver/callgraph.py`**: walk `mpfr/src/*.c`, regex
   `mpfr_[a-z0-9_]+|mpn_[a-z0-9_]+`, build `{caller: [callee, ...]}`.
   Topologically sort; emit `eval/driver/callgraph.json`. Functions
   not yet in state.db are candidates for the next batch.

5. **Extend ralph.py** with `--next [--batch-size N]` and `--grade <fns...>` and `--commit-batch <msg>` (CLI surface above). Test with the next batch (see step 7).

6. **Write `scripts/grade-batch.sh`** as a thin wrapper around
   `ralph.py --grade ... && ralph.py --commit-batch ...`. Pick one of:
   bash or Python (consistency with driver scripts says Python).

7. **Pilot the engine**: pick a batch of 4 easy functions
   (`mpfr_get_prec`, `mpfr_set_prec`, `mpfr_set`, `mpfr_swap`). Run the
   end-to-end loop using the new commands. Measure: how many
   orchestrator actions per function?

8. **Iterate**: target ≤3 orchestrator actions per batch. If you hit
   the target, scale to 25 more functions in this session (51–75).

9. **Worklog**: at 75 functions, write `docs/worklog/006-scale-out-75.md`
   with throughput data, any new bd issues, and the next handoff.

## Where things live

```
mpfr-ts/
├── CLAUDE.md                       laws + rules; re-read each session
├── PHASE.md                        "Pilot" (do not flip without ADR)
├── HANDOFF.md                      pointer to current handoff (this file)
├── docs/
│   ├── PILOT_PLAN.md               original 10-step plan, mostly done
│   ├── worklog/
│   │   ├── 001-step5-harness.md
│   │   ├── 002-pilot-mpn-add-n.md
│   │   ├── 003-pilot-complete.md
│   │   ├── 004-50-ports-complete.md
│   │   └── 005-scale-out-handoff.md   ← you are here
│   ├── memory/                     cross-device snapshot of project memory
│   └── adr/                        (empty; ADRs go here as schema bumps land)
├── src/
│   ├── core.ts                     LOCKED schema
│   ├── internal/
│   │   ├── mpn/{add_n,sub_n,cmp,lshift}.ts
│   │   └── mpfr/{round_raw,cmp_raw}.ts
│   └── ops/                        44 public ports
├── eval/
│   ├── state.db                    SQLite, single source of truth
│   ├── driver/
│   │   ├── schema.sql
│   │   ├── prompts.py              prompt renderer (~520 LOC)
│   │   ├── ralph.py                CLI (~140 LOC; extend per this handoff)
│   │   └── callgraph.py            ← TO BUILD
│   ├── harness/
│   │   ├── runner.ts               main grader (1287 LOC; see mpfr-ts-5a3)
│   │   ├── worker.ts
│   │   ├── value_codec.ts
│   │   └── ast_check.ts
│   ├── golden_master/
│   │   ├── common.h                wire emit + PRNG + libmpfr helpers
│   │   ├── build.sh
│   │   └── _smoke_driver.c         executable spec for common.h
│   ├── acceptance/
│   │   └── step5/                  RED→GREEN suite, still 5/5 GREEN
│   ├── functions/<fn>/             per-function spec.json + golden_driver.c + golden.jsonl (gitignored)
│   └── reference_ports/
│       ├── correct/                thin re-exports of src/ops/<fn>.ts and src/internal/mpn/<fn>.ts
│       └── broken/                 deliberately-buggy variants for mutation-prove
└── .beads/                         bd database (Dolt) + .gitignored issues.jsonl
```

## Session close protocol (when you finish)

Per `CLAUDE.md §Session close`:

1. Update `eval/state.db` with any in_flight runs (mark done / parked / slow).
2. Add `docs/worklog/006-*.md` shard summarising your batch.
3. Refresh memory snapshot if any memory file changed.
4. `bd export -o .beads/issues.jsonl`.
5. `git add -A && git commit && git push`.
6. Verify `git status` shows "up to date with origin".

## One final thing

This handoff is itself the worklog for the meta-step of "build the
scale-out engine". When you finish your first session, write
`docs/worklog/006-*.md` capturing what you actually built versus what
this spec proposed. Future handoffs read better when each one shows
its predecessors' contracts versus what landed.

Good luck.
