/**
 * ops/custom_get_kind.ts -- pure-TS port of MPFR's `mpfr_custom_get_kind`.
 *
 * Returns a signed int that combines the kind discriminant and the sign,
 * matching MPFR's `mpfr_kind_t` enum on the C side:
 *
 *   nan     -> 0
 *   inf     -> +/- 1
 *   zero    -> +/- 2
 *   normal  -> +/- 3
 *
 * Ref: mpfr/src/stack_interface.c L91-L103 -- C reference body.
 * Ref: mpfr/src/mpfr.h L287-L292 -- MPFR_NAN_KIND=0, INF=1, ZERO=2, REGULAR=3.
 */

import type { MPFR } from '../core.ts';
import { MPFRError } from '../core.ts';

/**
 * Encode (kind, sign) into a single signed integer per MPFR's
 * `mpfr_kind_t` convention.
 *
 * @mpfrName mpfr_custom_get_kind
 *
 * @param x  Any MPFR value.
 * @returns  Integer in `[-3, 3]`: `0` for NaN; `+/- 1` for `+/- Inf`;
 *           `+/- 2` for `+/- 0`; `+/- 3` for normals.
 */
export function mpfr_custom_get_kind(x: MPFR): number {
  if (x === null || typeof x !== 'object') {
    throw new MPFRError('EDOMAIN', 'mpfr_custom_get_kind: x must be an MPFR object');
  }
  switch (x.kind) {
    case 'nan':
      return 0;
    case 'inf':
      return 1 * x.sign;
    case 'zero':
      return 2 * x.sign;
    case 'normal':
      return 3 * x.sign;
    default:
      throw new MPFRError(
        'EDOMAIN',
        `mpfr_custom_get_kind: unknown kind '${String(x.kind)}'`,
      );
  }
}
