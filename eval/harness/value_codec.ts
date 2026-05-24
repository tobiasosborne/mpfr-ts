/**
 * value_codec.ts — wire-format <-> runtime value conversion for the grader.
 *
 * Pure functions, no I/O, no module-level side effects. The single source of
 * truth for how the JSON emitted by eval/golden_master/common.h decodes into
 * the `MPFR` value shape locked in src/core.ts, and how runtime values are
 * compared against expectations.
 *
 * Three hallucination-risk callouts from CLAUDE.md drive non-obvious code
 * here; each is cited inline at the site it applies:
 *
 *   - "NaN ≠ NaN" — direct === on MPFR-NaN values would reject every NaN
 *     golden. compareOutput() short-circuits on both sides being kind:'nan'.
 *   - "Signed zero is real" — +0 and -0 are distinct MPFR values; the
 *     equality check on kind:'zero' includes sign.
 *   - "Ternary is the sign of (rounded - exact), not 0/1" — the Result
 *     branch compares ternary exactly (no abs, no normalization).
 *
 * And one from mpfr_storage_traps.md:
 *
 *   - "NaN sentinel divergence": the wire format may serialise a NaN with
 *     its originating precision (libmpfr keeps it; the TS schema discards
 *     it). decodeMpfr() folds every NaN wire record to NAN_VALUE.
 *
 * Ref: src/core.ts — locked MPFR / RoundingMode / Result / Ternary types.
 * Ref: eval/golden_master/common.h — jl_kv_mpfr, jl_output_result wire shape.
 */

import {
  type MPFR,
  type MPFRKind,
  type RoundingMode,
  type Sign,
  type Ternary,
  NAN_VALUE,
  validate,
} from '../../src/core.ts';

// ---------------------------------------------------------------------------
// Wire types
// ---------------------------------------------------------------------------

/**
 * MPFR value as it appears on the wire. `prec`, `exp`, `mant` are decimal
 * strings because the C side emits them with `"\"%" PRId64 "\""` etc. so they
 * cross the JSON boundary without losing the high bits of a bigint.
 *
 * Ref: eval/golden_master/common.h L283–L334 — jl_kv_mpfr emit shape.
 */
export interface MpfrWire {
  readonly kind: MPFRKind;
  readonly sign: Sign;
  readonly prec: string;
  readonly exp: string;
  readonly mant: string;
}

// ---------------------------------------------------------------------------
// MPFR codec
// ---------------------------------------------------------------------------

/**
 * Decode an MPFR wire object to the runtime {@link MPFR} value. Validates
 * the result via `validate()` from src/core.ts; throws `MPFRError` on a
 * malformed wire record (e.g. mantissa not MSB-aligned).
 *
 * NaN handling: per mpfr_storage_traps.md §3, the wire may carry the
 * originating precision on a NaN (libmpfr keeps it; the TS schema discards
 * it). We fold every `kind === 'nan'` wire record to the canonical
 * {@link NAN_VALUE} unconditionally, ignoring whatever prec/sign/exp/mant
 * the wire carried. This is also what jl_kv_mpfr in common.h emits today,
 * so the fold is currently a no-op for our own goldens — but defensive
 * decoding here means a future driver bug can't poison validate().
 */
export function decodeMpfr(w: MpfrWire): MPFR {
  if (w.kind === 'nan') {
    // NaN sentinel divergence (mpfr_storage_traps.md §3): ignore wire fields.
    return NAN_VALUE;
  }
  const value: MPFR = {
    kind: w.kind,
    sign: w.sign,
    prec: BigInt(w.prec),
    exp: BigInt(w.exp),
    mant: BigInt(w.mant),
  };
  validate(value);
  return value;
}

/**
 * Encode an MPFR back to a canonical JSON string for use in error messages.
 *
 * Keys are emitted in a fixed order (kind, sign, prec, exp, mant) so two
 * encodings of the same value are character-identical — letting a string
 * diff in a grader log surface the differing field directly. BigInts go
 * out as decimal strings (the wire format), not BigInt literals, since
 * `JSON.stringify` cannot serialise BigInt at all.
 */
