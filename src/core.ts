/**
 * mpfr-ts — locked schema.
 *
 * This module is the single source of truth for the value model, rounding
 * modes, result shape, and error class used by every public function in the
 * library. Per CLAUDE.md Law 4 ("The library composes"), no port may
 * redeclare these types; the grader rejects schema violations.
 *
 * Bumps to this file are versioned changes that require an ADR under
 * `docs/adr/` and a full library re-grade. Treat it as frozen.
 *
 * ## Value model
 *
 * MPFR represents an extended real number as one of four kinds: a normal
 * (finite, nonzero, MSB-normalised) value, a signed zero, a signed infinity,
 * or NaN. The TypeScript surface preserves all four — in particular signed
 * zero is observable (RNDD vs RNDN of `+0 + -0` differ in sign).
 *
 * The C reference encodes this in `__mpfr_struct`:
 *
 *     typedef struct {
 *       mpfr_prec_t  _mpfr_prec;
 *       mpfr_sign_t  _mpfr_sign;
 *       mpfr_exp_t   _mpfr_exp;
 *       mp_limb_t   *_mpfr_d;
 *     } __mpfr_struct;
 *
 * Ref: mpfr/src/mpfr.h L247–L253 — main struct layout.
 *
 * Kind is encoded in the C side via sentinel exponent values
 * (`__MPFR_EXP_NAN`, `__MPFR_EXP_INF`, `__MPFR_EXP_ZERO`); the TypeScript
 * surface promotes that to an explicit discriminant `kind` so callers
 * pattern-match cleanly without comparing magic exponents.
 *
 * Ref: mpfr/src/mpfr.h L242–L245 — sentinel exponents `__MPFR_EXP_NAN`,
 *   `__MPFR_EXP_ZERO`, `__MPFR_EXP_INF`.
 *
 * The value represented by a `normal` MPFR is, in MPFR's own words:
 *
 *     _sign * (_d[k-1]/B + _d[k-2]/B^2 + ... + _d[0]/B^k) * 2^_exp
 *
 * where `k = ceil(_prec / GMP_NUMB_BITS)` and `B = 2^GMP_NUMB_BITS`, with
 * the normalisation invariant that the MSB of `_d[k-1]` is set whenever the
 * number is non-singular, and the trailing `k*GMP_NUMB_BITS - _prec` bits
 * of `_d[0]` are zero.
 *
 * Ref: mpfr/src/mpfr.h L263–L272 — value formula and MSB-normalisation rule.
 *
 * The TypeScript surface folds the limb array into a single `bigint`
 * `mant`, MSB-aligned to `prec` bits exactly (no trailing zero limb-padding
 * to worry about). The value of a `normal` MPFR is therefore:
 *
 *     sign * mant * 2^(exp - prec)
 *
 * with the invariants `mant >= 2^(prec-1)` and `mant < 2^prec` — that is,
 * the bit at position `prec-1` of `mant` is set, and no bit at position
 * `>= prec` is set. `exp` is the unbiased base-2 exponent of the value in
 * the same convention MPFR uses externally (so the value's magnitude lies
 * in `[2^(exp-1), 2^exp)`).
 *
 * `validate(x)` enforces all of this structurally; ported functions are
 * expected to return values that pass `validate` without modification.
 */

/**
 * Discriminant for {@link MPFR}. `'normal'` covers every finite non-zero
 * representable value; `'zero'`, `'inf'`, and `'nan'` are the three classes
 * of singular value MPFR carries explicitly.
 *
 * Ref: mpfr/src/mpfr.h L287–L292 — `mpfr_kind_t` enumeration in the
 *   custom-interface API uses the same four-way split.
 */
export type MPFRKind = 'normal' | 'zero' | 'inf' | 'nan';

