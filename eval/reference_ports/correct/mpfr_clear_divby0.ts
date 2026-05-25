/**
 * reference_ports/correct/mpfr_clear_divby0.ts -- mutation-prove
 * reference for mpfr_clear_divby0.
 *
 * Per CLAUDE.md PIL.3, the calibration baseline. Re-exports the shipped
 * src/internal/mpfr/flags.ts clearFlags primitive composed with the
 * MPFR_FLAGS_DIVBY0 bit. The production src/ops/clear_divby0.ts does
 * not yet exist; the orchestrator will materialise it during the
 * port-and-grade flow.
 *
 * Algorithm (mpfr/src/exceptions.c L168-L174):
 *   new_flags = mask & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0)
 *             = mask & ~MPFR_FLAGS_DIVBY0 (within the 6-bit domain)
 *
 * Bit values (mpfr.h L77-L88): UNDERFLOW=1, OVERFLOW=2, NAN=4,
 * INEXACT=8, ERANGE=16, DIVBY0=32, ALL=63.
 *
 * Ref: mpfr/src/exceptions.c L168-L174 -- C reference.
 * Ref: src/internal/mpfr/flags.ts -- the shipped TS flag register.
 * Ref: eval/functions/mpfr_clear_divby0/spec.json -- contract.
 */

import { MPFRError } from '../../../src/core.ts';

const MPFR_FLAGS_ALL = 63n;
const MPFR_FLAGS_DIVBY0 = 32n;

export function mpfr_clear_divby0(mask: bigint): bigint {
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_clear_divby0: mask must be bigint, got ${typeof mask}`,
    );
  }
  // Mirror the C: __gmpfr_flags &= MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0.
  // Mask the input to MPFR_FLAGS_ALL first (matches setFlags semantics
  // in src/internal/mpfr/flags.ts).
  const inDomain = mask & MPFR_FLAGS_ALL;
  return inDomain & (MPFR_FLAGS_ALL ^ MPFR_FLAGS_DIVBY0);
}
