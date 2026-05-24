/**
 * reference_ports/correct/mpfr_nanflag_p.ts -- re-export of the production port.
 *
 * The port pair is mpfr_nanflag_p + (TS-side) flag-state setter. This
 * wrapper signature matches the production port: takes mask (the bitmask
 * the test asked to set first) as a bigint, calls the flag setter, then
 * reads the predicate. Distinct from mpfr_nan_p (value predicate) in
 * src/ops/nan_p.ts -- the sonnet porter must not collapse them.
 *
 * See spec.json divergence_from_c for the flag-state API gap.
 *
 * Ref: CLAUDE.md PIL.3.
 */

import { mpfr_nanflag_p } from '../../../src/ops/nanflag_p.ts';

export { mpfr_nanflag_p };
