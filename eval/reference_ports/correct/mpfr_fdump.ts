/**
 * reference_ports/correct/mpfr_fdump.ts -- mutation-prove reference.
 *
 * Format (mpfr/src/dump.c L43-L125):
 *   NaN:    '@NaN@\n'
 *   +Inf:   '@Inf@\n'
 *   -Inf:   '-@Inf@\n'
 *   +Zero:  '0\n'
 *   -Zero:  '-0\n'
 *   Normal: [-]0.{binary mantissa}E{decimal exp}\n
 *           - mantissa: MSB first, '1' or '0', trailing zeros stripped
 *           - exponent: signed decimal of x.exp
 *
 * For normal values: walk the mantissa bits from position prec-1 down
 * to 0, emitting '1' or '0'; stop at the last set bit (or hit position
 * 0). This matches the C 'stop early if trailing bits are all 0'.
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_fdump(x: MPFR): string {
  if (x.kind === 'nan') return '@NaN@\n';
  if (x.kind === 'inf') return (x.sign === -1 ? '-' : '') + '@Inf@\n';
  if (x.kind === 'zero') return (x.sign === -1 ? '-0\n' : '0\n');

  // Normal: walk bits from MSB to LSB; strip trailing zeros.
  const sign = x.sign === -1 ? '-' : '';
  const prec = Number(x.prec);
  let bits = '';
  for (let i = prec - 1; i >= 0; i--) {
    const bit = (x.mant >> BigInt(i)) & 1n;
    bits += bit === 1n ? '1' : '0';
  }
  // Strip trailing zeros.
  let end = bits.length;
  while (end > 1 && bits[end - 1] === '0') end--;
  bits = bits.substring(0, end);
  return `${sign}0.${bits}E${x.exp}\n`;
}
