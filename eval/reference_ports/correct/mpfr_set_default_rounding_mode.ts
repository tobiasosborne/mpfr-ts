/**
 * reference_ports/correct/mpfr_set_default_rounding_mode.ts --
 * mutation-prove reference for mpfr_set_default_rounding_mode.
 *
 * Per CLAUDE.md PIL.3 the calibration baseline. The production
 * src/ops/set_default_rounding_mode.ts does not yet exist.
 *
 * Algorithm (mpfr/src/set_rnd.c L27-L32):
 *   if (rnd >= MPFR_RNDN && rnd < MPFR_RND_MAX)
 *       __gmpfr_default_rounding_mode = rnd;     // else silent no-op
 *
 * Integer codes (this build's mpfr.h L100-L111):
 *   0 RNDN, 1 RNDZ, 2 RNDU, 3 RNDD, 4 RNDA, 5 RNDF; RND_MAX == 6;
 *   RNDNA == -1 (retired). Accepted set is 0..5 (RNDF IS accepted);
 *   rnd < 0 and rnd >= 6 are rejected and leave the prior mode in place.
 *
 * Immutable-API lift (INTEGER-coded): the domain includes invalid codes
 * with no RoundingMode-string spelling, so the port works on the raw
 * integer code. It takes (rnd, prior) and returns the resulting code:
 * rnd if accepted, else prior. `prior` is an explicit input because the
 * no-op branch's result depends on the live mode.
 *
 * Ref: mpfr/src/set_rnd.c L27-L32 -- C reference.
 * Ref: /usr/include/mpfr.h L100-L111 -- mpfr_rnd_t codes; RND_MAX == 6.
 * Ref: eval/functions/mpfr_set_default_rounding_mode/spec.json -- contract.
 */

import { MPFRError } from '../../../src/core.ts';

/** First valid code (MPFR_RNDN). */
const MPFR_RNDN = 0;
/** One past the last valid code (MPFR_RND_MAX): RNDN..RNDF accepted. */
const MPFR_RND_MAX = 6;

export function mpfr_set_default_rounding_mode(
  rnd: number,
  prior: number,
): number {
  if (typeof rnd !== 'number' || !Number.isInteger(rnd)) {
    throw new MPFRError(
      'EROUND',
      `mpfr_set_default_rounding_mode: rnd must be an integer, got ${String(rnd)}`,
    );
  }
  if (typeof prior !== 'number' || !Number.isInteger(prior)) {
    throw new MPFRError(
      'EROUND',
      `mpfr_set_default_rounding_mode: prior must be an integer, got ${String(prior)}`,
    );
  }
  // Mirror the C guard exactly: accept iff RNDN <= rnd < RND_MAX.
  if (rnd >= MPFR_RNDN && rnd < MPFR_RND_MAX) {
    return rnd;
  }
  // Out of range: silent no-op -> the prior mode is preserved.
  return prior;
}
