/**
 * ops/sub1sp.ts -- pure-TS port of MPFR's `mpfr_sub1sp`.
 *
 * Same-precision same-sign subtraction dispatcher. Routes to one of
 * 5 prec-specialised fast paths (mpfr_sub1sp1/1n/2/2n/3, already
 * shipped) based on the operand precision, with a fallback to the
 * general `mpfr_sub` for prec >= 3*GMP_NUMB_BITS (192 bits).
 *
 * C signature
 * -----------
 *
 *   int mpfr_sub1sp(mpfr_ptr a, mpfr_srcptr b, mpfr_srcptr c,
 *                   mpfr_rnd_t rnd_mode);
 *
 *   Preconditions (MPFR_ASSERTD): prec(a) == prec(b) == prec(c),
 *   both b and c are pure FP (kind 'normal'). Same-sign is enforced
 *   by the upstream caller mpfr_sub. The function returns ternary.
 *
 *   Ref: mpfr/src/sub1sp.c L1437-L1492 (dispatcher) and L1494-end
 *   (general case for prec >= 3*GMP_NUMB_BITS).
 *
 * TS signature
 * ------------
 *
 *   mpfr_sub1sp(b: MPFR, c: MPFR, prec: bigint, rnd: RoundingMode): Result
 *
 * Dispatch table (mirrors C L1474-L1491)
 * ---------------------------------------
 *
 *   prec < 64                  -> mpfr_sub1sp1(b, c, rnd)
 *   prec == 64                 -> mpfr_sub1sp1n(b, c, rnd)
 *   64 < prec < 128            -> mpfr_sub1sp2(b, c, rnd)
 *   prec == 128                -> mpfr_sub1sp2n(b, c, rnd)
 *   128 < prec < 192           -> mpfr_sub1sp3(b, c, rnd)
 *   prec >= 192                -> mpfr_sub(b, c, prec, rnd)   (general case)
 *
 * General-case strategy (prec >= 192)
 * -----------------------------------
 *
 * The C `mpfr_sub1sp` falls through to ~500 LOC of limb-walking code
 * (sub1sp.c L1494-end) when none of the fast paths apply. Per the
 * project's [[project-future-bigint-refactor]] memory and the
 * substrate carve-out reasoning in ADR 0002, native TS bigint
 * subtraction (via `mpfr_sub`) IS the correct algorithm at high
 * precision -- the C limb-walking exists for performance, not for
 * a different correctness contract. We delegate accordingly.
 *
 * The choice is safe because mpfr_sub composes via `mpfr_add(a, -b,
 * prec, rnd)` rather than recursing into mpfr_sub1sp; there is no
 * circular dependency. See src/ops/sub.ts L40-L60 for the composition
 * rationale.
 *
 * Same-sign precondition
 * ----------------------
 *
 * The C dispatcher mpfr_sub1sp does NOT itself enforce that b and c
 * share a sign -- that contract is established by its sole upstream
 * caller mpfr_sub (which routes mixed-sign subtractions to add1sp
 * instead). We enforce it here as an MPFRError so a misuse of the
 * raw dispatcher surfaces loudly.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/sub1sp.c L1437-L1492 -- C dispatcher.
 *   - src/ops/sub1sp{1,1n,2,2n,3}.ts -- already-shipped fast paths.
 *   - src/ops/sub.ts -- general-case fallback (no circular dep; see
 *     sub.ts L40-L60 for the composition).
 *   - src/core.ts -- locked schema.
 *   - eval/functions/mpfr_sub1sp/spec.json -- signature contract.
 */

import type { MPFR, Result, RoundingMode } from '../core.ts';
import { MPFRError, PREC_MAX, PREC_MIN } from '../core.ts';
import { mpfr_sub } from './sub.ts';
import { mpfr_sub1sp1 } from './sub1sp1.ts';
import { mpfr_sub1sp1n } from './sub1sp1n.ts';
import { mpfr_sub1sp2 } from './sub1sp2.ts';
import { mpfr_sub1sp2n } from './sub1sp2n.ts';
import { mpfr_sub1sp3 } from './sub1sp3.ts';

