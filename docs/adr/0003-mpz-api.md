# ADR 0003 -- mpz/bigint API for `_z` family functions

> Resolves bd `mpfr-ts-3a9`. Unblocks five functions: `mpfr_set_z_2exp`,
> `mpfr_get_z_2exp`, `mpfr_add_z`, `mpfr_sub_z`, `mpfr_mul_z`. Codifies
> the surface that the already-shipped `mpfr_set_z` and `mpfr_get_z`
> implicitly established, so future `_z` ports follow one rule rather
> than re-litigating the API per function.

## Status

Accepted. Closes bd `mpfr-ts-3a9`. Unblocks `mpfr_set_z_2exp`,
`mpfr_get_z_2exp`, `mpfr_add_z`, `mpfr_sub_z`, `mpfr_mul_z`.

## Context

The MPFR public API includes a small family of functions whose C
signatures take or return a GMP `mpz_t` (arbitrary-precision integer).
Five of these are unported and currently `status=blocked` in
`eval/state.db` pending an API-shape decision:

- `mpfr_set_z_2exp(rop, z, e, rnd)` -- set `rop` to `z * 2^e`.
- `mpfr_get_z_2exp(z, f)` -- extract `f` as a pair `{z, exp}` such that
  `f = z * 2^exp`.
- `mpfr_add_z(rop, x, z, rnd)` -- `rop = x + z`.
- `mpfr_sub_z(rop, x, z, rnd)` -- `rop = x - z`.
- `mpfr_mul_z(rop, x, z, rnd)` -- `rop = x * z`.

The blocker was framed in the original `spec.json` files (e.g.
`eval/functions/mpfr_add_z/spec.json`) as two unresolved questions:

1. **Surface question.** Do we expose `_z` variants in the TS public
   API at all, or do users compose via
   `mpfr_set_z(z) -> mpfr_add(x, t, prec, rnd)` themselves?
2. **Coercion question.** If we do expose them, what is the
   bigint-to-mpfr coercion strategy? How is an integer with thousands
   of bits represented in the MPFR mantissa model the schema locks?

The investigation that preceded this ADR observed that the architecture
**has already answered both questions, implicitly**, via the two
shipped `_z` ports:

- `src/ops/set_z.ts` (`mpfr_set_z`) takes `z: bigint` directly -- no
  marshalling to an intermediate `MpzWire`, no separate
  `mpfr_set_from_bigint` factory, no exposed limb-array surface. The
  TS `bigint` *is* the mpz analogue.
- `src/ops/get_z.ts` (`mpfr_get_z`) returns `bigint` directly. Same
  rationale: no intermediate type.

The value-codec (`eval/harness/value_codec.ts`) already supports the
bigint round-trip natively:

- `isDecimalIntegerString` (L129) + `decodeInputValue` (L176-L178) accept
  any `"^-?\d+$"` string from the wire and decode to `bigint`.
- `decodeExpectedOutput` (L276-L277) does the same on the output side.
- `compareOutput` `case 'scalar'` (L450-L466) accepts `bigint` actuals
  against `bigint` expected.
- The generic `'object'` output branch (L546-L559) plus `compareField`
  (L580+) already supports arbitrary `{key: bigint, key: bigint}` pair
  outputs -- which is what `mpfr_get_z_2exp` needs to return its
  `{z, exp}` pair.

The C source for the other three (`mpfr_add_z`, `mpfr_sub_z`,
`mpfr_mul_z`) in `mpfr/src/gmp_op.c` is itself a delegation: build a
temporary `mpfr_t` from the `mpz_t` via `init_set_z`, call
`mpfr_add` / `_sub` / `_mul`, return the ternary. The TS port mirrors
that delegation exactly using the already-shipped `mpfr_set_z` and the
unified `mpfr_add` / `_sub` / `_mul`.

Downstream, at least 13 callers in `mpfr/src/` invoke these `_z`
functions (`mpfr_muldiv_z`, `mpfr_add_q`, `mpfr_sub_q`, `mpfr_cmp_z`,
the Riemann zeta path, several `gamma`-family helpers, etc.). Every
one of them will need this API stabilised before they can be ported.
Letting the question linger blocks not five functions but a whole
downstream cone of the call graph.

## Decision