/**
 * Sign of an {@link MPFR} value.
 *
 * - For `kind === 'normal'`: the sign of the value (necessarily nonzero).
 * - For `kind === 'zero'`: distinguishes `+0` and `-0`. This is **observable**
 *   in MPFR — e.g. `add(+0, -0, p, 'RNDN') → +0` but
 *   `add(+0, -0, p, 'RNDD') → -0` — so ports must preserve it.
 * - For `kind === 'inf'`: distinguishes `+inf` and `-inf`.
 * - For `kind === 'nan'`: by convention `1`. NaN has no meaningful sign in
 *   the MPFR data model; we fix it to `1` so `MPFR` values are structurally
 *   comparable. Do not rely on this for semantics.
 *
 * Ref: mpfr/src/mpfr.h L195–L196, L284 — `mpfr_sign_t` is `int` and
 *   `MPFR_SIGN(x)` reads `_mpfr_sign` directly.
 */
export type Sign = 1 | -1;

/**
 * A finite or singular MPFR value. Immutable.
 *
 * See the module-level docstring for the full value model. In short:
 *
 * - `kind === 'normal'`: value is `sign * mant * 2^(exp - prec)` with
 *   `prec >= 1`, `2^(prec-1) <= mant < 2^prec`.
 * - `kind === 'zero'`: value is `±0` per `sign`; `prec >= 1`,
 *   `exp === 0n`, `mant === 0n`.
 * - `kind === 'inf'`: value is `±∞` per `sign`; `prec >= 1`,
 *   `exp === 0n`, `mant === 0n`.
 * - `kind === 'nan'`: value is NaN; `sign === 1`, `prec === 0n`,
 *   `exp === 0n`, `mant === 0n`. The `prec === 0n` sentinel for NaN is a
 *   TypeScript-side convention (MPFR's C side carries the originating
 *   precision; here a `nan` has no well-defined precision since no
 *   subsequent op preserves it).
 *
 * Constructed via {@link posInf}, {@link negInf}, {@link posZero},
 * {@link negZero}, {@link NAN_VALUE}, or by ports producing normal values.
 * Validate with {@link validate}.
 */
export interface MPFR {
  readonly kind: MPFRKind;
  readonly sign: Sign;
  /** Precision in **bits** (never decimal digits). `>= 1` for non-NaN; `0n` for NaN. */
  readonly prec: bigint;
  /**
   * Unbiased base-2 exponent. Meaningful only when `kind === 'normal'`,
   * in which case `|value|` lies in `[2^(exp-1), 2^exp)`. `0n` by
   * convention for `zero`, `inf`, `nan`.
   */
  readonly exp: bigint;
  /**
   * Mantissa as an unsigned integer, MSB-aligned to `prec` bits. When
   * `kind === 'normal'`: bit `prec - 1` of `mant` is set and bits at
   * position `>= prec` are clear (`2^(prec-1) <= mant < 2^prec`). `0n` by
   * convention for `zero`, `inf`, `nan`.
   *
   * Ref: mpfr/src/mpfr.h L268–L272 — MSB-normalisation requirement
   *   (`_d[k-1] >= B/2` for non-singular values; trailing
   *   `k*GMP_NUMB_BITS - _prec` bits are zero).
   */
  readonly mant: bigint;
}

/**
 * MPFR rounding modes. Five — `MPFR_RNDF` (faithful) is unsupported in this
 * port (it requires a different correctness contract from the grader);
 * `MPFR_RNDNA` was retired in current MPFR.
 *
 * - `RNDN`: round to nearest, ties to even (IEEE 754 `roundTiesToEven`).
 * - `RNDZ`: round toward zero (truncate).
 * - `RNDU`: round toward `+∞`.
 * - `RNDD`: round toward `-∞`.
 * - `RNDA`: round away from zero.
 *
 * Ref: mpfr/src/mpfr.h L105–L125 — `mpfr_rnd_t` enum. The `DON'T USE
 *   MPFR_RNDNA!` warning at L105 is why we drop it.
 */
