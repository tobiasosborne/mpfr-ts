/**
 * reference_ports/broken/mpfr_get_prec.ts — deliberately-buggy
 * mpfr_get_prec.
 *
 * Used to mutation-prove the golden master per docs/PILOT_PLAN.md
 * Step 8 and CLAUDE.md PIL.3. If this port scores composite > 0.55 on
 * the mpfr_get_prec golden, the golden is too weak.
 *
 * **Deliberately broken: returns x.exp instead of x.prec.** Plausible
 * agent bug — the four bigint fields of an MPFR (`prec`, `exp`,
 * `mant`, `sign`) are visually similar in JSDoc / autocomplete, and a
 * cut-and-paste from a hypothetical `mpfr_get_exp` port would swap the
 * field. The bug is correctness-real because:
 *
 *   - For `normal` MPFR values constructed from doubles like 3.14 at
 *     prec 53, exp ≈ 2 while prec = 53 → wrong.
 *   - For `zero`, `inf`, `nan` the exp is 0n but the prec is the
 *     allocated value → very wrong (returns 0 instead of e.g. 64).
 *   - For PREC_MIN-prec normals the value happens to align (exp=1 for
 *     1.0 at prec 1, prec=1) — so this bug is NOT caught by every case,
 *     but it IS caught by the overwhelming majority.
 *
 * Note: NaN happens to give the right answer for the WRONG reason: the
 * TS NaN sentinel has both prec=0n AND exp=0n, so returning exp gives
 * 0n which matches the TS-expected output of 0n. The broken port is
 * still wrong for NaN by accident; this is fine for mutation-prove
 * purposes because non-NaN cases provide an overwhelming signal.
 *
 * NOT used in production. NOT imported from `src/`. Do NOT fix this
 * file — the bug is the point.
 *
 * Ref: docs/PILOT_PLAN.md Step 8 — mutation-prove acceptance.
 * Ref: CLAUDE.md PIL.3 — composite must drop below 0.55 under mutation.
 * Ref: src/ops/get_prec.ts — the correct version.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_get_prec(x: MPFR): bigint {
  // BUG: should return x.prec. Returns x.exp.
  return x.exp;
}
