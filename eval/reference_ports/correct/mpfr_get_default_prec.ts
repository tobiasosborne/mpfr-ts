/**
 * reference_ports/correct/mpfr_get_default_prec.ts — re-export of the production port.
 *
 * The port pair is mpfr_get_default_prec + mpfr_set_default_prec. This
 * wrapper signature matches the production port: takes prev_set (the
 * value the test asked to set first) as a bigint, calls set then get.
 */

export { mpfr_get_default_prec } from '../../../src/internal/mpfr/get_default_prec.ts';
