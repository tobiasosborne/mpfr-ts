# ADR 0002 — approximation-helper grading policy

> Resolves bd `mpfr-ts-52u`. Surfaced by shadow trial 2
> (`docs/reports/011-shadow-trial-2.md`) when opus parked
> `mpfr_sqrt2_approx` and the orchestrator filed an issue framed as
> "the grader needs inequality-output mode". Investigation revealed
> the architecture already has a cleaner answer; this ADR documents
> it and closes the framing gap.

## Status

Accepted.

## Context

A class of MPFR C helpers — `mpfr_sqrt2_approx`, `mpfr_div2_approx`,
`__gmpfr_invert_limb_approx`, the Newton-seed primitives, internal
transcendental approximants — have **inequality contracts**:

```c
/* {rp, 2} - 4 <= floor(sqrt(2^128*n)) <= {rp, 2} + 26. */
/* q <= floor(u*B^2/v) <= q + 21. */
```

These functions exist *for performance*: they're computed once with a
cheap-but-loose method (LUT-backed inverse, fixed-point Newton step)
and then refined by their caller (`mpfr_sqrt2`, `mpfr_div_q`) via
correction loops. There is no single "correct" output — any value
inside the inequality band satisfies the contract.

The shadow trial 2 framing was: **the grader uses strict `===` on
(value, ternary), so approximation helpers cannot be graded.** Three
options were enumerated in HANDOFF: (a) extend the wire format with
`output_range: [lo, hi]`; (b) park all such helpers via ADR;
(c) always-delegate to a unified public op (the
standalone-wire-form-with-delegate-to-unified-op pattern).

Investigation of the already-shipped `mpfr_div2_approx`
(`composite_correctness=1.0`, 129/129 cases, `r2026-05-24T12:40:42Z`)
revealed a **fourth path the architecture had already adopted**: the
golden_driver-substitute pattern.

### The golden-driver-substitute pattern (already in use)

`mpfr/src/div.c` `mpfr_div2_approx` calls
`__gmpfr_invert_limb_approx`, a LUT-backed inverse defined in
`invert_limb.h` — non-portably-replicable (256-entry x86-ASM-derived
table).

`eval/functions/mpfr_div2_approx/golden_driver.c` does **not** link
against libmpfr's `mpfr_div2_approx`. Instead it re-implements the
algorithm verbatim using a **portable substitute** for
`invert_limb_approx`:

```c
/* Re-derive the approximate inverse using __uint128_t arithmetic.
 * num = (B-1)*B - v;  return (uint64_t)(num / v); */
```

The golden is then emitted from this portable algorithm. The TS port
(`src/internal/mpfr/div2_approx.ts`) mirrors the same portable
algorithm step-by-step. Because both the golden and the TS port
derive from the *same portable substitute*, they match bit-for-bit
under strict equality grading. The inequality contract is honored as
an *invariant of the algorithm*, not as a runtime assertion in the
grader.

This generalizes. For any C helper whose only obstacle to faithful
porting is a non-portable primitive (LUT, ASM, target-FP, intrinsic):

1. `golden_driver.c` replaces the primitive with a portable
   substitute; the rest of the algorithm is mirrored verbatim.
2. The golden is recorded from the substituted algorithm.
3. The TS port mirrors the substituted algorithm.
4. The grader compares with strict equality.

Both the port and the golden are downstream of one algorithm —
they cannot disagree unless one of them has a bug, which is exactly
what the grader is supposed to catch.

### Why `mpfr_sqrt2_approx` is still parked under this ADR

The framing gap surfaced when the parked spec.json for
`mpfr_sqrt2_approx` listed three reasons for parking — and "(2)
inequality contract incompatible with the exact-equality grader"
*sounded* load-bearing, but actually wasn't. The other two reasons
hold without it:

1. **No public-API caller.** The unified `src/ops/sqrt.ts` uses
   bigint `isqrt` across all precisions; `mpfr_sqrt2` (the >64-<128
   window helper that's `sqrt2_approx`'s only C caller) is not
   selected for porting. Porting `sqrt2_approx` standalone produces
   dead code in `src/`.

3. **Raw-limb wire form misaligned with bigint TS surface.** The
   golden-driver-substitute pattern *could* address this — emit
   `{rp1, rp0}` limb pairs in the golden, accept them in the
   `MpfrLimbPair` codec entry. But until reason (1) is resolved,
   doing so just lights up more dead code.

Reasons (1) and (3) jointly justify parking. Reason (2) doesn't.

## Decision

1. **Approximation helpers are faithfully ported and equality-graded
   against a portable-substitute golden_driver.** The
   golden-driver-substitute pattern formalized above is the standard
   approach for any function whose C implementation relies on a
   non-portable primitive.

2. **No inequality / range / tolerance grader mode is implemented.**
   The `output_range: [lo, hi]` wire-format extension from HANDOFF
   option (a) is not built. YAGNI applies (system-prompt rule + Law
   4): no concrete divergent-algorithm port exists today, and the
   substrate discipline pushes new approximation helpers toward
   golden-driver-substitute regardless.

