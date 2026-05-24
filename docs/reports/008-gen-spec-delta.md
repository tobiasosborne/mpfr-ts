# 008 -- gen_spec.py vs curated spec.json: delta analysis across 112 functions

> Step 3 of pre-trial validation. Characterizes how
> `gen_spec.extract_spec` output diverges from the hand-curated
> `eval/functions/<fn>/spec.json` corpus. Read-only analysis; no code
> changes.

## TL;DR

Across 112 callgraph-resolved functions, `gen_spec` matches the curated
spec on **all 5 fields zero times**. 8 functions error outright (5
parenthesized-name macro-overrides like `(mpfr_inf_p) (...)`; 3 missing
non-static decls). A further 18 silently extract a *call site* in a
dispatcher (e.g. `mpfr_add1sp1n`) instead of the static decl. The
remaining 86 land partial: `c_signature` whitespace + identifier-name
drift (100%), `class` semantic-vs-budget disagreement (~60%), `params`
rename + `long int`/`mpz_ptr` TODO-leakage (~77%). Recommendation: file
P2 bds against the 4 distinct `gen_spec` extraction bugs (parens,
static-skip, call-site-grab, unknown-output-pointer types) before step
6 wires gen_spec into the ralph loop. The high `class` /
identifier-name disagreement is **compatible-but-different**, not a
bug; curated `class` encodes the runtime budget tier (per orchestrator
note), and curated identifier names reflect the published-API surface,
not the C source. These should be documented as deliberate divergences
rather than "fixed."

## Method

```bash
python3 eval/driver/validate_specs.py --json > /tmp/spec_deltas.json
```

Then aggregated in Python (no files added to the repo). 4 mpn_*
substrate function dirs are not in `callgraph.json` and were skipped by
the runner (per the warnings in stderr); 112 remain. Field comparison
follows `validate_specs._compare`: `c_signature` is whitespace-collapsed,
all other fields are strict equality.

Compared fields: `class`, `signature.params`, `signature.returns`,
`c_signature`, `prec_unit`.

## Aggregate stats

```
Total entries:                       112
Extraction errors:                     8  (7.1%)
Non-error entries:                   104
Functions matching ALL 5 fields:       0  (0.0%)
```

Per-field disagreement (of 104 non-error entries):

| field              | disagreements |     %   |
|--------------------|---------------|---------|
| c_signature        |    104 / 104  | 100.0%  |
| signature.params   |     80 / 104  |  76.9%  |
| class              |     62 / 104  |  59.6%  |
| signature.returns  |     55 / 104  |  52.9%  |
| prec_unit          |     20 / 104  |  19.2%  |

## Patterns observed

Patterns are grouped by category. Counts are over the 112-function
corpus unless stated otherwise.

### P1: parenthesized-name macro override -- 5 fns -- gen_spec bug

C source like `int (mpfr_inf_p) (mpfr_srcptr x) { ... }` (the parens
around the name override the same-named `MPFR_IS_INF`-style macro).
`gen_spec._find_signature` matches `\b<name>\s*\(` but the `\b` after a
`(` is satisfied -- the match starts at the name itself. The
look-back `flat[start:m.start()]` then includes everything up to the
prior `;` or `}`, which here is `int (`, and the trailing `(` breaks
the decl/call disambiguation. Net result: extraction errors out with
"Could not locate non-static declaration of ...".

Affected: `mpfr_inf_p`, `mpfr_nan_p`, `mpfr_regular_p`, `mpfr_sgn`,
`mpfr_zero_p`.

Category: **gen_spec bug**. The parens-around-name idiom is a standard
GMP/MPFR pattern for macro-overriding identifiers; gen_spec should
handle it.

### P2: static helper -- 3 fns hard-error -- gen_spec bug

`gen_spec._find_signature` explicitly skips heads starting with
`static`. But `callgraph.json` *does* list certain static helpers as
their own functions (the porter is expected to expose them). Result:
no decl found; error.

Affected (hard error): `mpfr_add1sp1` (in `add1sp1_extracted.c`),
`mpfr_div2_approx` (in `div.c`), `mpfr_mul_1` (in `mul_1_extracted.c`).

### P3: static helper -- 18 fns silently wrong -- gen_spec bug

