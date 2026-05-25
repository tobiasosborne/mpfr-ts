/**
 * reference_ports/broken/mpfr_fdump.ts -- deliberately-buggy.
 *
 * **BUG: omits the trailing newline.** Every case mismatches by one
 * character. The grader uses strict === on strings.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_fdump(x: MPFR): string {
  // BUG: no trailing '\n' on any branch.
  if (x.kind === 'nan') return '@NaN@';
  if (x.kind === 'inf') return (x.sign === -1 ? '-' : '') + '@Inf@';
  if (x.kind === 'zero') return (x.sign === -1 ? '-0' : '0');
  const sign = x.sign === -1 ? '-' : '';
  const prec = Number(x.prec);
  let bits = '';
  for (let i = prec - 1; i >= 0; i--) {
    bits += ((x.mant >> BigInt(i)) & 1n) === 1n ? '1' : '0';
  }
  let end = bits.length;
  while (end > 1 && bits[end - 1] === '0') end--;
  bits = bits.substring(0, end);
  return `${sign}0.${bits}E${x.exp}`;
}