export type RoundingMode = 'RNDN' | 'RNDZ' | 'RNDU' | 'RNDD' | 'RNDA';

/**
 * Ternary flag: the sign of `(rounded - exact)`.
 *
 * - `0`: the returned value is exactly the mathematical result.
 * - `+1`: the returned value is strictly greater than the exact result.
 * - `-1`: the returned value is strictly less than the exact result.
 *
 * Direction matters: do **not** compute `sign(exact - rounded)`. The C
 * reference is authoritative.
 *
 * Ref: mpfr/doc/mpfr.texi — chapter "Rounding". Also CLAUDE.md
 *   "Hallucination-risk callouts" / "Ternary flag is the sign of
 *   (rounded - exact), not 0/1."
 */
export type Ternary = -1 | 0 | 1;

/**
 * Standard return shape for every public op in the library. The pair is
 * (idiomatic value, ternary classification) — never a mutated rop handle.
 */
export interface Result {
  readonly value: MPFR;
  readonly ternary: Ternary;
}

/**
 * Discriminant for {@link MPFRError}.
 *
 * - `EPREC`: bad precision argument (`prec < 1`, or non-bigint at runtime
 *   in untyped callers, or `prec > PREC_MAX`).
 * - `EROUND`: unknown rounding mode string (i.e. not in {@link RoundingMode}).
 * - `EDOMAIN`: a NaN input reached a function documented as a domain error
 *   (rare; the default MPFR behaviour is to propagate NaN, not throw).
 */
export type MPFRErrorCode = 'EPREC' | 'EROUND' | 'EDOMAIN';

/**
 * Thrown only for malformed **input** to a public op. Routine MPFR
 * behaviour (overflow, underflow, NaN-producing arithmetic) never throws —
 * it returns the appropriate {@link MPFR} value.
 *
 * The error carries a discriminant `code` so callers can pattern-match
 * without parsing message text.
 */
export class MPFRError extends Error {
  /** Stable machine-readable discriminant. */
  public readonly code: MPFRErrorCode;

  public constructor(code: MPFRErrorCode, message: string) {
    super(message);
    this.code = code;
    // Set the `name` explicitly: under `useDefineForClassFields`, assigning
    // a class field would shadow the prototype's `name`. Direct assignment
    // here gives both `e.name === 'MPFRError'` and a robust `instanceof`.
    this.name = 'MPFRError';
  }
}

/**
 * Minimum allowed precision in bits. Mirrors `MPFR_PREC_MIN`.
 *
 * Ref: mpfr/src/mpfr.h L192 — `#define MPFR_PREC_MIN 1`.
 */
export const PREC_MIN: bigint = 1n;

/**
 * Maximum allowed precision in bits. Chosen to match the smallest of the
 * platform-dependent C values across the configurations we care about,
 * minus the same `- 256` safety margin MPFR itself applies.
 *
 * MPFR's C definition is
 *
 *     #define MPFR_PREC_MAX ((mpfr_prec_t) ((((mpfr_uprec_t) -1) >> 1) - 256))
 *
 * which depends on `_MPFR_PREC_FORMAT`. For `_MPFR_PREC_FORMAT == 2`
 * (`int`, 32 bits) this is `INT_MAX - 256 == 2^31 - 1 - 256 == 2^31 - 257`.
 * Larger formats give larger ceilings; clamping to the 32-bit value keeps
 * goldens portable and well within every host platform's native precision
 * limit. A port that needs to exceed this should require an ADR.
 *
 * Ref: mpfr/src/mpfr.h L191–L193 — `MPFR_PREC_MAX` formula and the
 *   `- 256` safety margin.
 */
export const PREC_MAX: bigint = 2n ** 31n - 257n;

/**
 * Canonical NaN value. NaN does not carry a meaningful precision in this
 * surface (see {@link MPFR}); we expose a single shared constant rather
 * than a factory.
 */