Same root cause as P2, but with a *worse* failure mode: when the static
decl is skipped, the scan continues. Many of these helpers are called
from a sibling dispatcher in the same file (`mpfr_add1`,
`mpfr_sub1sp_n`, `mpfr_mul`, `mpfr_sqr`, `mpfr_div`, etc.). Those call
sites have no `static` keyword in front of them, so the loop accepts
them as decls and harvests the surrounding `if (...)` / `return` text
as the "decl head."

Net result: `c_signature` returns something like
`if (p == GMP_NUMB_BITS) return mpfr_add1sp1n (a, b, c, rnd_mode)`,
`params` becomes a list of `TODO: ...` strings (because there is no
real param list to parse), and `returns` becomes the truncated `TODO:
if (...) return`.

Affected: `mpfr_add1sp1n`, `mpfr_add1sp2`, `mpfr_add1sp2n`,
`mpfr_add1sp3`, `mpfr_sub1sp1n`, `mpfr_sub1sp2`, `mpfr_sub1sp2n`,
`mpfr_sub1sp3`, `mpfr_mul_1n`, `mpfr_mul_2`, `mpfr_sqr_1`,
`mpfr_sqr_1n`, `mpfr_sqr_2`, `mpfr_div_1`, `mpfr_div_1n`,
`mpfr_overflow`, `mpfr_powerof2_raw2`, `mpfr_mpn_cmp_aux`.

Combined P1+P2+P3 = 26 / 112 = 23% of corpus where `c_signature` is
either errored or garbage.

Category: **gen_spec bug**. Same fix as P2 surfaces these: stop
skipping static, and verify the matched head ends in a sane
storage-class/return-type token (not `return`, not `}`, not `?`).

### P4: c_signature -- space-before-paren and identifier renaming -- compatible-but-different

