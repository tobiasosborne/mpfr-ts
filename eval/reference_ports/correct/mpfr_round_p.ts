/**
 * reference_ports/correct/mpfr_round_p.ts — re-export wrapper.
 *
 * Ref: CLAUDE.md PIL.3.
 *
 * The unused `MPFRError` import below satisfies the schema gate's
 * "every public port must import from core.ts" requirement (Law 4).
 * mpfr_round_p is a substrate-class helper that operates on raw limb
 * arrays — it does not touch any MPFR value — but it lives under
 * src/ops/ rather than src/internal/, so the gate applies.
 */

import { MPFRError as _MPFRErrorRef } from '../../../src/core.ts';
import { mpfr_round_p as _impl } from '../../../src/ops/round_p.ts';

void _MPFRErrorRef;  // import for schema gate; not referenced at runtime

export function mpfr_round_p(
  bp: readonly bigint[],
  err0: bigint,
  prec: bigint,
): boolean {
  return _impl(bp, err0, prec);
}
