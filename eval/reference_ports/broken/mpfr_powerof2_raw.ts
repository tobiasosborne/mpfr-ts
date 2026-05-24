/**
 * reference_ports/broken/mpfr_powerof2_raw.ts — deliberately-buggy mpfr_powerof2_raw.
 *
 * **Multi-bug perturbation: (1) invert the bit-pattern check (any
 * mantissa is treated as a power of 2 iff bit `prec-1` IS NOT the
 * only set bit), (2) return `true` for kind === 'inf' (collapses to
 * the same answer as +inf is "in some sense" a limit of powers of 2,
 * but the predicate is supposed to be false), (3) for normal kind
 * return raw2 inverted.**
 *
 * Effect: every MSB-only mantissa returns false (was true); every
 * MSB-plus-other returns true (was false); every Inf returns true
 * (was false). Most happy/edge cases flip. Composite well below 0.45.
 *
 * NOT used in production.
 *
 * Ref: CLAUDE.md PIL.3.
 * Ref: src/internal/mpfr/powerof2_raw.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_powerof2_raw(x: MPFR): boolean {
  // BUG 1: return true for Inf (no MSB pattern to inspect).
  if (x.kind === 'inf') return true;
  // BUG 2: return true for Zero (mant === 0n; raw2 would say false).
  if (x.kind === 'zero') return true;
  if (x.kind === 'nan') return false;

  // BUG 3: invert the bit-pattern check. raw2 returns
  // `mant === 1n << (prec - 1n)`; we return the opposite.
  // So every MSB-only mantissa (true case) returns false here.
  const msb = 1n << (x.prec - 1n);
  return x.mant !== msb;
}
