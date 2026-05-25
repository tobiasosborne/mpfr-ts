/**
 * ops/compound_near_one.ts -- pure-TS port of MPFR's
 * `mpfr_compound_near_one`.
 *
 * Internal helper for `mpfr_compound_si`: when the exact value of
 * `(1+x)^n - 1` is within `1/4 * ulp(1)` of zero, this routine produces
 * the correctly-rounded value of `(1+x)^n` at the target precision by
 * starting from `y = 1` (exact) and conditionally stepping one ulp in the
 * direction determined by the rounding mode and `s`, the sign of
 * `n * log2(1+x)`.
 *
 * Decision table (Ref: mpfr/src/compound.c L31-L54)
 * -------------------------------------------------
 *
 * For `s = +1` (true result is slightly above 1):
 *   RNDN -> y = 1, ternary = -1  (round to nearest -- 1 is closer than 1+ulp)
 *   RNDZ -> y = 1, ternary = -1  (round toward 0)
 *   RNDD -> y = 1, ternary = -1  (round down)
 *   RNDU -> y = 1+ulp, ternary = +1
 *   RNDA -> y = 1+ulp, ternary = +1
 *
 * For `s = -1` (true result is slightly below 1):
 *   RNDN -> y = 1, ternary = +1
 *   RNDU -> y = 1, ternary = +1
 *   RNDA -> y = 1, ternary = +1
 *   RNDZ -> y = 1-ulp, ternary = -1
 *   RNDD -> y = 1-ulp, ternary = -1
 *
 * The "round toward 1" branch collapses these into:
 *   rnd == RNDN
 *   OR (s > 0 AND rnd in {RNDZ, RNDD})
 *   OR (s < 0 AND rnd in {RNDA, RNDU})
 *
 * yielding ternary = -s (the value 1 is on the opposite side from the
 * true result).
 *
 * Otherwise the result steps one ulp:
 *   s > 0  -> y = nextabove(1) = 1 + ulp,   ternary = +1
 *   s < 0  -> y = nextbelow(1) = 1 - 0.5ulp, ternary = -1
 *
 * (For prec >= 2, `nextbelow(1)` is `1 - 2^(-prec)`, half the ulp of 1.
 * The mpfr_nextbelow shipped port handles this exponent-step correctly.)
 *
 * Notes on the port
 * -----------------
 *
 * - The C MPFR_RNDF (faithful rounding) maps to the RNDN branch in the
 *   C reference. The TS locked schema (src/core.ts) does not expose
 *   RNDF, so the port simply does not enumerate it. The golden_driver
 *   skips RNDF test cases.
 *
 * - Delegates to the shipped `mpfr_nextabove` / `mpfr_nextbelow` for the
 *   ulp-step branches -- per Law 4, ports should compose. Both
 *   delegates have composite=1.0 in state.db (worklog 017).
 *
 * - Builds the literal value `1` at the requested precision as a
 *   `kind=normal` MPFR with `sign=+1, mant = 2^(prec-1), exp = 1`. The
 *   `validate` invariants are: MSB set at position `prec-1` (true --
 *   `mant = 2^(prec-1)` has exactly that bit), and `mant < 2^prec`
 *   (true). The encoded numeric value is
 *   `1 * 2^(prec-1) * 2^(1-prec) = 1`.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/compound.c L29-L54 -- C reference body.
 *   - mpfr/src/compound.c L57+ -- `mpfr_compound_si` (only caller).
 *   - src/ops/nextabove.ts -- shipped, used for RNDA/RNDU step (s > 0).
 *   - src/ops/nextbelow.ts -- shipped, used for RNDZ/RNDD step (s < 0).
 *   - eval/functions/mpfr_compound_near_one/spec.json -- contract.
 *   - src/core.ts -- locked schema (MPFR, Result, RoundingMode, Ternary,
 *     MPFRError).
 *   - CLAUDE.md "Hallucination-risk callouts" / "Rounding mode count is
 *     FIVE" -- TS surface has no RNDF.
 */

import type { MPFR, Result, RoundingMode, Ternary } from '../core.ts';
import { MPFRError } from '../core.ts';
import { mpfr_nextabove } from './nextabove.ts';
import { mpfr_nextbelow } from './nextbelow.ts';

/**
 * The five rounding modes supported by the locked schema. Used for the
 * input-validation reject when the caller passes an unknown string.
 */
