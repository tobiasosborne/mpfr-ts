/**
 * ops/init.ts — pure-TS port of MPFR's `mpfr_init`.
 *
 * Public-surface op (class: misc). Imports from the locked schema in
 * `src/core.ts` per CLAUDE.md Law 4 (library coherence).
 *
 * C signature
 * -----------
 *
 *   void mpfr_init(mpfr_ptr x);
 *
 *   - delegates to mpfr_init2(x, __gmpfr_default_fp_bit_precision);
 *   - the default precision is 53 bits (IEEE float64 mantissa).
 *
 *   Ref: mpfr/src/init.c L24–L28 — full C body is a single-line
 *     delegation to mpfr_init2 at the library default precision.
 *
 * Divergence from C — and why
 * ---------------------------
 *
 * The C function mutates an mpfr_ptr x using the globally-settable
 * __gmpfr_default_fp_bit_precision (default 53, changeable via
 * mpfr_set_default_prec). The immutable TS surface has no global
 * precision state — mpfr_set_default_prec is not ported. We hard-code
 * the canonical default of 53n bits and return posZero(53n), matching
 * the same divergence-from-C choice made in src/ops/init2.ts
 * (see §divergence_from_c there).
 *
 *   Ref: mpfr/src/mpfr.h — __gmpfr_default_fp_bit_precision starts at 53.
 *   Ref: src/ops/init2.ts — delegate; same posZero(prec) return contract.
 *   Ref: src/core.ts — MPFR shape, posZero, PREC_MIN/PREC_MAX.
 *   Ref: eval/functions/mpfr_init/spec.json — class:"misc", no params.
 */

import type { MPFR } from "../core.ts";
import { posZero } from "../core.ts";

/**
 * Construct a fresh {@link MPFR} at the library default precision (53 bits).
 *
 * Returns `posZero(53n)` — a deterministic `+0` at 53 bits — as the
 * immutable-surface analog to C's `mpfr_init`. No global precision state
 * is consulted; the default is hard-coded to 53 (IEEE float64).
 *
 * @mpfrName mpfr_init
 *
 * @returns  an `MPFR` value with `kind === 'zero'`, `sign === 1`,
 *           `prec === 53n`, `exp === 0n`, `mant === 0n`.
 *
 * @example
 *   const x = mpfr_init();  // +0 at IEEE float64 precision (53 bits)
 *   x.kind  === 'zero';
 *   x.sign  === 1;
 *   x.prec  === 53n;
 */
export function mpfr_init(): MPFR {
  // Ref: mpfr/src/init.c L24–L28 — C body is:
  //   mpfr_init2(x, __gmpfr_default_fp_bit_precision);
  // where __gmpfr_default_fp_bit_precision == 53 by default.
  // Immutable TS surface: delegate to posZero at the hard-coded default.
  return posZero(53n);
}