export function encodeMpfrForCompare(v: MPFR): string {
  // Fixed key order: matches the C-side common.h emit for byte-identity
  // when round-tripping a libmpfr-emitted record.
  return (
    `{"kind":"${v.kind}",` +
    `"sign":${v.sign},` +
    `"prec":"${v.prec.toString()}",` +
    `"exp":"${v.exp.toString()}",` +
    `"mant":"${v.mant.toString()}"}`
  );
}

// ---------------------------------------------------------------------------
// Input decoding
// ---------------------------------------------------------------------------

const ROUNDING_MODES: ReadonlySet<string> = new Set([
  'RNDN',
  'RNDZ',
  'RNDU',
  'RNDD',
  'RNDA',
]);

/**
 * `true` iff `s` is a decimal-integer string parseable via `BigInt(s)` —
 * optional leading `-`, then one or more digits, nothing else. Used to
 * distinguish a bigint-on-the-wire from an arbitrary user string.
 */
function isDecimalIntegerString(s: string): boolean {
  return /^-?\d+$/.test(s);
}

/**
 * `true` iff `o` is an object with a `kind` field that matches one of the
 * four {@link MPFRKind} discriminants. Just enough to dispatch — the full
 * shape check is `validate()` inside `decodeMpfr`.
 */
function looksLikeMpfrWire(o: object): o is MpfrWire {
  // Casting to a record with unknown values is the standard way to inspect
  // an unknown object without disabling type checking; the property access
  // pattern below would otherwise be ill-typed.
  const r = o as Readonly<Record<string, unknown>>;
  const kind = r['kind'];
  return (
    kind === 'normal' || kind === 'zero' || kind === 'inf' || kind === 'nan'
  );
}

/**
 * Decode an arbitrary JSON-decoded input value into the runtime
 * representation the port expects:
 *
 *   - `{kind, sign, prec, exp, mant}` → MPFR (via decodeMpfr)
 *   - `["1","2",...]` → readonly bigint[]   (a GMP mpn limb array)
 *   - `"RNDN" | "RNDZ" | ...` → RoundingMode
 *   - `"123"` (any other decimal-integer string) → bigint
 *   - `123` (a JS number) → number (small ints: counts, indices, flags)
 *   - `true | false` → boolean
 *
 * Returns `unknown` — the caller is expected to know the function signature
 * and may narrow by position. We never silently coerce a string that looks
 * like a non-integer; an unrecognised string is returned as-is so the
 * caller can decide.
 */
export function decodeInputValue(raw: unknown): unknown {
  if (raw === null || raw === undefined) return raw;

  if (typeof raw === 'number' || typeof raw === 'boolean') {
    return raw;
  }

  if (typeof raw === 'string') {
    if (ROUNDING_MODES.has(raw)) {
      return raw as RoundingMode;
    }
    if (isDecimalIntegerString(raw)) {
      return BigInt(raw);
    }
    // IEEE 754 special-value tokens from jl_kv_double in
    // eval/golden_master/common.h. JSON cannot represent NaN/±Infinity
    // as bare numbers (RFC 8259 forbids it), so the C side wraps them
    // in quoted strings; we recognise the three sentinel tokens here
    // and return them as JS `number` values rather than strings. This
    // is the *input* path — the call site is decoding a value the C
    // driver wants the TS port to receive as a `number`. Finite doubles
    // emitted via "%.17g" hit the catch-all `Number(raw)` branch below.
    if (raw === 'NaN') return Number.NaN;
    if (raw === '+Infinity') return Number.POSITIVE_INFINITY;
    if (raw === '-Infinity') return Number.NEGATIVE_INFINITY;
    // Finite-double check: `%.17g` outputs look like `1.5`, `-1.5`,
    // `5e-324`, `-3.14e+0`, etc. — anything `Number()` can parse to a
    // finite value (and which is NOT a plain decimal integer, already
    // handled above) is treated as a double. Strict parse: `Number("")`
    // is 0, `Number("0x10")` is 16 — both ambiguous, so we require at
    // least one digit AND that the string isn't empty AND that the
    // strict-parse `Number(raw)` is finite. Hex / octal / whitespace-
    // padded strings fall through to the return-as-string branch.
    if (/^-?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?$/.test(raw)) {
      const n = Number(raw);
      if (Number.isFinite(n)) return n;
    }
    return raw;
  }

  if (Array.isArray(raw)) {
    // We treat any all-string-of-decimal-integers array as a limb array.
    // Mixed arrays fall back to per-element recursion.
    if (raw.every((x) => typeof x === 'string' && isDecimalIntegerString(x))) {
      return raw.map((x) => BigInt(x as string));
    }
    return raw.map((x) => decodeInputValue(x));
  }

  if (typeof raw === 'object') {
    if (looksLikeMpfrWire(raw)) {
      return decodeMpfr(raw);
    }
    // Generic object: recurse on values. Used for struct-shaped inputs that
    // common.h doesn't emit today but may in future.
    const out: Record<string, unknown> = {};
    for (const [k, v] of Object.entries(raw)) {
      out[k] = decodeInputValue(v);
    }
    return out;
  }

  return raw;
}

