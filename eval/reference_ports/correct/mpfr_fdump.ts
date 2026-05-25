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
 *           - mantissa: MSB first, EXACTLY prec bits (no stripping)
 *           - exponent: signed decimal of x.exp
 *
 * For normal values: walk bits from position prec-1 down to 0, emitting
 * each. The C source emits exactly prec bits — it does NOT strip trailing
 * zeros (the L82-L92 break only fires after emitting the prec-th bit;
 * trailing zeros within the prec window are preserved).
 */

import type { MPFR } from '../../../src/core.ts';

export function mpfr_fdump(x: MPFR): string {
  if (x.kind === 'nan') return '@NaN@\n';
  if (x.kind === 'inf') return (x.sign === -1 ? '-' : '') + '@Inf@\n';
  if (x.kind === 'zero') return (x.sign === -1 ? '-0\n' : '0\n');

  const sign = x.sign === -1 ? '-' : '';
  const prec = Number(x.prec);
  let bits = '';
  for (let i = prec - 1; i >= 0; i--) {
    bits += ((x.mant >> BigInt(i)) & 1n) === 1n ? '1' : '0';
  }
  return `${sign}0.${bits}E${x.exp}\n`;
}