export const NAN_VALUE: MPFR = Object.freeze({
  kind: 'nan',
  sign: 1,
  prec: 0n,
  exp: 0n,
  mant: 0n,
}) satisfies MPFR;

/**
 * `+∞` at the given precision. `prec` is required so the result composes
 * with downstream ops that need a precision context; the value itself
 * has none.
 *
 * @throws {MPFRError} `EPREC` if `prec < {@link PREC_MIN}` or `prec > {@link PREC_MAX}`.
 */
export function posInf(prec: bigint): MPFR {
  assertPrec(prec);
  return { kind: 'inf', sign: 1, prec, exp: 0n, mant: 0n };
}

/**
 * `-∞` at the given precision. See {@link posInf}.
 *
 * @throws {MPFRError} `EPREC` on invalid precision.
 */
export function negInf(prec: bigint): MPFR {
  assertPrec(prec);
  return { kind: 'inf', sign: -1, prec, exp: 0n, mant: 0n };
}

/**
 * `+0` at the given precision.
 *
 * @throws {MPFRError} `EPREC` on invalid precision.
 */
export function posZero(prec: bigint): MPFR {
  assertPrec(prec);
  return { kind: 'zero', sign: 1, prec, exp: 0n, mant: 0n };
}

/**
 * `-0` at the given precision. Signed zero is observable in MPFR rounding —
 * see {@link Sign}.
 *
 * @throws {MPFRError} `EPREC` on invalid precision.
 */
export function negZero(prec: bigint): MPFR {
  assertPrec(prec);
  return { kind: 'zero', sign: -1, prec, exp: 0n, mant: 0n };
}

/**
 * True iff `x` is finite (`'normal'` or `'zero'`). Note: this collides with
 * the global `isFinite` on `number`; callers should use a named import or
 * rename on import (`import { isFinite as isFiniteMpfr } from "./core.ts"`)
 * if the conflict matters.
 */
export function isFinite(x: MPFR): boolean {
  return x.kind === 'normal' || x.kind === 'zero';
}

/**
 * True iff `x` is NaN. Like {@link isFinite}, collides with the global
 * `isNaN` — rename on import if needed.
 */
export function isNaN(x: MPFR): boolean {
  return x.kind === 'nan';
}

/** True iff `x` is `±∞`. */
export function isInf(x: MPFR): boolean {
  return x.kind === 'inf';
}

/** True iff `x` is `±0`. */
export function isZero(x: MPFR): boolean {
  return x.kind === 'zero';
}

/** True iff `x` is a finite nonzero (`'normal'`) value. */
export function isNormal(x: MPFR): boolean {
  return x.kind === 'normal';
}

/**
 * Structurally validate an {@link MPFR}. Throws `MPFRError` with code
 * `EPREC` on the first invariant violation; the message identifies the
 * specific failure for grader diagnostics.
 *
 * Invariants enforced (all of these are required for the value to be
 * well-formed against the locked schema):
 *
 * 1. `kind` is one of `'normal' | 'zero' | 'inf' | 'nan'`.
 * 2. `sign` is `1` or `-1`.
 * 3. `prec`, `exp`, `mant` are all `bigint` (caller-side TS guarantees this
 *    statically; we re-check for runtime callers crossing the type
 *    boundary — e.g. JSON-decoded grader inputs).
 * 4. For `kind !== 'nan'`: `PREC_MIN <= prec <= PREC_MAX`.
 * 5. For `kind === 'nan'`: `prec === 0n`, `sign === 1`, `exp === 0n`,
 *    `mant === 0n` (the canonical {@link NAN_VALUE} shape).
 * 6. For `kind === 'zero'` or `kind === 'inf'`: `exp === 0n`,
 *    `mant === 0n` (singular values carry no exponent or mantissa).
 * 7. For `kind === 'normal'`: `mant >= 2n ** (prec - 1n)` (MSB at position
 *    `prec - 1` is set) and `mant < 2n ** prec` (no bit at position
 *    `>= prec` is set). Equivalent to MPFR's MSB-normalisation rule for
 *    non-singular values.
 *
 * Ports are expected to return values that pass `validate` without
 * post-processing; the runner calls `validate` on every returned value
 * before comparing against the golden.
 */