// ---------------------------------------------------------------------------
// Output decoding
// ---------------------------------------------------------------------------

/**
 * Discriminated runtime representation of a golden's expected output. The
 * four shapes are the four jl_output_* helpers in common.h.
 *
 * Note on the boolean scalar variant: the predicate family (mpfr_less_p
 * et al.) returns a TS `boolean`, with `true`/`false` emitted on the wire
 * by `jl_output_scalar_bool` in common.h. We collapse it into the same
 * `'scalar'` kind because the comparison rule is the same shape as the
 * number-vs-number one (strict `===`), just on a different primitive.
 * Keeping it a separate `'bool'` discriminant would force every caller to
 * branch over it without adding correctness.
 */
export type ExpectedOutput =
  | { readonly kind: 'result'; readonly value: MPFR; readonly ternary: Ternary }
  | { readonly kind: 'mpfr'; readonly value: MPFR }
  | { readonly kind: 'object'; readonly fields: Readonly<Record<string, unknown>> }
  | { readonly kind: 'scalar'; readonly value: bigint | number | boolean };

/**
 * `true` iff `t` is a valid {@link Ternary} value (`-1`, `0`, or `1`).
 * Used to distinguish the Result-shaped output from a generic object.
 */
function isTernary(t: unknown): t is Ternary {
  return t === -1 || t === 0 || t === 1;
}

/**
 * Decode the `output` field of a golden record. Dispatch on shape:
 *
 *   1. `{value: MpfrWire, ternary: -1|0|1}`  →  Result (the common case;
 *      every rounding op uses this shape via jl_output_result).
 *   2. `MpfrWire`                            →  bare MPFR (jl_output_mpfr).
 *   3. `{<k>: <v>, ...}` (any other object)  →  generic struct
 *      (jl_output_begin_object / jl_output_end_object).
 *   4. decimal string                        →  bigint (jl_output_scalar_u64).
 *   5. JS number                             →  number (small int result).
 *
 * Throws on a wholly unrecognised shape so a malformed golden trips the
 * grader's internal-error path rather than silently mis-grading.
 */
