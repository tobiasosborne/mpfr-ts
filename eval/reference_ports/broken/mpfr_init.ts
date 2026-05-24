/**
 * reference_ports/broken/mpfr_init.ts — deliberately-buggy mpfr_init.
 *
 * **Deliberately broken: uses wrong default precision.** Returns
 * posZero(64n) instead of posZero(53n). Every single case fails on the
 * `prec mismatch` branch of compareMpfr in value_codec.ts (since the
 * golden always has prec=53). This is a plausible agent bug: a TS
 * programmer may pick `64` thinking "machine word" or "long double"
 * without consulting MPFR's compile-time default.
 *
 * Mutation-prove acceptance per CLAUDE.md PIL.3: composite ≈ 0
 * (because every case mismatches).
 *
 * NOT used in production.
 *
 * Ref: src/ops/init.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';
import { posZero } from '../../../src/core.ts';

export function mpfr_init(): MPFR {
  // BUG: wrong default precision. Should be 53n.
  return posZero(64n);
}
