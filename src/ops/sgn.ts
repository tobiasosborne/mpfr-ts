/**
 * ops/sgn.ts — pure-TS port of MPFR's `mpfr_sgn`.
 *
 * Return the sign-of-value of an {@link MPFR}: `-1`, `0`, or `+1`. Like
 * `mpfr_cmp`, this is a pure inspection op (no rounding, no allocation)
 * and returns a plain JS `number` outside the `{value, ternary}` Result
 * wrapper.
 *
 * C signature
 * -----------
 *
 *   int mpfr_sgn(mpfr_srcptr op);
 *
 *   Returns +1 if op > 0, -1 if op < 0, 0 if op == 0. NaN sets the
 *   erange flag and returns 0.
 *
 *   Ref: mpfr/src/sgn.c L24–L39.
 *
 * TS signature
 * ------------
 *
 *   mpfr_sgn(x: MPFR): number;
 *
 *   - takes a single immutable {@link MPFR} from src/core.ts;
 *   - returns a plain JS `number` in `{-1, 0, +1}` for the four
 *     non-NaN kinds;
 *   - **throws** `MPFRError('EDOMAIN', ...)` on NaN — the documented
 *     domain-error divergence (see below).
 *
 * Divergence from C → TS
 * ----------------------
 *
 * MPFR's C surface signals NaN via the erange flag (a silent "invalid"
 * channel callers must remember to test) and returns `0`. The TS port
 * **throws** `MPFRError` with code `'EDOMAIN'` on NaN, matching the
 * convention already established by `mpfr_cmp` (see src/ops/cmp.ts §
 * Divergence). Per CLAUDE.md "Hallucination-risk callouts: NaN ≠ NaN":
 *
 *   > the idiomatic TS port should throw a documented MPFRRangeError,
 *   > never silently return 0.
 *
 * The C source structures the dispatch around `MPFR_IS_SINGULAR`:
 *
 *     if (MPFR_IS_SINGULAR(a)) {
 *       if (MPFR_IS_ZERO(a)) return 0;
 *       if (MPFR_IS_NAN(a))  { MPFR_SET_ERANGEFLAG(); return 0; }
 *       // Remains infinity, handled below.
 *     }
 *     return MPFR_INT_SIGN(a);
 *
 * — i.e. NaN and zero short-circuit; +Inf / -Inf and normal share the
 * same `MPFR_INT_SIGN` extraction. Our locked schema makes the kind
 * discriminant explicit, so we flatten this into a single switch.
 *
 * Algorithm
 * ---------
 *
 *   - kind === 'nan'   → throw EDOMAIN.
 *   - kind === 'zero'  → return 0 (sign is observable but `sgn(±0) == 0`
 *                        in MPFR — see mpfr/src/sgn.c L29–L30).
 *   - kind === 'inf'   → return x.sign (the sign of ±Inf is meaningful).
 *   - kind === 'normal'→ return x.sign.
 *
 * The 'inf' and 'normal' branches collapse to a single `return x.sign`,
 * but we keep the explicit cases for symmetry with `cmp.ts` and to make
 * the dispatch read as enumeration over the four kinds.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sgn.c L24–L39 — the C reference.
 *   - src/ops/cmp.ts — adjacent port establishing the "throw EDOMAIN on
 *     NaN, return plain number" convention.
 *   - src/core.ts — locked `MPFR` value type, `MPFRError` class.
 *   - CLAUDE.md "Hallucination-risk callouts" — NaN ≠ NaN
 *     (throws here, not return 0); signed zero is observable for
 *     arithmetic but NOT for sgn (`sgn(±0) == 0` per the C source).
 */

import type { MPFR } from '../core.ts';
import { MPFRError } from '../core.ts';

/**
 * Return the sign-of-value of `x` as a plain JS `number`.
 *
 * @mpfrName mpfr_sgn
 *
 * @param x   the {@link MPFR} to inspect.
 *
 * @returns   `-1`, `0`, or `+1`:
 *            - `0` for any zero (`±0` are both `0`, despite the sign
 *              being observable in arithmetic);
 *            - `+1` for any positive value (`+Inf` or a positive normal);
 *            - `-1` for any negative value (`-Inf` or a negative normal).
 *
 * @throws {MPFRError} `EDOMAIN` on NaN. The C reference sets the erange
 *                    flag and returns 0; this port surfaces the error
 *                    loudly per CLAUDE.md "Hallucination-risk callouts".
 *
 * @example
 *   sgn(setD(3.14, 53n, 'RNDN').value);    // +1
 *   sgn(setD(-1.5,  53n, 'RNDN').value);   // -1
 *   sgn(posZero(53n));                     // 0
 *   sgn(negZero(53n));                     // 0  — signed zero collapses
 *   sgn(posInf(53n));                      // +1
 *   sgn(negInf(53n));                      // -1
 *   sgn(NAN_VALUE);                        // throws MPFRError('EDOMAIN')
 */
export function mpfr_sgn(x: MPFR): number {
  switch (x.kind) {
    case 'nan':
      throw new MPFRError('EDOMAIN', 'mpfr_sgn: NaN operand');
    case 'zero':
      // mpfr/src/sgn.c L29–L30: MPFR_IS_ZERO → return 0. Signed zero is
      // observable in arithmetic (RNDD breaks +0 + -0 to -0) but
      // sgn(±0) collapses to 0 — different invariant.
      return 0;
    case 'inf':
    case 'normal':
      // Both ±Inf and normal carry a meaningful sign (`{1, -1}`); the C
      // source extracts it via `MPFR_INT_SIGN(a)` uniformly across these
      // two kinds (mpfr/src/sgn.c L38).
      return x.sign;
    default: {
      // Exhaustiveness: the TS `MPFRKind` union narrows to `never` here;
      // the runtime throw covers a runtime-typed caller passing a
      // structurally-malformed MPFR past the type system.
      const _exhaustive: never = x.kind;
      void _exhaustive;
      throw new MPFRError(
        'EDOMAIN',
        `mpfr_sgn: unknown kind ${String((x as { kind?: unknown }).kind)}`,
      );
    }
  }
}