export function decodeExpectedOutput(raw: unknown): ExpectedOutput {
  if (typeof raw === 'string') {
    if (isDecimalIntegerString(raw)) {
      return { kind: 'scalar', value: BigInt(raw) };
    }
    // IEEE 754 sentinel tokens from jl_output_scalar_double in
    // eval/golden_master/common.h. JSON cannot carry NaN/±Infinity as
    // bare numbers (RFC 8259), so the C emitter wraps them. Mirror the
    // input-side parsing in decodeInputValue.
    if (raw === 'NaN') return { kind: 'scalar', value: Number.NaN };
    if (raw === '+Infinity') {
      return { kind: 'scalar', value: Number.POSITIVE_INFINITY };
    }
    if (raw === '-Infinity') {
      return { kind: 'scalar', value: Number.NEGATIVE_INFINITY };
    }
    // Finite-double-shaped strings (the `%.17g`-with-decimal-point form
    // jl_output_scalar_double emits). Parsed via strict `Number(s)` —
    // requires at least one digit and that the result is finite. Same
    // regex as decodeInputValue's finite-double branch.
    if (/^-?(?:\d+\.\d*|\.\d+|\d+)(?:[eE][+-]?\d+)?$/.test(raw)) {
      const n = Number(raw);
      if (Number.isFinite(n)) return { kind: 'scalar', value: n };
    }
    throw new Error(`unrecognised scalar string output: ${JSON.stringify(raw)}`);
  }
  if (typeof raw === 'number') {
    return { kind: 'scalar', value: raw };
  }
  if (typeof raw === 'boolean') {
    // Bare-boolean output — emitted by jl_output_scalar_bool in
    // eval/golden_master/common.h for the predicate family
    // (mpfr_less_p / mpfr_greater_p / mpfr_lessequal_p /
    // mpfr_greaterequal_p / mpfr_equal_p). The compareOutput's scalar
    // branch handles boolean via strict `===`.
    return { kind: 'scalar', value: raw };
  }
  if (typeof raw === 'object' && raw !== null && !Array.isArray(raw)) {
    const r = raw as Readonly<Record<string, unknown>>;
    if (looksLikeMpfrWire(raw)) {
      return { kind: 'mpfr', value: decodeMpfr(raw) };
    }
    // Result-shaped: { value: <wire>, ternary: <-1|0|1> }
    const valueField = r['value'];
    const ternaryField = r['ternary'];
    if (
      valueField !== undefined &&
      ternaryField !== undefined &&
      typeof valueField === 'object' &&
      valueField !== null &&
      !Array.isArray(valueField) &&
      looksLikeMpfrWire(valueField)
    ) {
      // Shape looks like a Result (value is MpfrWire-shaped, ternary
      // present). Once we've committed to that interpretation, the ternary
      // MUST be valid — otherwise the golden is malformed and we surface
      // it as an internal error rather than silently falling through to
      // generic-object decoding and grading the case as a struct mismatch.
      if (!isTernary(ternaryField)) {
        throw new Error(
          `malformed Result output: value is MpfrWire-shaped but ternary ` +
            `is ${JSON.stringify(ternaryField)} (must be -1, 0, or 1)`,
        );
      }
      return {
        kind: 'result',
        value: decodeMpfr(valueField),
        ternary: ternaryField,
      };
    }
    // Generic struct output. Decode each field recursively.
    const fields: Record<string, unknown> = {};
    for (const [k, v] of Object.entries(r)) {
      fields[k] = decodeInputValue(v);
    }
    return { kind: 'object', fields };
  }
  throw new Error(
    `unrecognised output shape: ${typeof raw} (${JSON.stringify(raw)})`,
  );
}

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

/**
 * Compare two MPFR values structurally. Returns `null` on match or a short
 * human-readable mismatch description. Handles:
 *
 *   - NaN reflexivity: both kind:'nan' → match. (CLAUDE.md
 *     "Hallucination-risk callouts: NaN ≠ NaN".)
 *   - Signed zero: kind:'zero' compares both kind AND sign. (CLAUDE.md
 *     "Signed zero is real".)
 *   - Normal / inf: full structural equality on all fields.
 */
function compareMpfr(actual: MPFR, expected: MPFR): string | null {
  if (actual.kind === 'nan' && expected.kind === 'nan') {
    // NaN == NaN by reflexive intent; both already canonicalised to NAN_VALUE.
    return null;
  }
  if (actual.kind !== expected.kind) {
    return `kind mismatch: expected ${expected.kind}, got ${actual.kind}`;
  }
  if (actual.sign !== expected.sign) {
    return `sign mismatch: expected ${expected.sign}, got ${actual.sign} (signed zero/inf is observable)`;
  }
  if (actual.prec !== expected.prec) {
    return `prec mismatch: expected ${expected.prec}, got ${actual.prec}`;
  }
  if (actual.exp !== expected.exp) {
    return `exp mismatch: expected ${expected.exp}, got ${actual.exp}`;
  }
  if (actual.mant !== expected.mant) {
    return `mant mismatch: expected ${expected.mant}, got ${actual.mant}`;
  }
  return null;
}

/**
 * `true` iff `x` is plausibly an MPFR value (object with the four required
 * fields and a recognisable kind discriminant). Strict enough to avoid
 * matching unrelated objects whose first field happens to be `kind`.
 */
function looksLikeMpfr(x: unknown): x is MPFR {
  if (x === null || typeof x !== 'object') return false;
  const r = x as Readonly<Record<string, unknown>>;
  const kind = r['kind'];
  if (kind !== 'normal' && kind !== 'zero' && kind !== 'inf' && kind !== 'nan') {
    return false;
  }
  return (
    typeof r['sign'] === 'number' &&
    typeof r['prec'] === 'bigint' &&
    typeof r['exp'] === 'bigint' &&
    typeof r['mant'] === 'bigint'
  );
}

