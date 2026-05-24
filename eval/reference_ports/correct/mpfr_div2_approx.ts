/**
 * reference_ports/correct/mpfr_div2_approx.ts — re-export of the production port.
 */

import { mpfr_div2_approx as _impl } from '../../../src/ops/div2_approx.ts';

export function mpfr_div2_approx(
  u1: bigint, u0: bigint, v1: bigint, v0: bigint,
): { Q1: bigint; Q0: bigint } {
  return _impl(u1, u0, v1, v0);
}