const VALID_RND: ReadonlySet<RoundingMode> = new Set<RoundingMode>([
  'RNDN',
  'RNDZ',
  'RNDU',
  'RNDD',
  'RNDA',
]);

function validateArgs(prec: bigint, rnd: RoundingMode): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
  if (!VALID_RND.has(rnd)) {
    throw new MPFRError('EROUND', `unknown rounding mode: ${String(rnd)}`);
  }
}

/**
 * Same-precision same-sign subtraction dispatcher.
 *
 * @mpfrName mpfr_sub1sp
 *
 * @param b    minuend; must be `kind: 'normal'` with `prec === prec`.
 * @param c    subtrahend; must be `kind: 'normal'` with `prec === prec`
 *             and `sign === b.sign`.
 * @param prec output precision in **bits**.
 * @param rnd  one of the five RoundingMode values.
 *
 * @returns `{value, ternary}` -- b - c rounded per rnd.
 *
 * @throws {MPFRError} `EPREC` on bad prec or prec mismatch with b/c;
 *         `EROUND` on unknown rnd; `EDOMAIN` if either operand is
 *         non-normal or signs differ.
 */
export function mpfr_sub1sp(
  b: MPFR,
  c: MPFR,
  prec: bigint,
  rnd: RoundingMode,
): Result {
  validateArgs(prec, rnd);

  // Ref: mpfr/src/sub1sp.c L1465 -- prec(a) == prec(b) == prec(c).
  if (b.prec !== prec) {
    throw new MPFRError(
      'EPREC',
      `mpfr_sub1sp: b.prec (${b.prec}) must equal prec (${prec})`,
    );
  }
  if (c.prec !== prec) {
    throw new MPFRError(
      'EPREC',
      `mpfr_sub1sp: c.prec (${c.prec}) must equal prec (${prec})`,
    );
  }

  // Ref: mpfr/src/sub1sp.c L1466-L1467 -- MPFR_IS_PURE_FP(b), MPFR_IS_PURE_FP(c).
  if (b.kind !== 'normal') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_sub1sp: b must be normal (pure FP), got ${b.kind}`,
    );
  }
  if (c.kind !== 'normal') {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_sub1sp: c must be normal (pure FP), got ${c.kind}`,
    );
  }

  // Same-sign precondition (set by upstream mpfr_sub; mpfr_sub1sp's C
  // body assumes it but doesn't assert. We enforce in TS for safety.)
  if (b.sign !== c.sign) {
    throw new MPFRError(
      'EDOMAIN',
      `mpfr_sub1sp: requires same-sign operands (b.sign=${b.sign}, c.sign=${c.sign}); ` +
        `mixed-sign subtractions route through mpfr_add1sp instead`,
    );
  }

  // Dispatch table -- mirrors mpfr/src/sub1sp.c L1474-L1491.
  if (prec < 64n) {
    return mpfr_sub1sp1(b, c, rnd);
  }
  if (prec === 64n) {
    return mpfr_sub1sp1n(b, c, rnd);
  }
  if (prec < 128n) {
    return mpfr_sub1sp2(b, c, rnd);
  }
  if (prec === 128n) {
    return mpfr_sub1sp2n(b, c, rnd);
  }
  if (prec < 192n) {
    return mpfr_sub1sp3(b, c, rnd);
  }

  // General case (prec >= 192): mpfr/src/sub1sp.c L1494-end is ~500
  // LOC of limb-walking. Delegate to mpfr_sub instead -- it composes
  // via add(a, -b, prec, rnd) (no circular dep on sub1sp; see
  // src/ops/sub.ts L40-L60), uses native BigInt arithmetic, and
  // produces the same correctly-rounded output.
  return mpfr_sub(b, c, prec, rnd);
}
