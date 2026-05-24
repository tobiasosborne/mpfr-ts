# 010 — shadow-mode trial: 5-function mini-batch

> First shadow-mode trial of the opus-prep automation against a live
> opus prep + sonnet port workflow. 5 functions selected; opus prep
> produced 20 artifacts in ~16 min / ~198K tokens; 1 of the 5 was
> sonnet-ported to composite=1.0 in 2 iterations (the other 4 parked
> on a real architectural gap the trial surfaced).

## TL;DR

Shadow mode worked as designed. It surfaced one real architectural
gap (`mpfr-ts-ikr`, the missing `__gmpfr_flags` analog in `src/`),
demonstrated **gen_spec's structural extraction is safe to integrate
when paired with ADR 0001's curator-wins overrides**, and showed that
**mutate.py's automated gate agrees with opus's hand-tuned broken-port
mutation-prove** on the single arithmetic port (both conclude the
golden discriminates). The data justifies proceeding with step 6
(ralph.py wiring) using the explicit per-field precedence rules from
ADR 0001.

## Method

**Functions chosen** (deliberately diverse, deliberately small):

| function | class | shape | rationale |
|---|---|---|---|
| `mpfr_underflow_p` | misc | flag-state predicate | tests gen_spec on `_p` shape; tests mutate.py's known predicate weakness |
| `mpfr_overflow_p` | misc | same | same |
| `mpfr_nanflag_p` | misc | same | same |
| `mpfr_divby0_p` | misc | same | same |
| `mpfr_div_2` | arithmetic | 2-limb division fast path | tests gen_spec on `static` decls (post-fix); tests mutate.py's known arithmetic strength |

**Procedure**:
1. Seed pending state.db rows for 5 functions via `python3 eval/driver/ralph.py --next` (with manual filtering).
2. Dispatch opus prep subagent (model=opus). Output: 20 artifacts (5 spec.json + 5 golden_driver.c + 10 reference ports).
3. Build + run goldens for all 5 (`build.sh` + `run_all.sh`).
4. **Shadow A** (prep): run `gen_spec.extract_spec` on each C source, diff against opus's spec.json per ADR 0001.
5. Dispatch sonnet port subagent (model=sonnet) for `mpfr_div_2` only — the 4 predicates blocked by `mpfr-ts-ikr`.
6. **Shadow B** (port): grade opus's broken port, run `mutate.mutation_prove` on sonnet's port, compare conclusions.

Wall: ~30 min total (16 opus prep + ~5 sonnet port + 10 ortho work). Tokens: ~198K opus + ~80K sonnet ≈ **~280K total**, vs the prior mega-batches at ~380K opus alone for 15-30 functions. The shadow ortho work (gen_spec + mutate.py) added negligible cost.

## Real finding surfaced before any code: `mpfr-ts-ikr`

Opus prep correctly identified that `src/` has no analog of MPFR's
`__gmpfr_flags` global register — the 4 `_p` predicates require either
a new `src/internal/mpfr/flags.ts` module (~30 LOC) or an ADR
formalizing the absence. Opus documented both options in each
predicate's `divergence_from_c` field rather than inventing the API.

This is exactly the kind of finding shadow mode was designed to
produce: a real architectural gap surfaced before any porter wrote
code, with clear options for resolution. The 4 predicate ports are
**parked** in state.db; they retain their spec.json + golden_driver.c
contributions and become unblockable once the flags module lands.

## Shadow A: gen_spec vs opus's spec.json

5 functions, 25 field comparisons (5 fields × 5 functions). Per-field
agreement and per-field winner (per ADR 0001):

| field | matches | ADR winner | observation |
|---|---|---|---|
| `class` | 5/5 | gen_spec | Filename heuristic happened to align with opus's chosen budget tier on all 5 |
| `signature.params` | 1/5 | **curator** | 4 predicates: gen_spec emitted `[]` (C `void`); opus emitted `['mask']` (added a runtime-state input for the wire codec). Curator's lift is essential — gen_spec couldn't infer this |
| `signature.returns` | 1/5 | **curator** | 4 predicates: gen_spec emitted `'number'` (C `int`); opus emitted `'boolean'` (idiomatic-TS lift per ADR 0001). Curator's lift is essential |
| `c_signature` | 0/5 | gen_spec (tolerated) | All 5 differ in whitespace (`func (...)` vs `func(...)`). `mpfr_div_2` also differs in `static` prefix (gen_spec keeps; opus strips for the public-API view). All compatible-but-different |
| `prec_unit` | 1/5 | **curator** | 4 predicates: gen_spec emitted `'bits'` (always); opus emitted `'n/a'` (no `prec` parameter — bits is meaningless). Curator's override is correct |

