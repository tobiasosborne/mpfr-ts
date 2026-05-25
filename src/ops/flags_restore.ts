/**
 * ops/flags_restore.ts -- pure-TS port of MPFR's `mpfr_flags_restore`.
 *
 * Mask-driven selective replace: bits named by `mask` are copied from
 * `flags`; bits NOT named by `mask` are preserved from `pre`. Both inputs
 * and the result are clamped to `MPFR_FLAGS_ALL` (63n).
 *
 * Algorithm (mpfr/src/exceptions.c L133-L141):
 *
 *   __gmpfr_flags =
 *     (__gmpfr_flags & (MPFR_FLAGS_ALL ^ mask)) | (flags & mask);
 *
 * As with the rest of the flags family in mpfr-ts (see `flags_set.ts`,
 * `flags_clear.ts`) the C side mutates a static global `__gmpfr_flags`;
 * the immutable wire-form here takes the prior register state `pre`
 * explicitly and returns the post-restore state, so the function is a
 * pure expression with no module-level mutation observable across calls.
 * The substrate `setFlags`/`clearFlags` calls cycle the shared register
 * for routine `getFlags`-based reads inside this op, but the cycle is
 * fully encapsulated.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/exceptions.c L133-L141 -- C reference body.
 *   - /usr/include/mpfr.h L77-L88 -- MPFR_FLAGS_* bit constants.
 *   - src/internal/mpfr/flags.ts -- shipped TS flag register.
 *   - src/ops/flags_set.ts -- sister wire-form precedent.
 */

import { MPFRError } from '../core.ts';
import {
  clearFlags,
  setFlags,
  getFlags,
  MPFR_FLAGS_ALL,
} from '../internal/mpfr/flags.ts';

/**
 * Restore the bits named by `mask` from `flags` into `pre`, leaving
 * other bits of `pre` untouched. Returns the resulting register state.
 *
 * @mpfrName mpfr_flags_restore
 *
 * @param pre    Prior flag state. Bits outside MPFR_FLAGS_ALL (63n) are
 *               silently masked off.
 * @param flags  Source of the bits named by `mask`. Bits outside
 *               MPFR_FLAGS_ALL silently ignored.
 * @param mask   Which bit positions to copy from `flags` (vs. preserve
 *               from `pre`). Bits outside MPFR_FLAGS_ALL silently ignored.
 * @returns      `((pre & ~mask) | (flags & mask)) & 63n`.
 */
export function mpfr_flags_restore(
  pre: bigint,
  flags: bigint,
  mask: bigint,
): bigint {
  if (typeof pre !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_flags_restore: pre must be bigint, got ${typeof pre}`,
    );
  }
  if (typeof flags !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_flags_restore: flags must be bigint, got ${typeof flags}`,
    );
  }
  if (typeof mask !== 'bigint') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_flags_restore: mask must be bigint, got ${typeof mask}`,
    );
  }

  // Route through the shared register so substrate stays the canonical
  // source of truth for bit clamping behaviour. The fold-in is the C
  // expression verbatim; the surrounding clear/set encapsulates the
  // module-level register so this call has no externally-visible effect
  // on the next caller of getFlags / setFlags.
  const inPre = pre & MPFR_FLAGS_ALL;
  const inFlags = flags & MPFR_FLAGS_ALL;
  const inMask = mask & MPFR_FLAGS_ALL;
  const restored =
    (inPre & (MPFR_FLAGS_ALL ^ inMask)) | (inFlags & inMask);
  clearFlags();
  setFlags(restored);
  return getFlags();
}
