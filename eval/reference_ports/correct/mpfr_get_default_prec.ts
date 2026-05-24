/**
 * reference_ports/correct/mpfr_get_default_prec.ts — re-export of the production port.
 *
 * The port pair is mpfr_get_default_prec + mpfr_set_default_prec. This
 * wrapper signature matches the production port: takes prev_set (the
 * value the test asked to set first) as a bigint, calls set then get.
 */

import { mpfr_get_default_prec as _get, mpfr_set_default_prec as _set } from '../../../src/ops/get_default_prec.ts';

/**
 * For grading: calls mpfr_set_default_prec(prev_set) then
 * mpfr_get_default_prec(). The wire input is prev_set; the wire output
 * is the bigint that get_default_prec returned. This composition is the
 * only stateless way to grade a function whose output depends on a
 * mutable global.
 */
export function mpfr_get_default_prec(prev_set: bigint): bigint {
  _set(prev_set);
  return _get();
}