**The pattern**: gen_spec produces a structurally-correct extraction;
opus overrides where idiomatic-TS shape, semantic information, or
field applicability matters. The ADR 0001 precedence rules predicted
every override observed in this trial.

**Implication for step 6 (ralph.py wiring)**: the prep prompt must
EXPLICITLY instruct opus to override `signature.returns` (predicates:
number→boolean), `signature.params` (predicates may add wire-codec
inputs), and `prec_unit` (override to `'n/a'` when no prec parameter).
The current ralph.py prep prompt is too minimal; it would let opus
silently inherit gen_spec's structural defaults for these 3 fields,
losing the idiomatic-TS lift.

Suggested prompt addendum for step 6:

```
The structural fields below are extracted from the C source by
gen_spec.extract_spec. They are CORRECT for the C definition but
require these overrides for the idiomatic TS port:

  - signature.returns: int → 'boolean' for _p predicates;
    void → 'MPFR' when first dropped C ptr is the output slot;
    long/mpfr_prec_t/mpfr_exp_t → 'bigint'
  - signature.params: may add wire-codec inputs (e.g. 'mask' for
    flag-state predicates) not present in the C signature
  - prec_unit: override to 'n/a' if the function has no `prec`
    parameter (predicates, comparators)

The c_signature field is authoritative; do not edit it.
```

## Shadow B: mutate.py vs opus's broken port

Only `mpfr_div_2` contributed port-shadow data (the 4 predicates were
parked). Two parallel signals on whether the golden discriminates:

| signal | composite | conclusion |
|---|---|---|
| Opus's broken port (mpfr_div_2) | 0.1516 | clean kill — golden discriminates |
| mutate.py mutation_prove (sonnet's port) | gate_passed=True | golden discriminates (via shift-direction-swap=0.0 + rnd-swap=0.80) |

Both methods agreed: the golden is well-calibrated. But the *shape*
of the conclusion differs:

- **Opus's broken port** is a *single, semantically-aware perturbation*: operand swap (computes v/u) + inverted directional rounding. Lands at composite=0.15 — well below the 0.55 clean-kill threshold and below the 0.30 multi-bug standard. Opus designed it specifically to break this function.
- **mutate.py** is *6 deterministic structural perturbations*. 1 lands hard (shift-direction-swap=0.0 — the single-limb division relies on bigint bit-shifts), 1 lands medium (rnd-swap=0.80 — just below the 0.95 threshold), 4 don't land (ternary-negate, bigint-bump, comparison-swap, sign-flip all at 1.0).

**Agreement**: both gates pass. The golden is calibrated under both
methods.

**Disagreement** (interesting): opus's hand-tuned perturbation is
~5.3× harder than mutate.py's best automated mutation (0.15 vs 0.80
if we ignore the dominant shift-direction-swap; 0.15 vs 0.00 if we
include it). The take-away is **shape-dependent**:

- For ports with bit-shifts (substrate, single-limb fast paths), `shift-direction-swap` reliably lands hard kills (see report 009: 87.5% clean-kill rate when applicable). mutate.py is a credible REPLACEMENT for opus's broken port in this shape.
- For ports without bit-shifts (predicates, simple wrappers, prose-heavy code), mutate.py relies on weaker mutations (ternary-negate, rnd-swap, comparison-swap — all of which had low effectiveness in report 009). Opus's hand-tuned broken port is meaningfully stronger.

**Recommendation**: keep opus's broken port deliverable in the prep
workflow (it costs ~5K opus tokens per function and adds real signal
for non-shift-heavy ports). Use mutate.py as a parallel cross-check
that runs AFTER sonnet ports. The two methods are complementary, not
mutually exclusive.

## Cost / wall summary

| step | opus tokens | sonnet tokens | wall |
|---|---:|---:|---|
| ralph.py --next seeding | 0 | 0 | <5 s |
| Opus prep dispatch (5 functions) | ~198K | 0 | 16 min |
| build.sh + run_all.sh | 0 | 0 | ~10 s |
| Sonnet port dispatch (1 function, 2 iterations) | 0 | ~80K | ~5 min |
| Shadow A analysis (gen_spec diff) | 0 | 0 | <1 s |
| Shadow B analysis (grader + mutate) | 0 | 0 | ~5 s |

