# 009 -- mutator menu calibration across function classes

> Step 5 of pre-trial validation. Read-only analysis: runs
> `eval/driver/calibrate.py` against stratified samples of `status='done'`
> ports, characterizes mutator coverage per class, and files follow-up
> bds for class-level gaps.

## TL;DR

At a stratified N=6 per class (24 fixtures total), the mutation-prove
gate (composite <= 0.95 on at least one applied mutation) passes for
**19/24 = 79%** of sampled ports. All four classes pass the gate on
the majority of fixtures: arithmetic 5/6, conversion 4/6, misc 5/6,
substrate 5/6. The N=1 smoke pattern from step 4 ("only substrate
catches mutations") was sample noise and does **not** hold at N>=4.
The dominant catch is **shift-direction-swap** (88% clean-kill rate
when applicable), which lands on any port that uses bit-shifts in
mantissa/exponent arithmetic. `rnd-swap` is **dead weight** at this
sample (0 clean kills across 16 applications), as is most of
`comparison-swap` and `sign-flip`. Recommendation: keep the menu as
is (the dead mutators cost ~1 sec/run and are cheap insurance), but
file P2 bds against two real mutator bugs (ternary-negate corrupts
destructuring patterns; op-swap corrupts the function's own
declaration), and a P3 bd to harden gate eligibility for ports too
trivial to mutate productively (`mpfr_swap`, `mpfr_set_inf`).

## Method

```bash
python3 eval/driver/calibrate.py --sample-per-class 4 --json > /tmp/calib_n4.json
python3 eval/driver/calibrate.py --sample-per-class 6 --json > /tmp/calib_n6.json
```

The two runs share seed=42; N=6 is a strict superset of N=4 (verified
by intersecting fixture sets). N=4 took 87 s wall, N=6 took ~60 s wall
(parallelizable; substrate fixtures with fewer mutations run faster).
Both runs completed cleanly with no fixture skips -- 0 warnings to
stderr in either run.

Aggregated inline in Python with statistics.fmean over the
MutationOutcome dataclass shapes from `eval/driver/mutate.py`. No
new repo files (a /tmp/analyze_calib.py helper was used and then
removed). Gate definition follows `mutate.py`: a fixture's
`gate_passed` is True when >=1 applied non-init-failed mutation has
composite <= 0.95. `clean_kill` is the stricter threshold composite
<= 0.55.

## Aggregate stats

```
Total fixtures sampled (N=6):           24
Fixtures with gate_passed=True:         19  (79.2%)
Fixtures with >=1 clean_kill:           18  (75.0%)
Mean clean_kills per fixture:            0.875
Mean composite (all applied, non-init): 0.733
```

Across all 24 fixtures and 7 mutations, 85 mutations were applied and
actually ran (composite not None and module_init_failed is False).
Three additional applications were `module_init_failed` (op-swap on
`mpfr_add` ; ternary-negate on `mpfr_div_2si` and `mpfr_div_2ui`) -- 
these are real mutator bugs, see Patterns M1 and M2 below.

## Per-class patterns

| class      | n_fns | gate pass | mean kills/fn | mean composite |
|------------|-------|-----------|---------------|----------------|
| arithmetic | 6     | 5/6 (83%) | 1.00          | 0.775          |
| conversion | 6     | 4/6 (67%) | 0.67          | 0.778          |
| misc       | 6     | 5/6 (83%) | 1.00          | 0.752          |
| substrate  | 6     | 5/6 (83%) | 0.83          | 0.625          |

Both N=4 and N=6 show the same relative ordering: **substrate has the
lowest mean composite** (0.58 / 0.62), meaning when a mutator does
fire on a substrate port it tends to land harder. Conversion has the
**highest gate failure rate** at N=6 (2/6 = 33% failure), driven by
the trivial-delegation pattern (Pattern P2 below).

The N=1 smoke from step 4 had falsely suggested arithmetic and
conversion never trigger the gate. At N=6 every class passes the
gate on the majority. The smoke's `mpfr_sub` and `mpfr_get_d_2exp`
failures were genuine (both reproduce in N=6) but not representative.

## Per-mutation patterns

Across 24 fixtures, the menu's effectiveness varies by an order of
magnitude:

| mutation              | times appl. | kills | kill rate | mean comp. when appl. | init_failed |
|-----------------------|-------------|-------|-----------|------------------------|-------------|
| shift-direction-swap  | 16          | 14    | 87.5%     | 0.183                  | 0           |
| op-swap               | 2           | 1     | 50.0%     | 0.500                  | 1           |
| ternary-negate        | 8           | 2     | 25.0%     | 0.867                  | 2           |
| sign-flip             | 4           | 1     | 25.0%     | 0.706                  | 0           |
| bigint-bump           | 15          | 2     | 13.3%     | 0.843                  | 0           |
| comparison-swap       | 20          | 1     | 5.0%      | 0.911                  | 0           |
| rnd-swap              | 16          | 0     | 0.0%      | 0.984                  | 0           |

- **shift-direction-swap** is the workhorse: applicable to most
  ports that touch mantissa/exponent layout, and it almost always
  kills cleanly because flipping `<<` to `>>` (or vice versa)
  corrupts the bit alignment immediately.
- **rnd-swap** never clean-killed in this sample. Hypothesis B
  (below) explains why: most goldens are dominated by RNDN cases
  and ports either pass `rnd` through unchanged (mutation hits
  only string literals in unused branches) or the RNDU/RNDD swap
  is invisible for the value classes the golden exercises
  (exact returns, zeros, infinities).
- **comparison-swap** has a 5% kill rate. Pattern M3 (below)
  identifies the cause: the mutator picks the *first* comparison,
  which is often a precondition check (`i < n`, `prec >= 1n`)
  whose perturbation doesn't change observable output for
  reasonable inputs.
- **ternary-negate** clean-killed only on ports where ternary is
  computed inline; on delegating ports (mpfr_sub, mpfr_get_d1) the
  ternary value is sourced from the delegated function and the
  mutation site reduces to `ternary: 0` or `ternary: -(0)`, both
  of which equal 0 numerically. Documented in Pattern P1.

## Cross-tabulation: (class x mutation) clean-kill matrix

Rows are classes, columns are mutations. Cell is `kills / applied`
(applied = composite not None and not module-init-failed). N=6 data.

| class       | bigint-bump | cmp-swap | op-swap | rnd-swap | shift-swap | sign-flip | ternary-neg |
|-------------|-------------|----------|---------|----------|------------|-----------|-------------|
| arithmetic  | 0/3         | 1/6      | 1/1     | 0/5      | 3/4        | 0/2       | 1/4         |
| conversion  | 0/3         | 0/3      | 0/0     | 0/6      | 3/4        | 1/1       | 0/1         |
| misc        | 0/5         | 0/5      | 0/1     | 0/5      | 5/5        | 0/1       | 1/3         |
| substrate   | 2/4         | 0/6      | 0/0     | 0/0      | 3/3        | 0/0       | 0/0         |

Mean composite when applied (N=6):

| class       | bigint-bump | cmp-swap | op-swap | rnd-swap | shift-swap | sign-flip | ternary-neg |
|-------------|-------------|----------|---------|----------|------------|-----------|-------------|
| arithmetic  | 0.985       | 0.803    | 0.000   | 1.000    | 0.331      | 0.874     | 0.882       |
| conversion  | 0.980       | 1.000    | n/a     | 0.999    | 0.249      | 0.075     | 1.000       |
| misc        | 0.889       | 0.976    | 1.000   | 0.948    | 0.065      | 1.000     | 0.803       |
| substrate   | 0.575       | 0.922    | n/a     | n/a      | 0.095      | n/a       | n/a         |

Per-class read:
- **arithmetic** is sensitive to op-swap (perfect kill, 1/1) and
  comparison-swap (1/6) -- but only when the comparison hits an
  algorithmic branch rather than a precondition check. shift-swap
  works 3/4 times.
- **conversion** is dominated by shift-swap (3/4) and sign-flip
  (1/1, on `mpfr_set_ui` where the sign assignment is observable).
  Notably 0/3 on cmp-swap and 0/6 on rnd-swap.
- **misc** is captured almost entirely by shift-swap (5/5) and
  ternary-negate (1/3 on `mpfr_sqr_1n`).
- **substrate** is mutation-rich via bigint-bump (2/4, hitting
  mantissa MSB constants) and shift-swap (3/3).

## Patterns observed

Patterns are grouped by category. Each is supported by specific data
points; the per-fixture log is in `/tmp/calib_n6.json`.

### P1: delegation invariance -- 3 fns -- structural

When a port's body reduces to a single delegation call (e.g.
`return mpfr_get_d(x, _defaultRoundingMode)`), most mutators are
applicable but vacuous: there is no algorithm to perturb.

