# ADR 0001 — spec-merge policy for gen_spec/curator integration

> Resolves bd `mpfr-ts-n8y`. Pre-work for step 6 of the opus-prep
> automation effort (worklog 008). Documents per-field precedence in
> the spec-merge logic that ralph.py's `_render_prep_prompt` will use
> once gen_spec.extract_spec output is included in opus prep dispatches.

## Status

Accepted.

## Context

`eval/driver/gen_spec.py` extracts structural spec.json fields from a
C source file. Report 008 (`docs/reports/008-gen-spec-delta.md`)
characterized how gen_spec's output diverges from the hand-curated
spec.json files we already have for 115+ functions:

After fixing the 3 P2 extraction bugs (commit `d8d83bb`), the residual
disagreements split into two distinct categories:

1. **gen_spec is more authoritative**: where curated specs have
   drifted from the C source (e.g. missing `prec_unit` on predicate
   functions; inconsistent `c_signature` whitespace).
2. **Curator is more authoritative**: where curated specs encode a
   different concept under the same field name (e.g. `class` as
   runtime budget tier rather than semantic family; `signature.returns`
   as idiomatic TS shape rather than literal C type).

The integration into ralph.py's prep prompt requires a per-field
precedence policy so opus knows which fields to accept from the
gen_spec scaffold and which to override.

## Decision

For each spec.json field, the integration uses this precedence:

| Field | Winner | Notes |
|---|---|---|
| `function` | (input) | Always the function name passed to gen_spec; no choice |
| `class` | **Curator** | gen_spec emits a filename-stem heuristic (`add* → arithmetic`); curated `class` encodes the runtime budget tier (`misc=1000ms`, `arithmetic=200ms`, `substrate=fast`). The budget tier is what `runner.ts` uses for per-test timeouts. gen_spec's value is a hint; curator must override |
| `signature.params` | **Curator** | gen_spec lifts literal C declaration names (`b, c`); curator uses semantic names matching the published TS API (`a, b` for compare ops; `rop, op1, op2` for `mpfr_*` ops where the C reference uses those). gen_spec's list is a starting draft; curator renames to match the public surface |
| `signature.returns` | **Curator** | gen_spec maps C types to TS types using `_classify_return`'s scalar table (`int → Result` / `number`; `double → number`; `mpfr_prec_t → bigint`). Curator lifts to *idiomatic* TS shape: `_p` predicates return `boolean` (not `number`); some ops return object wrappers (e.g. `mpfr_get_d_2exp` returns `{value: number, exp: bigint}`). gen_spec is correct for the literal mapping; curator overrides for the idiomatic lift |
| `c_signature` | **gen_spec** | Always the most-accurate machine-extracted view of the C definition. Whitespace and identifier-name differences with the published `mpfr.h` prototype are tolerated — both are valid views of the same underlying C function. Curated drift on this field is not worth normalizing |
| `prec_unit` | **gen_spec (default), curator (override)** | gen_spec unconditionally emits `"bits"`. For functions without a `prec` parameter (predicates, comparators), `"bits"` is meaningless but harmless. Curator may explicitly set `"n/a"` or omit the field if precision is inapplicable |
| `doc` | **Curator** | gen_spec emits `"TODO: opus fills this in"`. Prose summary of what the function does + how the TS port mirrors / diverges from the C reference. This is the cognitive content opus contributes |
| `divergence_from_c` | **Curator** | Same shape as `doc`. Enumerates idiomatic-TS lifts, API-shape differences, error-handling divergences |
| `refs` | **gen_spec (first ref), curator (additional)** | gen_spec emits 3 generic refs: `mpfr/src/<basename>.c`, `src/core.ts`, `CLAUDE.md hallucination callouts`. Curator extends with function-specific refs (`mpfr/tests/t<short>.c` if mined; specific manual sections; load-bearing delegate ports) |

## Consequences

### For ralph.py's `_render_prep_prompt` (step 6 of the automation arc)

The prompt template becomes thinner. Currently opus generates the
entire spec.json from scratch (~22 lines per function). After
integration, the prompt includes a pre-filled scaffold and the
instructions narrow to:

```
The structural fields below are extracted from the C source by
gen_spec. Review them; you may override class, signature.params,
signature.returns, and prec_unit. Always replace the doc and
divergence_from_c TODO placeholders with real prose. Add function-
specific refs as needed.

<scaffold>
{
  "function": "<fn>",
  "class": "<gen_spec_inference>",       // override to budget tier
  "signature": {
    "params": [<C names>],               // override to public API names
    "returns": "<C-mapped>"              // override to idiomatic TS
  },
  "c_signature": "<extracted>",          // authoritative; do not edit
  "prec_unit": "bits",                   // override only if inapplicable
  "doc": "TODO: opus fills this in",
  "divergence_from_c": "TODO: opus fills this in",
  "refs": [<3 generic>]                  // extend with function-specific
}
</scaffold>
```

Estimated opus savings: ~5-8 lines of pure boilerplate per function;
~30-60 seconds of opus thinking time per function. For a 30-function
mega-batch this is ~25 min / ~150K opus tokens. Modest but real.

The bigger benefit is correctness: `c_signature` is always machine-
extracted (no drift); `refs[0]` always points to the canonical C
source file (no manual transcription error).

### For the curated spec corpus

The 80 curated specs currently missing `prec_unit` are not a problem
under this policy — both `null` and `"bits"` are accepted. The 46
specs with whitespace-different `c_signature` aren't a problem
either; the curator's value is allowed to differ from gen_spec's.
These are filed as P4 normalization bds (`mpfr-ts-bqq`, `mpfr-ts-00m`)
but are not blockers.

### For mutate.py and the mutation-prove gate

This ADR doesn't affect mutate.py. The mutation-prove gate operates
on the production `src/ops/<fn>.ts` port, not on the spec.json file.

### For shadow-mode trial (per worklog 008)

Before landing step 6 (the actual ralph.py wiring), a shadow-mode
trial against a real mega-batch is recommended. In shadow mode:

1. Pick a 30-function mega-batch normally.
2. Run opus prep with the existing workflow (no ralph.py changes).
3. For each function, run `gen_spec.extract_spec` and diff against
   opus's emitted spec.json using the precedence rules above:
   - If gen_spec wins (`c_signature`, `prec_unit` default) and opus's
     value differs in a non-cosmetic way, that's a finding worth
     investigating
   - If curator wins (`class`, `signature.params`, `signature.returns`,
     `doc`, `divergence_from_c`, `refs`) and opus's value is what we
     expect, the integration works

Shadow mode is read-only — it doesn't change opus's workflow. It just
measures what the integration WOULD have produced if it were live.

### For future schema changes

If a new spec.json field is added later, it gets a row in the table
above. The default is **gen_spec wins for mechanical/extractable
fields; curator wins for prose/idiomatic-TS fields**. ADRs supersede
themselves; an updated ADR bumps the number (0002, etc.).

## Alternatives considered

### A: Curator always wins

Simplest policy: gen_spec output is a hint, curator overrides
everything. Rejected: this loses the correctness benefit on
`c_signature` (curator drift would be tolerated when it shouldn't be).

### B: gen_spec always wins

Cleanest in the prompt: opus only fills in prose. Rejected:
`class` and idiomatic-TS lifts are real, valuable curator
contributions that a literal-C extractor can't produce.

### C: Per-class precedence policy

E.g. "for transcendentals, curator always wins on `signature.returns`
because of the prec-extension wrapper". Rejected as premature — the
current 115-port corpus doesn't yet include transcendentals; revisit
when we have data.

## References

- `docs/reports/008-gen-spec-delta.md` — the analysis that surfaced the
  patterns this ADR codifies
- `docs/worklog/008-automation-infra.md` — context for the broader
  automation effort
- `eval/driver/gen_spec.py` — the script whose output this ADR scopes
- `eval/driver/ralph.py` `_render_prep_prompt` — the function that
  will consume gen_spec output (step 6, not yet implemented)
- bd `mpfr-ts-n8y` — closes with this ADR