/**
 * Element-wise compare two readonly arrays of bigints. Returns null on
 * match, or a description of the first index that differs (or the length
 * mismatch).
 */
function compareBigintArray(
  actual: readonly bigint[],
  expected: readonly bigint[],
): string | null {
  if (actual.length !== expected.length) {
    return `array length mismatch: expected ${expected.length}, got ${actual.length}`;
  }
  for (let i = 0; i < expected.length; i++) {
    if (actual[i] !== expected[i]) {
      return `array[${i}] mismatch: expected ${expected[i]}, got ${actual[i]}`;
    }
  }
  return null;
}

/**
 * Compare an arbitrary actual value against a decoded {@link ExpectedOutput}.
 * Returns `null` on match, or a structured error string on mismatch.
 *
 * The error string is the basis of `first_error` in the grade.json; it must
 * be self-contained enough for a human to diagnose without re-running.
 */
export function compareOutput(
  actual: unknown,
  expected: ExpectedOutput,
): string | null {
  switch (expected.kind) {
    case 'scalar': {
      if (typeof expected.value === 'bigint') {
        // Accept bigint or number on the actual side; numbers are coerced
        // because not every port returns bigint for a small u64 result.
        if (typeof actual === 'bigint') {
          return actual === expected.value
            ? null
            : `scalar mismatch: expected ${expected.value}, got ${actual}`;
        }
        if (typeof actual === 'number' && Number.isInteger(actual)) {
          return BigInt(actual) === expected.value
            ? null
            : `scalar mismatch: expected ${expected.value}, got ${actual}`;
        }
        if (typeof actual === 'number' && Number.isFinite(actual)) {
          return `scalar mismatch: expected bigint ${expected.value}, got non-integer number ${actual}`;
        }
        return `scalar mismatch: expected bigint ${expected.value}, got ${typeof actual} ${String(actual)}`;
      }
      if (typeof expected.value === 'boolean') {
        // Boolean-valued scalar — the predicate family's return type.
        // Strict `===` is sufficient (no NaN/signed-zero corner like
        // the number branch). A port that returns 0/1, "true"/"false",
        // or any non-boolean must fail the grade — we do NOT coerce.
        if (typeof actual === 'boolean' && actual === expected.value) {
          return null;
        }
        return `scalar mismatch: expected boolean ${expected.value}, got ${typeof actual} ${String(actual)}`;
      }
      // number-valued scalar. Use `Object.is` rather than `===` so:
      //   - NaN equals NaN (=== returns false; CLAUDE.md hallucination
      //     callout "NaN ≠ NaN" — for the OUTPUT side of get-conversion
      //     ops the only way to assert NaN-equality is via Object.is or
      //     a Number.isNaN dance).
      //   - +0 is distinct from -0 (=== returns true; Object.is returns
      //     false). Signed zero is observable in MPFR's get_d/set_d
      //     round-trip (CLAUDE.md hallucination callout "Signed zero is
      //     real"), so a port that drops the sign on a -0 input must
      //     fail the grade.
      if (typeof actual === 'number' && Object.is(actual, expected.value)) {
        return null;
      }
      return `scalar mismatch: expected ${expected.value} (Object.is), got ${String(actual)}`;
    }

    case 'mpfr': {
      if (!looksLikeMpfr(actual)) {
        return `expected MPFR value, got ${typeof actual} (${String(actual)})`;
      }
      // Validate the structural invariants on the actual side too — a
      // port that returns a "looks right" object with bad MSB alignment
      // is wrong even if it equates to the expected.
      try {
        validate(actual);
      } catch (e) {
        return `actual MPFR fails validate(): ${e instanceof Error ? e.message : String(e)}`;
      }
      return compareMpfr(actual, expected.value);
    }

    case 'result': {
      if (actual === null || typeof actual !== 'object') {
        return `expected Result {value, ternary}, got ${typeof actual} (${String(actual)})`;
      }
      const r = actual as Readonly<Record<string, unknown>>;
      const av = r['value'];
      const at = r['ternary'];
      if (!isTernary(at)) {
        return `ternary mismatch: expected ${expected.ternary}, got ${String(at)} (must be -1, 0, or 1)`;
      }
      if (at !== expected.ternary) {
        // The "sign of (rounded - exact)" callout: direction matters, do
        // NOT abs() before comparing. (CLAUDE.md hallucination-risk.)
        return `ternary mismatch: expected ${expected.ternary}, got ${at}`;
      }
      if (!looksLikeMpfr(av)) {
        return `expected Result.value to be MPFR, got ${typeof av} (${String(av)})`;
      }
      try {
        validate(av);
      } catch (e) {
        return `Result.value fails validate(): ${e instanceof Error ? e.message : String(e)}`;
      }
      return compareMpfr(av, expected.value);
    }

    case 'object': {
      if (actual === null || typeof actual !== 'object' || Array.isArray(actual)) {
        return `expected object output, got ${typeof actual} (${String(actual)})`;
      }
      const a = actual as Readonly<Record<string, unknown>>;
      for (const [k, want] of Object.entries(expected.fields)) {
        const got = a[k];
        const fieldErr = compareField(got, want);
        if (fieldErr !== null) {
          return `object field '${k}': ${fieldErr}`;
        }
      }
      return null;
    }

    default: {
      // Exhaustiveness guard. If a new variant is added to ExpectedOutput
      // without a corresponding case, TS will refuse the `never` assignment
      // at compile time. The runtime throw is a defence against a malformed
      // ExpectedOutput object that bypasses decodeExpectedOutput.
      const _exhaust: never = expected;
      throw new Error(
        `unhandled ExpectedOutput kind: ${String((_exhaust as { kind?: string }).kind)}`,
      );
    }
  }
}