Affected: `mpfr_get_d1` (delegates to mpfr_get_d), `mpfr_swap`
(returns a fresh `{a:b, b:a}`), `mpfr_get_d_2exp` (delegates with
a temporary alias).

Evidence: `mpfr_get_d1` applies only rnd-swap, composite=1.0 (no
catch). `mpfr_swap` has zero applicable mutations. `mpfr_get_d_2exp`
applies only rnd-swap, composite=1.0.

This is **not a port defect**: the delegation IS the algorithm. The
delegated function carries its own mutation gate. But the calibrate
runner's current definition counts these as gate failures.

### P2: trivial-body constant returns -- 1 fn -- structural

`mpfr_set_inf` only applies op-swap (its docstring mentions other
mpfr functions) and sign-flip, both at composite=1.0 because the
function body is a 3-way switch on `sign in {1,-1}` and a constant
posInf/negInf return.

Same root cause as P1: the function is so small there's nothing
to perturb productively.

### P3: dispatcher-with-precondition-checks -- 1 fn -- mutator gap

`mpn_cmp` has only one applicable mutation (comparison-swap), and
it lands on the first `i < n` precondition check (line 133), not
on the algorithmic `a > b` / `a < b` at line 167. Composite=1.0.

This is a **mutator gap**: comparison-swap takes the first match.
A targeted variant that iterates all comparisons, or one that
prefers comparisons inside loops over those at function entry,
would fix this. The same issue affects `mpfr_mpn_cmpzero`, where
comparison-swap landed at 0.77 (gate passes but doesn't clean-kill).

### P4: composition-based ports route bugs out -- 1 fn -- structural

`mpfr_sub` delegates to `mpfr_add` (see `src/ops/sub.ts` L40-L77).
Only 3 mutations apply (rnd-swap, ternary-negate, comparison-swap),
none clean-killed. Composite values: 1.0, 1.0, 0.986. **Hypothesis
A in the brief is confirmed**: the port composes mpfr_add and
relies on mpfr_add's own validation, so most mutation sites
inside sub.ts are not on the rounding-critical path.

The op-swap mutator could in principle catch this -- but
`mpfr_add` in sub.ts appears as a function CALL inside the
delegation, not in any add-vs-sub-pair pattern the mutator looks
for. The pair table is `[mpfr_add, mpfr_sub]` / `[mpfr_mul,
mpfr_div]`: applicable only when one of the pair appears as a
call AND its mate does NOT (so the swap is unambiguous). Inside
sub.ts both `mpfr_add` (as a call) and `mpfr_sub` (as its own
function name/declaration) appear, so the heuristic rejects.

### P5: shift-direction-swap is the universal sledgehammer -- 16 fns -- intended

87.5% of shift-direction-swap applications were clean kills. The
2 non-kills were `mpfr_get_z` (composite=0.81; the shifted bits
land in low-order positions the golden only weakly observes) and
`mpfr_mpn_cmpzero` (composite=0.77; same -- the shift is in a
helper that returns a coarser signal than rounded values).

Both still pass the gate. This is the mutator working as
intended: any port that does serious bit-level work catches
shift-swap, and ports that do shift-light work catch it
incompletely but still below 0.95.

### P6: rnd-swap is dead weight -- 16 applications, 0 kills -- mutator gap

`rnd-swap` swaps `'RNDN' <-> 'RNDZ'` (or `'RNDU' <-> 'RNDD'`) in
quoted string literals. Mean composite across 16 applications was
0.984. Zero clean kills.

Hypothesis B-adjacent: most goldens, even on rnd-passing ports,
have a heavy mix of RNDN cases that are exact for the input value
(so swapping to RNDZ produces the same value). A spot-check of
golden tag distributions for `mpfr_get_z`, `mpfr_get_d1`, and
`mpfr_set_z` shows healthy 5-tag coverage (happy/edge/adversarial/
fuzz/mined all populated), so undersampling is NOT the cause -- 
the cases just happen to be ones where rounding mode doesn't
move the output across a representable boundary.

A stronger variant: a mutator that REMOVES one of the rounding
branches (replaces `rnd === 'RNDN'` with `rnd === 'RNDZ'`) instead
of swapping string literals would force a wrong dispatch.

### P7: bigint-bump is potent on substrate, weak elsewhere -- 15 applications -- algorithm-locality

Substrate ports take 2/4 clean kills from bigint-bump (50% kill
rate). The bumped literals are constants in mantissa-MSB / limb-
width arithmetic, perturbing them shifts the algorithm one bit.

Arithmetic / conversion / misc all take 0 clean kills. The bumped
literal is usually a precision constant or a magic number used
once (e.g. `2n` in an exponent calculation); the golden cases
either don't exercise the affected branch or absorb the bump in a
later rounding step.

This is consistent with the substrate-heavy mean composite (0.575
on bigint-bump) vs the others (0.889 - 0.985).

### P8: ternary-negate undercounts because of delegation -- intended weakness

Ternary-negate clean-killed 2/8 applications (25%): `mpfr_floor`
(composite=0.53) and `mpfr_sqr_1n` (composite=0.41). On the other
6, the ternary value at the mutation site was already `0`
(special-case branches), and `-(0) === 0`, so the mutation is a
no-op.

The pattern is universal: every MPFR port has multiple
`ternary: 0` returns (NaN, Inf, zero special-cases). The mutator
visits all of them but most reduce to no-ops. Only when a port has
inline computed ternary values does the mutator catch it.

### M1: ternary-negate corrupts destructuring patterns -- 2 fns -- mutator bug

`mpfr_div_2si` and `mpfr_div_2ui` register `module_init_failed`
for ternary-negate. Cause: both have `const { mant, exp, ternary:
tr } = roundMantissa(...)` at line 137 / 199. The mutator rewrites
that to `const { mant, exp, ternary: -(tr) } = roundMantissa(...)`
-- invalid TS, the destructuring rename target cannot be an
expression. Bun rejects the module at parse time.

`mutate.py` correctly detects this as `module_init_failed` and
discards the score; no false data. But the mutator is **applying
where it should not**, wasting a grader run per occurrence (~1
sec). The regex `[{,](\s*)ternary:\s*([^,};\n]+?)(\s*[,};])`
needs an additional exclusion: don't match inside a
`const|let|var|, ` destructuring pattern.

### M2: op-swap rewrites function-name in its own declaration -- 1 fn -- mutator bug

`mpfr_add` registers `module_init_failed` for op-swap. The op-swap
regex `\bmpfr_add\(` matches `export function mpfr_add(` at line
489. The mutation rewrites that to `export function mpfr_sub(`,
turning the file into a same-named-export collision (or in some
cases breaking other imports). Init fails.

Same root cause as M1: the mutator's pattern is too broad. Should
exclude lines that begin with `function`, `export function`, or
that are followed by a `:` (suggesting an interface field /
type). Discarded by `mutate.py`; no false data, but wastes ~1
sec per run.

### M3: comparison-swap targets first match (precondition vs algorithm) -- 1 fn -- mutator gap

`mpn_cmp` (composite=1.0 on comparison-swap): the mutator hits
`i < n` (precondition) instead of `a > b` (the actual comparison
in the algorithmic loop). The first-match policy means
boundary-check sites win over algorithmic sites whenever both
exist.

This is a generalization of P3. A variant that emits N mutants,
one per applicable comparison, would catch this -- at the cost of
N x grader runs per fixture.

## Recommendations

Each recommendation traces to specific patterns in the data above.

1. **Keep the menu as is for the trial run.** All four classes
   pass the gate on the majority of sampled fixtures (>=67%), and
   shift-direction-swap alone catches 14/16 of its applications.
   The dead mutators (rnd-swap, most of comparison-swap) cost
   <1 sec per fixture and are cheap. The trial gates a port on
   "did *any* mutation drop composite below 0.95"; the
   per-mutation kill rate is a calibration concern, not a
   correctness concern. (Trace: aggregate stats; Pattern P5,
   Pattern P6.)

2. **File P2 bds against the two mutator bugs M1 and M2.** Both
   inflate `module_init_failed` counts and waste grader time. The
   data already excludes them from gate decisions
   (via `_detect_module_init_failed`), so the trial is not
   blocked, but they should be fixed before the Production
   ralph-loop pays them for 500+ ports. (Trace: Patterns M1, M2;
   /tmp/calib_n6.json INIT_FAILED entries.)

3. **File a P3 bd to add a "trivial-body whitelist" to the gate.**
   Ports like `mpfr_swap` (zero applicable mutations) and
   `mpfr_set_inf` (only vacuous mutations apply) should pass the
   mutation-prove gate trivially -- they ARE correct by
   construction, and the gate's current False-on-these is a false
   negative. Options: (a) gate considers it a pass when 0
   mutations apply, (b) gate consults a fixture-level
   "complexity floor" (lines of code, branches, or applicable-
   mutation count), or (c) the spec.json carries a
   `mutation_prove_exempt: true` flag for trivially-correct
   ports. (Trace: Patterns P1, P2.)

4. **File a P3 bd to harden comparison-swap with multi-site
   enumeration.** Pattern M3 / P3 show that the first-match
   policy misses algorithmic comparisons when preconditions
   exist. A variant that emits one mutant per comparison and
   gate-passes when any mutant drops below 0.95 raises the kill
   rate at modest grader-time cost. (Trace: Patterns P3, M3.)

5. **File a P4 bd to consider a "branch-replacement" rnd-swap.**
   Rather than swapping string literals, mutate dispatch sites
   (`rnd === 'RNDN'` -> `rnd === 'RNDZ'`). Current rnd-swap has 0
   kill rate at N=6; while shift-direction-swap covers most
   ports, adding a working rnd mutator would close the
   rounding-correctness gap for ports where shift-swap is N/A.
   (Trace: Pattern P6.)

6. **Per-class gates are NOT recommended.** The N=6 data show no
   class with a <67% gate pass rate. Variant gates (e.g.
   "conversion uses composite <= 0.99 instead of 0.95") would
   over-fit the calibration sample. Stick with the uniform 0.95
   gate; address class-level gaps by extending the menu
   (recommendations 4 + 5), not by relaxing the threshold.

## Sampling caveats

- **N=6 per class = 24 fixtures of 115 done**. Per-class
  gate-pass rates have wide confidence intervals: arithmetic 5/6
  is 53-96% at 95% CI, substrate 5/6 same. At N=20 the per-class
  numbers may shift by +/- 10 points. The patterns at the
  mutation level (rnd-swap dead weight, shift-swap dominant) are
  more robust -- they pool across 16+ applications.
- **Same seed across runs**: N=6 is a strict superset of N=4. The
  N=4 -> N=6 consistency check is "additive samples did not
  contradict the original patterns", not "two independent
  samples gave the same answer." A future calibration with a
  different seed would harden this.
- **Per-fixture failure cases (mpfr_sub, mpfr_get_d_2exp,
  mpfr_set_inf, mpfr_swap, mpfr_get_d1, mpn_cmp) are
  deterministic**: they fail because of structural properties of
  the port (delegation, trivial body, first-match comparison
  policy), not statistical variance. Reproducible at any N.

## Open follow-up bds

Filed during this analysis. All `--type=task`, not claimed.

| bd ID         | pri | title                                                                       |
| ------------- | --- | --------------------------------------------------------------------------- |
| mpfr-ts-agn   | P2  | mutators.ts: ternary-negate must not mutate destructuring patterns          |
| mpfr-ts-omy   | P2  | mutators.ts: op-swap must not mutate function declarations                  |
| mpfr-ts-9di   | P3  | mutate.py: gate must pass trivial-body ports (no applicable mutations)      |
| mpfr-ts-18x   | P3  | mutators.ts: comparison-swap should enumerate all sites, not first-match    |
| mpfr-ts-6zg   | P4  | mutators.ts: add branch-replacement rnd-swap variant                        |

Two P2s gate the Production ralph-loop (they inflate init-failed runs by ~2 sec each across 500+ ports). The two P3s improve gate signal quality but don't block the trial. The P4 is a coverage extension for a follow-on iteration.