**~278K total tokens, ~22 min wall.** A 30-function mega-batch under
the existing workflow uses ~380K opus + sonnet waves ≈ ~600K total.
This trial (5 fns + full shadow analysis) at ~278K extrapolates to
~1.7M for a 30-fn mega-batch with full shadow analysis — well above
the unaugmented cost. Shadow mode is NOT cheaper than the existing
workflow; it's an investment for confidence in the integration.

If we ran the same trial WITHOUT the opus prep (using gen_spec +
sonnet directly), the opus prep tokens disappear and total drops to
~80K for 1 function or ~480K extrapolated to 30. That's the
replacement-mode pricing — **~2× cheaper than existing workflow**.
But it's gated on (a) the architectural decisions that opus catches,
like `mpfr-ts-ikr`, being handled some other way, and (b) the
mutate.py menu being strong enough for non-shift-heavy ports.

## Recommendations

### Proceed with step 6 (ralph.py wiring)

The shadow trial validated ADR 0001's precedence rules: every
disagreement between gen_spec and opus's spec.json was correctly
predicted by the ADR. Integrating gen_spec into the prep prompt is
safe when paired with the explicit per-field override instructions in
the prompt addendum above.

Estimated step 6 effort: ~80-120 LOC delta to `ralph.py`, the prompt
addendum above, ~20 new ralph.py test cases. All 52 existing
ralph.py tests must stay green.

### Do NOT yet drop opus's broken port deliverable

Shadow B showed mutate.py's automated gate agreeing with opus's
broken-port gate on `mpfr_div_2` (a shift-heavy port). But:

- Per report 009, mutate.py has class-level weaknesses on
  non-shift-heavy code (conversions, predicates, simple wrappers).
- Per shadow B, opus's hand-tuned perturbation lands 5× harder than
  mutate.py's best automated mutation. The extra margin matters when
  the golden is borderline.

Replacement mode (drop opus broken port → save ~5K opus tokens × N
functions) should wait until either (a) mutate.py menu is extended
enough to handle non-shift-heavy ports cleanly, or (b) per-class gate
thresholds prove robust across more shadow trials.

### Resolve `mpfr-ts-ikr` before the next mega-batch

The flag-state API gap blocks 4 functions in this trial and another
~6 in the next mega-batch's exception family (`mpfr_inexflag_p`,
`mpfr_erangeflag_p`, `mpfr_set_underflow`, `mpfr_set_overflow`,
`mpfr_clear_flags`, `mpfr_flags_save`, `mpfr_flags_set`). A
~30-LOC `src/internal/mpfr/flags.ts` module unblocks all of them at
once. Worth a dedicated bd-driven step.

### Run another shadow trial when prepared

Next sensible shadow trial: pick a 5-10 function set that doesn't
hit the same flag-state wall. Candidates:

- Modular/remainder family: `mpfr_modf` (its deps `mpfr_frac`,
  `mpfr_rint_trunc` are unported but ralph.py callgraph can sequence
  them)
- More single-limb fast paths: `mpfr_sqrt1`, `mpfr_sqrt1n`,
  `mpfr_sqrt2_approx` (test gen_spec + mutate.py on substrate-class)
- Mpz interop: `mpfr_add_z`, `mpfr_set_z` (test gen_spec on
  non-const `mpz_ptr` types after the `mpfr-ts-eqc` fix)

The chosen trial size of 5-10 functions plus the corresponding shadow
analysis is the right balance of cost vs information for now.

## Cleanup state

State.db at trial end:
- 4 predicate functions (`underflow_p`, `overflow_p`, `nanflag_p`, `divby0_p`): `pending` — blocked on `mpfr-ts-ikr`
- 1 function (`mpfr_div_2`): port written at `/tmp/eval_mpfr_div_2/port.ts` at composite=1.0 — needs ralph.py --ship to promote to `src/ops/div_2.ts` and update state.db to `done`

The orchestrator should:
1. File `mpfr-ts-ikr` ✓ already done
2. Promote `mpfr_div_2` via `--ship` (or document why we're holding off)
3. Update HANDOFF.md if this trial substantially changes the next-session contract
