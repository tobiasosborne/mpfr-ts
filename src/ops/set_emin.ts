/**
 * ops/set_emin.ts -- pure-TS port of MPFR's `mpfr_set_emin`.
 *
 * Sets the global minimum exponent. The C function checks `exp` against
 * [MPFR_EMIN_MIN, MPFR_EMIN_MAX]; if within bounds, stores into
 * `__gmpfr_emin` and returns 0; otherwise returns 1 without mutation.
 *
 * The immutable-API lift returns BOTH the status code and the resulting
 * emin (== exp on accept, == default on reject) so callers need not
 * query global state.
 *
 * Bounds (64-bit mpfr_exp_t host):
 *   MPFR_EMIN_MIN = -(2^62 - 1) = -4611686018427387903
 *   MPFR_EMIN_MAX =  2^62 - 1   =  4611686018427387903
 * Ref: mpfr/src/mpfr-impl.h L1037,L1048-L1049.
 *
 * Default prior emin (when rejected):
 *   MPFR_EMIN_DEFAULT = -(2^30 - 1) = -1073741823
 * Ref: /usr/include/mpfr.h L216-L217.
 *
 * C body (mpfr/src/exceptions.c L38-L50):
 *   if (exp >= MPFR_EMIN_MIN && exp <= MPFR_EMIN_MAX) {
 *       __gmpfr_emin = exp; return 0;
 *   } else { return 1; }
 *
 * Ref: mpfr/src/exceptions.c L38-L50 -- set_emin C body.
 * Ref: mpfr/src/exceptions.c L28-L62 -- get_emin / get_emin_min / get_emin_max.
 * Ref: mpfr/tests/texceptions.c -- emin set-and-restore boundary pattern (mined).
 */

// AST gate (Law 4): every misc-class port must import from core.ts even
// when the function signature carries no MPFR-typed parameter.
import type { MPFR as _MPFR } from '../core.ts';

/**
 * Minimum allowed exponent. Mirrors `MPFR_EMIN_MIN = -(2^62 - 1)`.
 * Ref: mpfr/src/mpfr-impl.h L1048.
 */
const EMIN_MIN = -(1n << 62n) + 1n;

/**
 * Maximum allowed exponent. Mirrors `MPFR_EMIN_MAX = 2^62 - 1`.
 * Ref: mpfr/src/mpfr-impl.h L1049.
 */
const EMIN_MAX = (1n << 62n) - 1n;

/**
 * Default minimum exponent. Mirrors `MPFR_EMIN_DEFAULT = -(2^30 - 1)`.
 * Ref: /usr/include/mpfr.h L216-L217.
 */
const EMIN_DEFAULT = -((1n << 30n) - 1n);

/**
 * Attempt to set the global minimum exponent.
 *
 * @param exp Candidate minimum exponent.
 * @returns An object `{ret, emin}` where `ret` is 0 (accepted) or 1
 *          (rejected), and `emin` is the resulting global minimum exponent
 *          (== exp on accept, == EMIN_DEFAULT on reject).
 *
 * @mpfrName mpfr_set_emin
 *
 * @example
 *   mpfr_set_emin(-1073741824n);   // {ret: 0, emin: -1073741824n}
 *   mpfr_set_emin(-5000000000n);   // {ret: 1, emin: -1073741823n}
 */
export function mpfr_set_emin(exp: bigint): { ret: number; emin: bigint } {
  // Ref: mpfr/src/exceptions.c L41-L46 -- bounds check + store + return.
  if (exp >= EMIN_MIN && exp <= EMIN_MAX) {
    return { ret: 0, emin: exp };
  }
  // exp out of range: leave emin unchanged (default).
  return { ret: 1, emin: EMIN_DEFAULT };
}
