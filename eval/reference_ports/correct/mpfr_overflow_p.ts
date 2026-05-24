/**
 * reference_ports/correct/mpfr_overflow_p.ts -- re-export of the production port.
 *
 * The port pair is mpfr_overflow_p + (TS-side) flag-state setter. This
 * wrapper signature matches the production port: takes mask (the bitmask
 * the test asked to set first) as a bigint, calls the flag setter, then
 * reads the predicate. See spec.json divergence_from_c for the flag-state
 * API gap that the sonnet porter must address.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import { mpfr_overflow_p } from '../../../src/ops/overflow_p.ts';

export { mpfr_overflow_p };
