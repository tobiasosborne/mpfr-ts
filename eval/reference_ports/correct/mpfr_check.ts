/**
 * reference_ports/correct/mpfr_check.ts -- mutation-prove reference.
 *
 * Algorithm: any value that survives the codec's decodeMpfr step is by
 * construction well-formed (the codec calls validate() during decoding).
 * The TS-side mpfr_check thus reduces to a re-validate that returns
 * true on success, false on failure.
 *
 * Mirrors mpfr/src/check.c L32-L80 conceptually:
 *   - sign in {-1, +1}     -> covered by validate()
 *   - prec in valid range  -> covered by validate()
 *   - mantissa MSB-aligned -> covered by validate()
 *   - singular invariants  -> covered by validate()
 */

import type { MPFR } from '../../../src/core.ts';
import { validate } from '../../../src/core.ts';

export function mpfr_check(x: MPFR): boolean {
  try {
    validate(x);
    return true;
  } catch {
    return false;
  }
}