**Option (a) -- native TS `bigint` as the `mpz_t` analogue.** Every
`_z` function takes `z: bigint` directly in its TS signature, with no
intermediate wrapper type. Functions that return an mpz return
`bigint` directly; the pair-returning `mpfr_get_z_2exp` returns
`{z: bigint, exp: bigint}` as a value tuple (no out-parameter
mutation).

The locked schema in `src/core.ts` is **not** extended -- no new
`MpzWire`, no `MPZValue` type. `bigint` is a native TS primitive,
already supported by every grader pathway, already used by the two
shipped `_z` ports.

## Invariants

The chosen pattern guarantees the following invariants. Any future
`_z` port must honour all of them; the grader enforces them
structurally.

1. **Bigint-in.** Every `_z` function that accepts an mpz on the C
   side accepts a `bigint` on the TS side. The parameter name is `z`
   (matching MPFR's C convention). No marshalling helper, no
   `BigInt.fromString` wrapper, no `MpzInput` union type.

2. **Bigint-out where the C function returns a scalar mpz.** The TS
   port returns `bigint` directly. Pair-returning ops (`_2exp`-style)
   return a frozen plain object `{z: bigint, exp: bigint}` -- a value
   tuple, not a mutated out-parameter. Field names mirror MPFR's
   manual where unambiguous (`z` for the mantissa, `exp` for the
   exponent).

3. **`mpfr_set_z` is the canonical coercion.** Any `_z` arithmetic
   port (`add_z` / `sub_z` / `mul_z` / `div_z`) that needs to lift a
   `bigint` to an `MPFR` for the underlying op MUST use
   `mpfr_set_z(z, p, 'RNDN').value` where `p` is `max(bitLength(z), 1n)`.
   Using `bitLength(z)` as the intermediate precision guarantees the
   coercion is lossless (`mpfr_set_z` documents ternary=0 whenever
   `prec >= bitLength(|z|)`), so the rounding of the composite op is
   the rounding of the underlying `mpfr_add`/`_sub`/`_mul` only. No
   double-rounding.

4. **No emax/emin clamp on input.** The C reference (`set_z_2exp.c`
   L52-L55) clamps against the active `__gmpfr_emax`/`__gmpfr_emin`
   exponent range. The TS schema (`src/core.ts`) has no exponent
   range surface -- `MPFR.exp` is an unbounded `bigint`. Ports do
   not perform this clamp; they emit the value at its mathematical
   exponent. The golden drivers stay within the default MPFR range
   (which is astronomically larger than any reasonable input) so C
   and TS agree on every golden case.

5. **Sign on z=0 follows MPFR.** The C side forces `MPFR_SET_POS(f)`
   on z=0 (`set_z_2exp.c` L40). The TS port matches -- `posZero(prec)`,
   never `negZero`. The `bigint` `0n` has no sign in JS (there is no
   `-0n`), so this is the natural behaviour, but ports must encode it
   explicitly rather than relying on accidental defaults.

## Worked examples

The five unblocked signatures, as the PORT subagent will implement
them:

```ts
// All from src/ops/<name>.ts -- public API.

export function mpfr_set_z_2exp(
  z: bigint, e: bigint, prec: bigint, rnd: RoundingMode,
): Result;

export function mpfr_get_z_2exp(
  x: MPFR,
): { z: bigint; exp: bigint };

export function mpfr_add_z(
  x: MPFR, z: bigint, prec: bigint, rnd: RoundingMode,
): Result;

export function mpfr_sub_z(
  x: MPFR, z: bigint, prec: bigint, rnd: RoundingMode,
): Result;

export function mpfr_mul_z(
  x: MPFR, z: bigint, prec: bigint, rnd: RoundingMode,
): Result;
```

A sample composition that exercises three of them:

```ts
import { mpfr_set_z_2exp, mpfr_add_z, mpfr_get_z_2exp } from 'mpfr-ts';

// Set x = 17 * 2^10 = 17408 at prec 53, RNDN. Lossless: 17 has 5 bits,
// so the result is exact and ternary === 0.
const x = mpfr_set_z_2exp(17n, 10n, 53n, 'RNDN').value;

// Add 3n to x. Composition: lifts 3n to a 2-bit MPFR via mpfr_set_z,
// then mpfr_add at prec 53 RNDN. Result is 17411 at prec 53, ternary 0.
const y = mpfr_add_z(x, 3n, 53n, 'RNDN').value;

// Extract y back as a (z, exp) pair. For y = 17411 at prec 53, this is
// z = 17411 << (53 - bitLength(17411)) (MSB-aligned mantissa) and
// exp = bitLength(17411) - 53 (so y == z * 2^exp).
const { z, exp } = mpfr_get_z_2exp(y);
// invariant: y === z * 2n^exp (exactly; mpfr_get_z_2exp is lossless).
```

## Alternatives considered

**Option (b) -- composition-only public API.** Drop the five `_z`
functions from the public surface entirely; users compose
`mpfr_set_z(z) -> mpfr_add(x, t, prec, rnd)` themselves. Rejected
because (i) it loses the C-side `mpz_fits_slong_p` fast path
(`gmp_op.c` L90-L94, etc.) which we'd want to reintroduce later
anyway; (ii) the C reference for the parent functions
(`mpfr_muldiv_z` -> `mpfr_mul_q` -> `mpfr_div_q`) hardcodes the `_z`
delegations, so porting those would require re-deriving the same
composition -- the `_z` functions are load-bearing as named primitives;
(iii) the surface is a one-line wrapper either way, so "force every
caller to write the wrapper" is worse ergonomics for zero
implementation savings.

## Cross-references

- **Shipped precedent ports** (the working template for this ADR):
  - `/home/tobiasosborne/Projects/mpfr-ts/src/ops/set_z.ts`
  - `/home/tobiasosborne/Projects/mpfr-ts/src/ops/get_z.ts`
  - `/home/tobiasosborne/Projects/mpfr-ts/eval/functions/mpfr_set_z/spec.json`
  - `/home/tobiasosborne/Projects/mpfr-ts/eval/functions/mpfr_set_z/golden_driver.c`
  - `/home/tobiasosborne/Projects/mpfr-ts/eval/functions/mpfr_get_z/golden_driver.c`

- **Functions this ADR unblocks** (status will move from blocked to
  pending after PORT lands):
  - `eval/functions/mpfr_set_z_2exp/`
  - `eval/functions/mpfr_get_z_2exp/`
  - `eval/functions/mpfr_add_z/`
  - `eval/functions/mpfr_sub_z/`
  - `eval/functions/mpfr_mul_z/`

- **C sources** that the ports mirror:
  - `mpfr/src/set_z_2exp.c` -- load-bearing body for `mpfr_set_z` and
    `mpfr_set_z_2exp` (set_z is a one-line delegate with e=0).
  - `mpfr/src/get_z_2exp.c` -- body for `mpfr_get_z_2exp`; also called
    indirectly by the golden-master `jl_kv_mpfr` helper.
  - `mpfr/src/gmp_op.c` -- bodies for `add_z`, `sub_z`, `mul_z` (each a
    delegation through `foo` to the underlying `mpfr_add`/`_sub`/`_mul`
    after `init_set_z`).
  - `mpfr/src/set_z.c` L24-L29 -- trivial delegate (`set_z(rop, z, rnd) =
    set_z_2exp(rop, z, 0, rnd)`).

- **Codec support** (already in place, no changes needed):
  - `eval/harness/value_codec.ts` L129-L131 (`isDecimalIntegerString`),
    L176-L178 (`decodeInputValue` bigint branch), L276-L277
    (`decodeExpectedOutput` bigint branch), L450-L466 (`compareOutput`
    `'scalar'`/bigint branch), L546-L559 (`compareOutput` `'object'`
    branch -- pair output support for `mpfr_get_z_2exp`).

- **Related ADRs:**
  - `docs/adr/0001-spec-merge-policy.md` -- spec-merge precedence
    (sibling shape; same decision-only format).
  - `docs/adr/0002-approximation-helper-grading.md` -- approximation
    grading + parking criteria.

- **Memory:**
  - `~/.claude/projects/-home-tobiasosborne-Projects-mpfr-ts/memory/decision_api_shape.md`
    -- idiomatic-immutable surface; `bigint` follows from
    "TypeScript-native types over MPFR-internal types" baseline.
  - `~/.claude/projects/-home-tobiasosborne-Projects-mpfr-ts/memory/mpfr_storage_traps.md`
    -- `mpfr_get_z_2exp` already de-pads (driver-side); the TS port can
    rely on the same property and emit `{z, exp}` directly.

- **bd:** `mpfr-ts-3a9` (closed by this ADR).