3. **Parking criterion for approximation helpers.** Park if and only
   if one or both of these hold, *independently* of the inequality
   shape:

   - **No public-API caller.** The function exists only to feed a
     C-internal helper that is itself not selected for porting. The
     standalone port would be dead code in `src/`.
   - **Wire-form intractability after substrate carve-out.** The
     function's I/O contract is so specific to MPFR-internal data
     structures that emitting it as a substrate helper (taking the
     raw bigint limbs and producing raw bigint limbs) is more
     complexity than the future caller pays back.

   If neither holds, port faithfully via golden-driver-substitute.

4. **`mpfr_sqrt2_approx` remains parked** under criterion (i): no
   public TS caller. Its spec.json is updated to delete the
   inequality-grader reason and cite this ADR.

5. **Revisit condition for inequality grading.** Inequality grading
   becomes worth building if and only if a port emerges where:

   - The TS algorithm is *deliberately* different from any
     portable C substitute (typically because the substitute is
     itself awkward to express in bigint TS — e.g., it uses
     fixed-point arithmetic that's natural in C but verbose in TS).
   - The port is needed in `src/` (public-API caller exists, or it
     itself is a public-surface function).

   Under those conditions, the wire-format extension would carry its
   weight. Until then, the simpler architecture wins.

## Consequences

### For the porting playbook

- Substrate ports of approximation helpers follow `div2_approx`:
  golden_driver.c picks a portable substitute, TS port mirrors the
  substitute, equality grading throughout.
- Curators / opus-prep do not need to invent inequality bound
  assertions; they need to find or design a portable substitute.
- The "is this function paroxysmal?" decision tree collapses to
  two yes/no checks (criteria above), not three with the inequality
  question as a third axis.

### For the grader

- `eval/harness/runner.ts` `compareOutput` and
  `eval/harness/value_codec.ts` `compareMpfr` are unchanged.
- `eval/harness/ast_check.ts` is unchanged (it didn't validate spec
  shape).
- `eval/driver/gen_spec.py` and `eval/driver/mutate.py` are
  unchanged (neither introspects output shape).
- Zero LOC delta in `eval/` core code; the ADR is the deliverable.

### For state.db

- `mpfr_sqrt2_approx` stays `status=blocked`. The "reason" string
  in the parked spec.json is rewritten to cite this ADR; no row
  movement.
- `mpfr_div2_approx` is unchanged: already `status=done`,
  `composite=1.0`, 129/129 cases. It is the live evidence
  underpinning this ADR.

### For future approximation helpers in the call graph

When the ralph loop picks a new function whose C definition contains
an inequality contract comment (`*_approx`, "approximation", "lies
in", "within ± of true"), opus prep applies this decision tree:

```
1. Is there a public-API caller in the TS port that would route
   here? (Check src/ops/ for a unified op that includes this prec
   window or this algorithm.)
   - NO → park (criterion i). Cite ADR 0002.
   - YES → continue.

2. Is the function's I/O contract expressible as bigint limb
   tuples (substrate surface) without contorting the algorithm?
   - NO → park (criterion ii). Cite ADR 0002.
   - YES → continue.

3. Port via golden-driver-substitute:
   - Write golden_driver.c with a portable substitute for any
     non-portable primitive the C uses (LUT, ASM, intrinsic).
   - Mirror the substituted algorithm verbatim in the TS port.
   - Grade with strict equality.
```

### Open questions deferred to a future ADR

- **Newton-seed transcendental approximants** (`mpfr_digamma_approx`,
  `mpfr_trigamma_approx`, etc.) are not yet selected for porting.
  When they are, decision tree above applies. Most likely outcome:
  parked under criterion (i) until the parent transcendental
  (`mpfr_digamma`, `mpfr_trigamma`) is selected — at which point
  they're inlined into the parent port rather than ported
  standalone.

- **`mpfr_sqrt2` itself** (the >64-<128 prec window helper). If a
  future perf-driven decision selects it for porting, this would
  unblock `mpfr_sqrt2_approx` under criterion (i). Both would be
  ported together via golden-driver-substitute.

## References

- `mpfr/src/div.c` L47-L103 — `mpfr_div2_approx` C source.
- `mpfr/src/sqrt.c` L29-L68 — `mpfr_sqrt2_approx` C source.
- `mpfr/src/invert_limb.h` — `__gmpfr_invert_limb_approx` LUT (the
  non-portable primitive whose substitute drives the pattern).
- `eval/functions/mpfr_div2_approx/golden_driver.c` — the live
  reference for the golden-driver-substitute pattern.
- `src/internal/mpfr/div2_approx.ts` — the live reference for the
  TS port mirroring the substitute.
- `docs/reports/011-shadow-trial-2.md` — shadow-trial run that
  surfaced the framing gap.
- `docs/adr/0001-spec-merge-policy.md` — sibling ADR; same
  shape (small, decision-only, cites live code).
- CLAUDE.md Law 2 — "the harness is the truth"; the
  golden-driver-substitute pattern preserves this without inequality
  grading.
- CLAUDE.md Law 3 — "faithful substrate, idiomatic surface"; the
  substitute pattern is what "faithful" means when the C primitive
  is non-portable.