/**
 * Compare a single field value (used recursively by the 'object' branch).
 * Accepts the same shapes `decodeInputValue` produces. Distinct from
 * `compareOutput` because object fields aren't wrapped in the
 * {@link ExpectedOutput} discriminant.
 */
function compareField(actual: unknown, expected: unknown): string | null {
  if (typeof expected === 'bigint') {
    if (typeof actual === 'bigint') {
      return actual === expected ? null : `expected ${expected}, got ${actual}`;
    }
    if (typeof actual === 'number' && Number.isInteger(actual)) {
      return BigInt(actual) === expected
        ? null
        : `expected ${expected}, got ${actual}`;
    }
    if (typeof actual === 'number' && Number.isFinite(actual)) {
      return `expected bigint ${expected}, got non-integer number ${actual}`;
    }
    return `expected bigint ${expected}, got ${typeof actual} (${String(actual)})`;
  }
  if (typeof expected === 'number') {
    return actual === expected ? null : `expected ${expected}, got ${String(actual)}`;
  }
  if (typeof expected === 'string') {
    return actual === expected ? null : `expected "${expected}", got "${String(actual)}"`;
  }
  if (Array.isArray(expected)) {
    if (!Array.isArray(actual)) {
      return `expected array, got ${typeof actual}`;
    }
    if (expected.every((x) => typeof x === 'bigint')) {
      return compareBigintArray(actual as readonly bigint[], expected as readonly bigint[]);
    }
    if (actual.length !== expected.length) {
      return `array length mismatch: expected ${expected.length}, got ${actual.length}`;
    }
    for (let i = 0; i < expected.length; i++) {
      const sub = compareField(actual[i], expected[i]);
      if (sub !== null) return `[${i}]: ${sub}`;
    }
    return null;
  }
  if (typeof expected === 'object' && expected !== null) {
    if (looksLikeMpfr(expected)) {
      if (!looksLikeMpfr(actual)) {
        return `expected MPFR, got ${typeof actual} (${String(actual)})`;
      }
      try {
        validate(actual);
      } catch (e) {
        return `actual fails validate(): ${e instanceof Error ? e.message : String(e)}`;
      }
      return compareMpfr(actual, expected);
    }
    if (actual === null || typeof actual !== 'object' || Array.isArray(actual)) {
      return `expected object, got ${typeof actual}`;
    }
    const a = actual as Readonly<Record<string, unknown>>;
    const e = expected as Readonly<Record<string, unknown>>;
    for (const [k, want] of Object.entries(e)) {
      const sub = compareField(a[k], want);
      if (sub !== null) return `.${k}: ${sub}`;
    }
    return null;
  }
  // null/undefined/boolean — strict equality.
  return actual === expected ? null : `expected ${String(expected)}, got ${String(actual)}`;
}