const VALID_RNDS: readonly RoundingMode[] = [
  'RNDN',
  'RNDZ',
  'RNDU',
  'RNDD',
  'RNDA',
];

/**
 * Construct the MPFR representation of the value `1` at the given
 * precision. `kind=normal, sign=+1, mant=2^(prec-1), exp=1`. The numeric
 * value is `sign * mant * 2^(exp - prec) = 1 * 2^(prec-1) * 2^(1-prec)
 * = 1`. Mantissa has the MSB set (position `prec-1`) and no bit at
 * position `>= prec`, so it passes `validate`.
 */
function buildOne(prec: bigint): MPFR {
  return {
    kind: 'normal',
    sign: 1,
    prec,
    exp: 1n,
    mant: 1n << (prec - 1n),
  };
}

/**
 * Compute the correctly-rounded value of `(1+x)^n` near 1, given the
 * sign `s` of `n * log2(1+x)` and the rounding mode.
 *
 * @mpfrName mpfr_compound_near_one
 *
 * @param prec Target precision in bits. `>= 1`.
 * @param s    Sign of `n * log2(1+x)` (the direction the true result
 *             deviates from 1). Must be `+1` or `-1`.
 * @param rnd  Rounding mode (one of the five from {@link RoundingMode}).
 * @returns    `Result` with `value` = either `1` exact, `1 + ulp`, or
 *             `1 - 0.5*ulp` (at `prec >= 2`), and `ternary` per the C
 *             contract (sign of rounded - exact).
 *
 * @throws {MPFRError} `EPREC` if `prec` is not a bigint or `< 1`.
 * @throws {MPFRError} `EDOMAIN` if `s` is not `+1` or `-1`.
 * @throws {MPFRError} `EROUND` if `rnd` is not one of the five modes.
 *
 * @example
 *   // s = +1, RNDN -> y = 1, ternary = -1 (true result was slightly above)
 *   mpfr_compound_near_one(53n, +1, 'RNDN');
 *
 *   // s = +1, RNDU -> y = 1 + ulp, ternary = +1
 *   mpfr_compound_near_one(53n, +1, 'RNDU');
 *
 *   // s = -1, RNDD -> y = 1 - 0.5ulp, ternary = -1
 *   mpfr_compound_near_one(53n, -1, 'RNDD');
 */
export function mpfr_compound_near_one(
  prec: bigint,
  s: number,
  rnd: RoundingMode,
): Result {
  // Entry-point validation (Rule 1 -- fail fast).
  if (typeof prec !== 'bigint') {
    throw new MPFRError(
      'EPREC',
      `mpfr_compound_near_one: prec must be bigint, got ${typeof prec}`,
    );
  }
  if (prec < 1n) {
    throw new MPFRError(
      'EPREC',
      `mpfr_compound_near_one: prec must be >= 1, got ${prec}`,
    );
  }
  if (s !== +1 && s !== -1) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_compound_near_one: s must be +1 or -1, got ${s}`,
    );
  }
  if (!VALID_RNDS.includes(rnd)) {
    throw new MPFRError(
      'EROUND',
      `mpfr_compound_near_one: unknown rounding mode ${String(rnd)}`,
    );
  }

  // Start from y = 1 (exact at any precision). C calls mpfr_set_ui(y, 1).
  // Ref: mpfr/src/compound.c L34.
  const one = buildOne(prec);

  // "Round toward 1" branch: y stays at 1, ternary is -s.
  // Ref: mpfr/src/compound.c L35-L41.
  if (
    rnd === 'RNDN' ||
    (s > 0 && (rnd === 'RNDZ' || rnd === 'RNDD')) ||
    (s < 0 && (rnd === 'RNDA' || rnd === 'RNDU'))
  ) {
    return { value: one, ternary: (-s) as Ternary };
  }

  // s > 0 with RNDA or RNDU: step toward +Inf, ternary = +1.
  // Ref: mpfr/src/compound.c L42-L46.
  if (s > 0) {
    return { value: mpfr_nextabove(one), ternary: 1 };
  }

  // s < 0 with RNDZ or RNDD: step toward 0, ternary = -1.
  // Ref: mpfr/src/compound.c L48-L52.
  return { value: mpfr_nextbelow(one), ternary: -1 };
}