After whitespace normalization, 46 / 104 non-error disagreements differ
*only* in the space between function name and `(` -- gen_spec emits
`mpfr_add (...)` (mirroring the C source's GNU style) while curated
emits `mpfr_add(...)` (no space, matching the `mpfr.h` prototype).

The remaining 58 (of 104) c_signature diffs are dominated by:
identifier renaming (gen_spec lifts the C function-body's identifiers
like `a, b, c`; curated uses the public mpfr.h names like `rop, op1,
op2`), and `long int` vs `long` (no semantic difference). Examples:

```
mpfr_abs:
  gen: int mpfr_abs (mpfr_ptr a, mpfr_srcptr b, mpfr_rnd_t rnd_mode)
  cur: int mpfr_abs(mpfr_t rop, mpfr_srcptr op, mpfr_rnd_t rnd)
mpfr_cmp:
  gen: int mpfr_cmp (mpfr_srcptr b, mpfr_srcptr c)
  cur: int mpfr_cmp(mpfr_srcptr op1, mpfr_srcptr op2)
mpfr_add_si:
  gen: int mpfr_add_si (mpfr_ptr y, mpfr_srcptr x, long int u, ...)
  cur: int mpfr_add_si(mpfr_ptr y, mpfr_srcptr x, long u, ...)
```

Category: **compatible-but-different**. gen_spec extracts the
*definition*; curated mirrors the *prototype* from the public header.
Both are legitimate "C signature" answers. Decision needed: should
gen_spec prefer the header prototype when available, or should curated
adopt the definition-side naming?

### P5: class -- semantic family vs runtime budget tier -- compatible-but-different

62 / 104 disagreements. The pattern splits roughly evenly:

| gen_spec        | curated      | count |
|-----------------|--------------|-------|
| `misc`          | `arithmetic` | 28    |
| `arithmetic`    | `misc`       | 27    |
| `misc`          | `substrate`  | 4     |
| `arithmetic`    | `substrate`  | 3     |

gen_spec uses a filename-stem heuristic: stems starting with
add/sub/mul/div/sqr/sqrt -> `arithmetic`. Curated `class` is the
*runtime budget tier* per the orchestrator note (arithmetic = 50ms or
200ms; misc = 1000ms). The mismatch is by design.

Examples: `mpfr_cmp` curated=arithmetic (50ms tier) but gen=misc;
`mpfr_add_d` curated=misc (1000ms; wraps mpfr_add which is itself
gated) but gen=arithmetic; `mpfr_mpn_cmp_aux` curated=substrate but
gen=arithmetic (filename starts with `cmp_`).

Category: **compatible-but-different**. Decision needed: gen_spec
should either (a) emit no `class` and let the curator fill in, or (b)
accept a `class_hint` from the callgraph / a manifest, or (c) be told
the budget rule (probably needs `mpfr/mp[fn]_` + complexity heuristic;
non-trivial).

### P6: params -- rename only -- compatible-but-different

53 / 80 param diffs are same-length lists where only the names differ
(gen lifts `a, b, c`; curated uses `x, b, c` or `rop, op1, op2`-style
semantics). Same root as P4: extraction site vs published API.

Examples: `mpfr_abs` gen=[b,prec,rnd] cur=[x,prec,rnd];
`mpfr_cmp` gen=[b,c] cur=[a,b].

Category: **compatible-but-different**.

### P7: params -- TODO leakage on uncommon C types -- gen_spec bug (minor)

39 / 80 param diffs contain at least one `TODO: ...` entry. Two
sub-patterns:

(a) `long int u` -> gen emits `TODO: long int u` because `_KNOWN_TYPES`
contains `"long"` but not `"long int"` (the `_map_param` `type_norm`
join uses `" ".join(toks[:-1])` which keeps "long int"). Same for
`unsigned long int`. 5+ affected. Trivially fixable by adding `"long
int"`, `"unsigned long int"`, `"signed long"`, etc., or by normalizing
`long int` -> `long` before lookup.

(b) `mpz_ptr z`, `mpfr_limb_ptr Q1` -> gen emits `TODO: mpz_ptr z`.
The known-types list omits the *non-const* mpz/mpq/mpf/limb ptrs and
exotic pointer types. Affected: `mpfr_get_z`, `mpfr_get_z_2exp` (and
the substrate `mpn_*_aux` family via P3 garbage). Fixable by extending
`_KNOWN_TYPES`.

Category: **gen_spec bug** (low-severity; the TODO is honest, but the
unknown-type list is incomplete).

### P8: returns -- idiomatic TS divergence (boolean / bigint / object wrappers) -- compatible-but-different

55 / 104 returns diffs. The largest sub-patterns:

| gen_spec       | curated      | count | nature                                    |
|----------------|--------------|-------|-------------------------------------------|
| `number`       | `boolean`    |  13   | `_p` predicates: int -> boolean in TS     |
| `TODO: void`   | `MPFR`       |   7   | init/set_inf/etc.: void out-param -> MPFR |
| various TODOs  | `Result`     |  10   | dispatcher call-sites from P3             |
| `TODO: double` | `number`     |   2   | `mpfr_get_d` etc.                         |
| TODO mpfr_*    | `bigint`     |   5   | prec_t / exp_t / si / ui -> bigint        |
| `Result`       | `MPFR`       |   1   | `mpfr_set_exp` (no ternary needed)        |
| various        | object types |   3   | swap, get_d_2exp, print_rnd_mode          |
| `TODO: never`  | `never`      |   1   | `mpfr_abort_prec_max`                     |

The `boolean`-for-predicate, `bigint`-for-prec_t/exp_t, and
object-wrapper patterns are documented `divergence_from_c` choices in
the curated specs (verified for `mpfr_equal_p`, `mpfr_print_rnd_mode`,
`mpfr_get_default_prec`). They are deliberate idiomatic-TS lifts.

Category: mostly **compatible-but-different** (idiomatic-TS choices);
partially **gen_spec bug** for the `TODO: double` / `TODO: long` /
`TODO: mpfr_prec_t` cases where gen_spec can't classify because
`_classify_return` only handles `int` and `mpfr_ptr`. Adding a type
table mapping (`double` -> `number`, `mpfr_prec_t` -> `bigint`, etc.)
removes ~10 TODOs.

### P9: prec_unit -- missing from 16 curated specs -- curated drift

20 / 104 disagreements; 16 are gen_spec=`'bits'` vs curated=`None`
(absent field). The affected functions are predicates and comparators
(`mpfr_cmp*`, `mpfr_*_p`) where there is no `prec` parameter -- so
`prec_unit` is *meaningless*, not missing-by-accident. The other 3
diffs are `mpfr_mpn_*_aux` (curated=`'bits (limb-level)'`) and 1
`mpfr_print_rnd_mode` (curated=`'n/a'`).

Category: **compatible-but-different** for the no-prec-param fns
(curated correctly omits an inapplicable field; gen_spec
unconditionally emits `'bits'`); **curated drift** for the
inconsistent annotation on `mpn_*_aux` (one set says `'bits
(limb-level)'`, another `'n/a'` for `print_rnd_mode`). Low priority.

## Recommendations

1. **Before step 6 wires gen_spec into the ralph loop, fix the four
   structural extraction bugs (P1, P2, P3, P7).** These materially
   misrepresent the C source to a porter agent. P3 in particular is a
   silent-wrong-output failure: agents would receive a "decl" that is
   actually a call site, producing nonsense params/returns/sig and
   no compile-time check would catch it. Specifically:
   - P1: handle `(name) (args)` parenthesized-name decls.
   - P2 + P3: don't skip static decls when callgraph lists them; AND
     verify the head ends in a return-type token (reject `return`,
     `?`, `)`, `}`).
   - P7: extend `_KNOWN_TYPES` with `long int`, `unsigned long int`,
     `signed long`, `mpz_ptr`, `mpf_ptr`, `mpq_ptr`, `mpfr_limb_ptr`,
     `mpfr_exp_t`, and extend `_classify_return`'s scalar mapping for
     `double` -> `number`, `long` -> `bigint`, `mpfr_prec_t` ->
     `bigint`, etc.

2. **Treat `class`, identifier names, and idiomatic-TS return types as
   intentional divergences.** Per the orchestrator note, curated
   `class` is a runtime budget tier; per inspection, curated parameter
   and return types reflect the published TS API surface, not the C
   definition. gen_spec should keep emitting its filename-heuristic
   `class` *as a hint*, but the spec-merge step in step 6 should
   prefer the curator's value for `class`, `signature.params`,
   `signature.returns`, and the identifier names inside
   `c_signature`. A small ADR documenting this would defuse repeated
   re-litigation by future agents.

3. **The auto-generation cost-saving lower bound is real but modest.**
   After fixing P1/P2/P3/P7, gen_spec correctly populates:
   `c_signature` (modulo identifier choice, which curator may
   normalize trivially), `prec_unit` (`'bits'` works for ~85% of
   prec-bearing fns), `refs[0]` (the C-source pointer), and a
   `class_hint`. That eliminates ~5 lines of boilerplate per spec
   stub, but `doc`, `divergence_from_c`, the rest of `refs[]`, and
   semantic identifier names still need curation. For 600 more
   functions, the savings are real (~10-15min of cap-touch per
   function) but the *correctness* gain (machine-extracted C-source
   pointer = always accurate; manual = drifts) is the bigger win.

## Open follow-up bds

Filed during this analysis (P2 = blocks step 6; P3 = curated cleanup
opportunity; P4 = trivial). All filed `--type=task`, *not* claimed.

| bd ID            | pri | title                                                                           |
|------------------|-----|---------------------------------------------------------------------------------|
| mpfr-ts-bu8      | P2  | gen_spec: handle parenthesized-name macro-override decls (mpfr_inf_p style)     |
| mpfr-ts-5s4      | P2  | gen_spec: stop unconditionally skipping static decls when callgraph lists them  |
| mpfr-ts-eqc      | P2  | gen_spec: extend _KNOWN_TYPES and _classify_return scalar mappings              |
| mpfr-ts-n8y      | P3  | ADR: document curated divergences from gen_spec (class, idents, idiomatic-TS)   |
| mpfr-ts-bqq      | P4  | curated drift: prec_unit inconsistent on no-prec-param functions                |
| mpfr-ts-00m      | P4  | curated drift: c_signature space-before-paren and identifier renaming           |

Three P2s gate step 6 (they fix extraction correctness on 26 / 112 =
23% of the corpus). One P3 gates step 6 only insofar as the spec-merge
policy needs to know which fields to prefer. Two P4s are post-step-6
cleanup.