export function validate(x: MPFR): void {
  // Defensive runtime type checks — TS guarantees these statically inside
  // the library, but the grader feeds in JSON-decoded values whose shape
  // must be re-verified at the trust boundary.
  if (
    x.kind !== 'normal' &&
    x.kind !== 'zero' &&
    x.kind !== 'inf' &&
    x.kind !== 'nan'
  ) {
    throw new MPFRError('EPREC', `invalid kind: ${String(x.kind)}`);
  }
  if (x.sign !== 1 && x.sign !== -1) {
    throw new MPFRError('EPREC', `invalid sign: ${String(x.sign)}`);
  }
  if (typeof x.prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof x.prec}`);
  }
  if (typeof x.exp !== 'bigint') {
    throw new MPFRError('EPREC', `exp must be bigint, got ${typeof x.exp}`);
  }
  if (typeof x.mant !== 'bigint') {
    throw new MPFRError('EPREC', `mant must be bigint, got ${typeof x.mant}`);
  }

  switch (x.kind) {
    case 'nan':
      if (x.prec !== 0n) {
        throw new MPFRError('EPREC', `nan must have prec=0n, got ${x.prec}`);
      }
      if (x.sign !== 1) {
        throw new MPFRError('EPREC', `nan must have sign=1 by convention, got ${x.sign}`);
      }
      if (x.exp !== 0n) {
        throw new MPFRError('EPREC', `nan must have exp=0n, got ${x.exp}`);
      }
      if (x.mant !== 0n) {
        throw new MPFRError('EPREC', `nan must have mant=0n, got ${x.mant}`);
      }
      return;

    case 'zero':
    case 'inf':
      if (x.prec < PREC_MIN) {
        throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${x.prec}`);
      }
      if (x.prec > PREC_MAX) {
        throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${x.prec}`);
      }
      if (x.exp !== 0n) {
        throw new MPFRError('EPREC', `${x.kind} must have exp=0n, got ${x.exp}`);
      }
      if (x.mant !== 0n) {
        throw new MPFRError('EPREC', `${x.kind} must have mant=0n, got ${x.mant}`);
      }
      return;

    case 'normal': {
      if (x.prec < PREC_MIN) {
        throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${x.prec}`);
      }
      if (x.prec > PREC_MAX) {
        throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${x.prec}`);
      }
      if (x.mant === 0n) {
        throw new MPFRError('EPREC', 'normal value must have nonzero mantissa');
      }
      const msbBit = 1n << (x.prec - 1n);
      if ((x.mant & msbBit) === 0n) {
        throw new MPFRError(
          'EPREC',
          `normal mantissa not MSB-aligned: bit ${x.prec - 1n} unset`,
        );
      }
      const upperBound = 1n << x.prec;
      if (x.mant >= upperBound) {
        throw new MPFRError(
          'EPREC',
          `normal mantissa out of range: mant=${x.mant} >= 2^prec=${upperBound}`,
        );
      }
      return;
    }
  }
}

/**
 * Internal: validate a precision argument for a constructor or op entry
 * point. Throws `EPREC` on out-of-range. Not exported — public surface
 * uses {@link validate} for full structural checks.
 */
function assertPrec(prec: bigint): void {
  if (typeof prec !== 'bigint') {
    throw new MPFRError('EPREC', `prec must be bigint, got ${typeof prec}`);
  }
  if (prec < PREC_MIN) {
    throw new MPFRError('EPREC', `prec must be >= ${PREC_MIN}, got ${prec}`);
  }
  if (prec > PREC_MAX) {
    throw new MPFRError('EPREC', `prec must be <= ${PREC_MAX}, got ${prec}`);
  }
}
