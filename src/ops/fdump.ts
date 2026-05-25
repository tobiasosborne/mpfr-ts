/**
 * ops/fdump.ts -- pure-TS port of MPFR's `mpfr_fdump`.
 *
 * Debug dump of an MPFR value to a human-readable, mantissa-bit-exact
 * string. The format mirrors what the C reference writes to its FILE*:
 *
 *   NaN:    "@NaN@\n"
 *   +Inf:   "@Inf@\n"
 *   -Inf:   "-@Inf@\n"
 *   +Zero:  "0\n"
 *   -Zero:  "-0\n"
 *   Normal: "[-]0.{binary mantissa}E{decimal exp}\n"
 *
 * For a normal value, the mantissa is emitted MSB-first as EXACTLY `prec`
 * bits -- the C source breaks out of the bit loop only after the
 * `prec`-th bit has been printed, so trailing zeros within the `prec`
 * window are preserved. The exponent is the signed decimal of `x.exp`
 * (MPFR's unbiased convention).
 *
 * The C version's `!!!...!!!` and `[]` diagnostic markers fire only on
 * structurally-invalid inputs (non-MSB-normalised mantissa, trailing-bit
 * garbage beyond `prec`, out-of-range exponent, UBF flag). Inputs that
 * reach this port have already passed `validate` (CLAUDE.md Law 4 / the
 * runner's structural check), so none of those conditions can be true
 * and the diagnostic branches are unreachable. We therefore omit them
 * rather than dead-code them.
 *
 * Refs
 * ----
 *
 *   - mpfr/src/dump.c L43-L125 -- C reference body.
 *   - eval/reference_ports/correct/mpfr_fdump.ts -- mutation-prove ref.
 *   - eval/functions/mpfr_fdump/spec.json -- contract.
 *   - src/core.ts -- locked schema (type-only import).
 */

import type { MPFR } from '../core.ts';

/**
 * Render an MPFR value as the canonical MPFR debug-dump string.
 *
 * @mpfrName mpfr_fdump
 *
 * @divergence The C signature is
 *   `void mpfr_fdump(FILE *stream, mpfr_srcptr x)` -- it writes to a
 *   `FILE*` and returns nothing. TypeScript has no canonical `FILE*`
 *   analogue (and forcing one would entangle this op with a runtime-
 *   specific stream API), so the port returns the dump as a string the
 *   caller can `console.log`, write to a file, or pipe themselves.
 *
 * @param x The value to dump.
 * @returns The dump string, terminated by `'\n'` exactly as the C
 *          reference's final `putc('\n', stream)`.
 *
 * @example
 *   mpfr_fdump({ kind: 'nan', sign: 1, prec: 0n, exp: 0n, mant: 0n });
 *   // "@NaN@\n"
 *   mpfr_fdump({ kind: 'inf', sign: -1, prec: 53n, exp: 0n, mant: 0n });
 *   // "-@Inf@\n"
 */
export function mpfr_fdump(x: MPFR): string {
  // Ref: mpfr/src/dump.c L49-L54 -- singular cases.
  if (x.kind === 'nan') return '@NaN@\n';
  if (x.kind === 'inf') return (x.sign === -1 ? '-' : '') + '@Inf@\n';
  if (x.kind === 'zero') return x.sign === -1 ? '-0\n' : '0\n';

  // Ref: mpfr/src/dump.c L46-L47, L66-L93, L109-L111 -- normal case:
  // optional '-', "0.", exactly prec mantissa bits MSB-first, "E<exp>".
  const sign = x.sign === -1 ? '-' : '';
  const prec = Number(x.prec);
  let bits = '';
  for (let i = prec - 1; i >= 0; i--) {
    bits += ((x.mant >> BigInt(i)) & 1n) === 1n ? '1' : '0';
  }
  return `${sign}0.${bits}E${x.exp}\n`;
}
