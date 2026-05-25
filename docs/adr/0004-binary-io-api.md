# ADR 0004 -- Binary I/O API for fpif family

> Resolves bd `mpfr-ts-4h9`. Unblocks six static substrate helpers:
> `mpfr_fpif_store_precision`, `mpfr_fpif_store_exponent`,
> `mpfr_fpif_store_limbs`, `mpfr_fpif_read_precision_from_file`,
> `mpfr_fpif_read_exponent_from_file`, `mpfr_fpif_read_limbs`.
> Pre-decides surface for the four public wrappers (`mpfr_fpif_export`,
> `mpfr_fpif_export_mem`, `mpfr_fpif_import`, `mpfr_fpif_import_mem`)
> for when the callgraph picker reaches them.

## Status

Accepted. Closes bd `mpfr-ts-4h9`.

## Context

MPFR's `fpif` module (`mpfr/src/fpif.c`, 778 LOC) defines a portable
binary interchange format for floating-point values. The format spec
(L30-L65) packs precision, sign+exponent, and mantissa limbs into a
compact little-endian byte stream. Special sentinel byte values
(`MPFR_KIND_ZERO=119`, `MPFR_KIND_INF=120`, `MPFR_KIND_NAN=121`)
distinguish singular values. Typical size is `1 + 1 + ceil(prec/8)`
bytes for a normal value.

The C implementation abstracts the byte sink/source via a function
pointer struct (`ext_data` with `io_fn` member, L76-L97). The same
`mpfr_fpif_export_aux` and `_import_aux` work against either a
`FILE *` (`ext_data_file` wrapping `fread`/`fwrite`) or a memory
arena (`ext_data_memory` wrapping `memcpy` with a cursor). This is
classical C: indirect via vtable so one algorithm serves both
sources.

TypeScript has no FILE* and no raw memory pointers. It has
`Uint8Array` (the binary buffer primitive), and per Rule 12 the
published `src/` runs on Bun OR Node, with **no `node:fs` and no
`Bun.file` imports**. The harness already speaks JSON-encoded golden
records and supports scalar string / bigint round-trips.

Two related questions blocked the family:

1. **API shape.** Does the TS public surface take a `Uint8Array`
   directly, accept a callback-based byte sink/source mirroring
   `ext_data`, or some third option?
2. **Substrate decomposition.** Do the six C static helpers each get
   their own TS file and golden (faithful substrate), or do they
   collapse into one `fpif.ts` module exposing only the public
   contract?

The investigation found:

- The state DB tracks **only the six statics** as `blocked`; the
  public wrappers (`mpfr_fpif_export`, `_export_mem`, `_import`,
  `_import_mem`, `_export_aux`, `_import_aux`) are absent from
  `callgraph.json`'s status table -- they will surface as `pending`
  later. So the ADR's first job is unblocking the six statics; the
  public surface is a related but secondary commitment.
- The callgraph shows the dependency cone: each public wrapper
  reaches every static through `_export_aux` / `_import_aux`. There
  are no callers of `fpif` from elsewhere in MPFR -- `fpif` is a
  leaf subsystem, only entered via the public wrappers.
- The value codec (`eval/harness/value_codec.ts`) supports `bigint`
  and scalar-string round-trips; it does not yet have a native
  `Uint8Array` carrier. The grader needs a wire encoding for the
  byte buffer output of the six store helpers. Decimal-bigint
  encoding (interpret the buffer as a little-endian integer) is the
  simplest viable option; a `hex` string carrier is the alternative.

## Decision

**Option (a) -- `Uint8Array` codec pair at the public surface;
faithful substrate decomposition for the six statics.**

### Public surface (forward commitment; ports land when picker reaches)

```ts
// src/ops/fpif.ts
export function mpfr_fpif_export(x: MPFR): Uint8Array;
export function mpfr_fpif_import(bytes: Uint8Array): MPFR;
```

Both `_mem` and FILE* C variants fold into one TS function each: the
FILE* form is not ported (Rule 12 forbids `node:fs` / `Bun.file` in
`src/`). Library users compose at the application boundary:

```ts
import { mpfr_fpif_export, mpfr_fpif_import } from 'mpfr-ts';
import { readFile, writeFile } from 'node:fs/promises';  // user code

const bytes = mpfr_fpif_export(x);
await writeFile('x.fpif', bytes);

const back = mpfr_fpif_import(await readFile('x.fpif'));
```

The library has no opinion on the file-system layer; the user picks
their runtime (`node:fs`, `Bun.file`, `Deno.readFile`, browser
`File`, network stream). The `Uint8Array` is the boundary type.

### Substrate decomposition (the six unblocked statics)

The six C statics each become a free function in **one** TS module:

```
src/internal/mpfr/fpif.ts
```

Exporting:

```ts
export function fpif_store_precision(precision: bigint): Uint8Array;
export function fpif_store_exponent(x: MPFR): Uint8Array;
export function fpif_store_limbs(x: MPFR): Uint8Array;

export function fpif_read_precision(
  bytes: Uint8Array, pos: number,
): { precision: bigint; nextPos: number };

export function fpif_read_exponent(
  bytes: Uint8Array, pos: number, prec: bigint,
): { kind: MPFR['kind']; sign: 1 | -1; exp: bigint; nextPos: number };

export function fpif_read_limbs(
  bytes: Uint8Array, pos: number, prec: bigint,
): { mant: bigint; nextPos: number };
```

The `_read_*_from_file` C functions read from an `ext_data_ptr` (a
generic byte source). The TS analogues take `(bytes, pos)` and
return `{...payload, nextPos}` -- the cursor is explicit, not hidden
behind a stateful handle. This is the natural TS shape: pure
functions over `(buffer, offset)`, no I/O abstraction needed.

### Wire encoding for the harness

Each store helper's golden case carries the expected byte buffer as
a **decimal-bigint** scalar in `expected_output`. The bytes are
interpreted as a little-endian unsigned integer (`bytes[0]` is the
least-significant byte). This piggybacks on the existing
`isDecimalIntegerString` codec path (`value_codec.ts` L129); no
schema change required.

For the `_read_*` helpers, the input byte buffer travels the same
way (decimal bigint), and the output `{precision, nextPos}` /
`{kind, sign, exp, nextPos}` / `{mant, nextPos}` objects use the
existing object-output codec path (L546-L559) plus per-field bigint
compare.

This is a deliberate choice over hex strings or base64: every codec
pathway needed (encode, decode, compare) already exists. The cost
is one helper in the TS port files
(`bytesToBigInt` / `bigIntToBytes`) to convert at the function
boundary. Both are 5-line ports of standard little-endian
serialization.

## Invariants

The pattern guarantees the following. Future ports in the fpif
family (e.g., when the picker eventually surfaces
`mpfr_fpif_export_aux`) must honour all of them; the grader enforces
them structurally.

1. **`Uint8Array` is the only buffer type.** No `ArrayBuffer`, no
   `Buffer` (Node-specific), no `Uint8ClampedArray`. The public
   surface returns/accepts `Uint8Array`; internal helpers also use
   `Uint8Array` (with a numeric cursor) for byte-level work.

2. **Pure functions over `(buffer, position)`.** The C
   `ext_data_ptr` abstraction is **not ported**. TS substrate
   helpers take a byte array and a numeric position; return a fresh
   payload object plus `nextPos`. No stateful reader/writer
   handles, no callback IO interface.

3. **Little-endian everywhere.** The fpif format spec mandates
   little-endian multi-byte integers (L30-L31). TS helpers
   serialize/deserialize in little-endian regardless of host
   platform -- there is no `HAVE_BIG_ENDIAN` branch (JavaScript
   buffers don't expose host endianness, so the C dual-path is
   collapsed to the single little-endian implementation).

4. **No file system.** `src/` never imports `node:fs`, `node:fs/promises`,
   or `Bun.file`. The FILE* C variants (`mpfr_fpif_export(FILE*, x)`,
   `mpfr_fpif_import(x, FILE*)`) are explicitly **not ported**.
   Users compose with their runtime's file API at the call site.

5. **Errors are thrown, not signalled by return value.** C uses
   `return 1` / `return 0` to signal malformed input
   (`_read_precision_from_file` L256, L259, L270, L279, L291;
   `_read_exponent_from_file` L411, L432, L435, L447, L451, L466,
   L470). The TS substrate throws `MPFRError('EDOMAIN', msg)` with a
   descriptive message ("malformed precision byte", "exponent out of
   range", "buffer truncated at offset N", etc.). This matches the
   existing TS error policy across other ports.

6. **Round-trip identity for the public pair.** For every `MPFR` x
   that the locked schema admits (all kinds, all signs, all
   precisions >= 1, all exponents in default MPFR range):
   `mpfr_fpif_import(mpfr_fpif_export(x))` returns a value
   structurally equal to x. The harness exercises this in
   `eval/integration/fpif_roundtrip.ts` once the public pair lands.

## Worked example -- the six substrate signatures

The PORT subagent will implement these against the existing
golden infrastructure:

```ts
// src/internal/mpfr/fpif.ts

import type { MPFR } from '../../core.ts';
import { MPFRError } from '../../core.ts';

const MAX_PRECSIZE = 7n;
const MAX_EMBEDDED_PRECISION = 248n;     // 255 - 7
const MAX_EMBEDDED_EXPONENT = 47n;
const EXTERNAL_EXPONENT = 94n;
const KIND_ZERO = 119;
const KIND_INF = 120;
const KIND_NAN = 121;

export function fpif_store_precision(precision: bigint): Uint8Array {
  if (precision < 1n) {
    throw new MPFRError('EDOMAIN', `precision must be >= 1, got ${precision}`);
  }
  // ... mirrors fpif.c L208-L240 ...
}

export function fpif_read_precision(
  bytes: Uint8Array, pos: number,
): { precision: bigint; nextPos: number } {
  // ... mirrors fpif.c L249-L302; throws EDOMAIN instead of `return 0` ...
}

// ... and four more.
```

## Alternatives considered

**Option (b) -- Callback IO interface mirroring C's `ext_data`.**
Define `interface MpfrByteSink { write(b: Uint8Array): number; }`
and `interface MpfrByteSource { read(n: number): Uint8Array; }`.
The six statics become generic over a sink/source argument. Public
API becomes `mpfr_fpif_export(x, sink)` / `mpfr_fpif_import(source)`.

Rejected because (i) it adds two new types to the locked schema
(`MpfrByteSink`, `MpfrByteSource`) for a single subsystem -- Law 4
penalises types per function; (ii) it forces the harness's golden
driver to emit raw bytes plus marshal them into a generic sink that
the grader instantiates -- vastly more codec surface than the
decimal-bigint wire format; (iii) 90% of callers want
`Uint8Array <--> MPFR` and would write a one-line
`makeMemorySink`/`makeMemorySource` wrapper anyway -- net negative
ergonomics; (iv) streaming-large-file import is an anti-goal at
this scope (a 1 MB fpif file fits in `Uint8Array` trivially; if
users want streaming chunked import in the future, that's a v2
ADR, not a v1 design constraint).

**Option (c) -- Encode fpif as a `bigint`.** The byte stream
interpreted as a little-endian unsigned integer becomes a single
`bigint`. `mpfr_fpif_export(x): bigint`. Reuses ADR 0003's
bigint-as-primitive principle.

Rejected because (i) the `Uint8Array` boundary is correct for byte
data, and silently converting to `bigint` loses the byte-length
information at the boundary (a leading-zero byte becomes
indistinguishable from a missing one in the bigint encoding); (ii)
file I/O at the user's call site is naturally byte-oriented; users
would need to convert `bigint <--> Uint8Array` themselves before
writing to disk, which is the reverse direction of helpful; (iii)
the harness's existing decimal-bigint codec is a wire-format
choice for transmitting goldens, not a public-API constraint --
they can be different. **Note**: we ARE using bigint as the wire
format inside the harness, but the public API exposes
`Uint8Array`. The codec converts at the harness boundary.

**Option (d) -- Skip / defer the ADR.** File a placeholder ADR
that parks the six statics indefinitely and defers the public
wrappers until a user asks. Rejected because the six statics are
already in `state.db` as `blocked`, accumulating debt; the ADR
investment is small (this document); and `fpif` is a leaf
subsystem so there is no downstream cone competing for the
unblock.

## Open questions deferred to follow-up issues

1. **Public wrappers spec/golden lands when picker surfaces them.**
   The four public wrappers (`mpfr_fpif_export`, `_export_mem`,
   `_import`, `_import_mem`) are not currently in `state.db`.
   When the callgraph picker eventually surfaces them, the ADR
   contract above applies -- `_mem` and FILE* C variants both fold
   to one TS function each. No new ADR needed.

2. **FILE* wrappers stay unported.** If a future user genuinely
   needs `mpfr_fpif_export_to_file(path, x)`, that goes in a
   downstream user-facing convenience module
   (`mpfr-ts/node-io`?). The core library does not ship it.

3. **Streaming / chunked I/O.** If `Uint8Array`-bounded becomes
   limiting (`> 2 GiB` MPFR values are not a thing today; future-
   proofing not warranted), an ADR 0005 successor can re-open
   callback-based IO. Out of scope here.

4. **Integration test.** `eval/integration/fpif_roundtrip.ts`
   should land alongside the first public-pair port. Confirms
   `import(export(x)) === x` across the kind / sign / prec /
   exponent space the locked schema admits. Filed as a follow-up
   when the public pair lands.

## Cross-references

- **C source mirrored:**
  - `mpfr/src/fpif.c` L30-L65 -- format spec.
  - L76-L97 -- `ext_data` family (NOT ported -- decision in
    Invariant 2).
  - L208-L240 -- `mpfr_fpif_store_precision` body.
  - L248-L302 -- `mpfr_fpif_read_precision_from_file` body.
  - L317-L387 -- `mpfr_fpif_store_exponent` body.
  - L399-L473 -- `mpfr_fpif_read_exponent_from_file` body.
  - L483-L511 -- `mpfr_fpif_store_limbs` body.
  - L520-L542 -- `mpfr_fpif_read_limbs` body.
  - L589-L640 -- `mpfr_fpif_export_aux` (composes the three store
    helpers; for the future public-pair port).
  - L643-L692 -- `mpfr_fpif_import_aux` (composes the three read
    helpers; for the future public-pair port).

- **Functions this ADR unblocks** (status will move from `blocked`
  to `pending` after this ADR lands and the next
  `ralph.py --next` runs):
  - `eval/functions/mpfr_fpif_store_precision/` (already exists --
    spec/golden re-confirmed under new ADR)
  - `eval/functions/mpfr_fpif_store_exponent/`
  - `eval/functions/mpfr_fpif_store_limbs/`
  - `eval/functions/mpfr_fpif_read_precision_from_file/`
  - `eval/functions/mpfr_fpif_read_exponent_from_file/`
  - `eval/functions/mpfr_fpif_read_limbs/`

- **Codec support** (already in place, no harness changes required):
  - `eval/harness/value_codec.ts` L129-L131 (`isDecimalIntegerString`)
    -- the byte buffer is decimal-bigint-encoded on the wire.
  - L546-L559 (`compareOutput` `'object'` branch) -- the read
    helpers return objects with bigint fields.

- **Related ADRs:**
  - `docs/adr/0001-spec-merge-policy.md` -- spec-merge precedence
    (sibling shape; same decision-only format).
  - `docs/adr/0002-approximation-helper-grading.md` -- approximation
    grading + parking criteria (unchanged by this ADR).
  - `docs/adr/0003-mpz-api.md` -- the most recent ADR; established
    the "TS-native primitive over MPFR-internal type" pattern that
    this ADR extends to `Uint8Array` for binary buffers.

- **Memory:**
  - `~/.claude/projects/-home-tobiasosborne-Projects-mpfr-ts/memory/decision_api_shape.md`
    -- idiomatic-immutable surface; `Uint8Array` follows from
    "TypeScript-native types over MPFR-internal types" baseline.
  - `~/.claude/projects/-home-tobiasosborne-Projects-mpfr-ts/memory/decision_runtime.md`
    -- Rule 12 (no `node:*` or `Bun.*` in `src/`); load-bearing for
    Invariant 4.

- **bd:** `mpfr-ts-4h9` (closed by this ADR).
