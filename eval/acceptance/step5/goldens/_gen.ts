/**
 * _gen.ts — regenerate the step-5 acceptance goldens.
 *
 * Bun script. Pure (no PRNG, no clock): the same source produces
 * byte-identical JSONL every run, so the goldens in this directory can
 * be regenerated and diffed against the committed copies. Run with:
 *
 *     bun eval/acceptance/step5/goldens/_gen.ts
 *
 * Five files are written:
 *
 *   correct.jsonl          — 10 normal-MPFR cases; tag distribution spans
 *                            happy / edge / adversarial / fuzz / mined.
 *                            (The minimum-counts rule in CLAUDE.md Rule 7
 *                            applies to *function* goldens, not synthetic
 *                            acceptance goldens; we use the tag space
 *                            mainly to exercise the runner's by_tag
 *                            bucketing.)
 *   broken.jsonl           — same 10 cases as correct.jsonl. The expected
 *                            ternary is 0 in both, so when ports/broken.ts
 *                            returns ternary=1 every case fails comparison.
 *   infloop.jsonl          — 5 cases (small — workers terminate fast).
 *   schema_violator.jsonl  — 5 cases (content irrelevant: ast_check
 *                            rejects pre-flight).
 *   nan_equality.jsonl     — 5 NaN-input cases. Per the value_codec
 *                            contract, the NaN wire fields can be any
 *                            shape (decodeMpfr() folds to NAN_VALUE
 *                            unconditionally); we still emit the canonical
 *                            {prec:"0", sign:1, exp:"0", mant:"0"} form
 *                            so the file is human-diagnosable.
 *
 * Wire format per eval/golden_master/common.h:
 *
 *     {"tag":"happy","inputs":{"x":<MpfrWire>},"output":{"value":<MpfrWire>,"ternary":0},"time_ns":<n>}
 *
 * Per src/core.ts validate():
 *   - normal: 2^(prec-1) <= mant < 2^prec  AND  exp,prec,mant as bigints.
 *   - nan:    prec="0", sign=1, exp="0", mant="0".
 *
 * For prec=53 the mantissa range is [2^52, 2^53). We pick a few distinct
 * values in that range to keep cases visibly distinct in the JSONL.
 */

// We don't import 'node:fs' or '@types/bun' — tsconfig has `types: []`.
// Bun exposes `Bun.write` as a built-in for synchronous string-to-file
// writes that doesn't require any ambient `node:fs` typings. Declare the
// minimal surface we use; the runtime resolves it without @types/*.
interface BunNamespace {
  write(path: string, data: string): Promise<number>;
}
declare const Bun: BunNamespace;

// ---------------------------------------------------------------------------
// Types matching the wire format. Re-declared here (not imported from
// value_codec) so this generator stands alone and doesn't drift out of
// sync if value_codec evolves — the wire format is the contract.
// ---------------------------------------------------------------------------

interface MpfrWire {
  readonly kind: 'normal' | 'zero' | 'inf' | 'nan';
  readonly sign: 1 | -1;
  readonly prec: string;
  readonly exp: string;
  readonly mant: string;
}

interface Record {
  readonly tag: 'happy' | 'edge' | 'adversarial' | 'fuzz' | 'mined';
  readonly inputs: { readonly x: MpfrWire };
  readonly output: { readonly value: MpfrWire; readonly ternary: -1 | 0 | 1 };
  readonly time_ns: number;
}

// ---------------------------------------------------------------------------
// Canonical MPFR wire values. PREC=53, MSB-aligned mantissas.
// 2^52 = 4503599627370496; 2^53 - 1 = 9007199254740991.
// ---------------------------------------------------------------------------

/** Construct a `normal` MPFR wire record at prec=53. */
function normal(sign: 1 | -1, exp: bigint, mant: bigint): MpfrWire {
  const PREC = 53n;
  const lo = 1n << (PREC - 1n);
  const hi = 1n << PREC;
  if (mant < lo || mant >= hi) {
    throw new Error(`mantissa ${mant} out of MSB-aligned range [${lo}, ${hi}) for prec=53`);
  }
  return {
    kind: 'normal',
    sign,
    prec: PREC.toString(),
    exp: exp.toString(),
    mant: mant.toString(),
  };
}

/** Canonical NaN wire record per src/core.ts validate(): prec=0, sign=1. */
const NAN_WIRE: MpfrWire = {
  kind: 'nan',
  sign: 1,
  prec: '0',
  exp: '0',
  mant: '0',
};

/**
 * Ten distinct normal MPFR values at prec=53. The mantissas range across
 * the MSB-aligned span [2^52, 2^53) and the exponents span a useful
 * spread so by_tag buckets see structurally-varied inputs.
 *
 * Tags follow the Rule 7 five-class space:
 *   happy: 2 cases, edge: 2 cases, adversarial: 2 cases,
 *   fuzz: 2 cases, mined: 2 cases.
 * (Counts not at Rule 7 minima — this is a synthetic acceptance golden,
 * not a function golden.)
 */
const NORMAL_VALUES: ReadonlyArray<{ readonly tag: Record['tag']; readonly value: MpfrWire }> = [
  // happy: small positive, mantissa exactly 2^52 (MSB only).
  { tag: 'happy', value: normal(1, 1n, 1n << 52n) },
  // happy: small positive, mantissa exactly 2^52 + 1 (MSB + LSB).
  { tag: 'happy', value: normal(1, 2n, (1n << 52n) + 1n) },
  // edge: mantissa exactly 2^53 - 1 (all 53 bits set — top of range).
  { tag: 'edge', value: normal(1, 0n, (1n << 53n) - 1n) },
  // edge: negative sign with smallest mantissa.
  { tag: 'edge', value: normal(-1, -3n, 1n << 52n) },
  // adversarial: large positive exponent (1024 = double-precision overflow boundary).
  { tag: 'adversarial', value: normal(1, 1024n, (1n << 52n) | (1n << 30n)) },
  // adversarial: large negative exponent (mirror of double-precision underflow).
  { tag: 'adversarial', value: normal(-1, -1022n, (1n << 53n) - 2n) },
  // fuzz: arbitrary mid-range positive.
  { tag: 'fuzz', value: normal(1, 100n, (1n << 52n) + 0x1234567n) },
  // fuzz: arbitrary mid-range negative.
  { tag: 'fuzz', value: normal(-1, -100n, (1n << 52n) + 0x76543210n) },
  // mined: value reminiscent of a doc example (3.14 at prec=53,
  // mantissa = round(3.14 * 2^51), exp=2).
  { tag: 'mined', value: normal(1, 2n, 7070651414971679n) },
  // mined: value reminiscent of e (~2.71828, similar magnitude).
  { tag: 'mined', value: normal(1, 2n, 6121026514868074n) },
];

// ---------------------------------------------------------------------------
// Emit
// ---------------------------------------------------------------------------

/**
 * Encode a record to one JSONL line. We hand-write the serialiser (rather
 * than `JSON.stringify`) to (a) pin the key order for diff stability and
 * (b) keep numeric `time_ns` as a JS number while string-encoded fields
 * stay as strings.
 */
function encodeRecord(r: Record): string {
  const x = r.inputs.x;
  const v = r.output.value;
  const xJson =
    `{"kind":"${x.kind}",` +
    `"sign":${x.sign},` +
    `"prec":"${x.prec}",` +
    `"exp":"${x.exp}",` +
    `"mant":"${x.mant}"}`;
  const vJson =
    `{"kind":"${v.kind}",` +
    `"sign":${v.sign},` +
    `"prec":"${v.prec}",` +
    `"exp":"${v.exp}",` +
    `"mant":"${v.mant}"}`;
  return (
    `{"tag":"${r.tag}",` +
    `"inputs":{"x":${xJson}},` +
    `"output":{"value":${vJson},"ternary":${r.output.ternary}},` +
    `"time_ns":${r.time_ns}}`
  );
}

/** Write a JSONL file. Trailing newline per POSIX text-file convention. */
async function writeJsonl(path: string, records: readonly Record[]): Promise<void> {
  const body = records.map(encodeRecord).join('\n') + '\n';
  await Bun.write(path, body);
}

// Build a `Record` from a wire value: ternary always 0 (identity port is
// exact). time_ns is a deterministic small int derived from index so
// regeneration is byte-stable.
function rec(tag: Record['tag'], value: MpfrWire, time_ns: number): Record {
  return {
    tag,
    inputs: { x: value },
    output: { value, ternary: 0 },
    time_ns,
  };
}

// 10 cases — correct.jsonl and broken.jsonl share these.
const correct: Record[] = NORMAL_VALUES.map((nv, i) =>
  rec(nv.tag, nv.value, 1000 + i * 7),
);

// 5 cases for infloop. Drawn from the first 5 normals.
const infloop: Record[] = NORMAL_VALUES.slice(0, 5).map((nv, i) =>
  rec(nv.tag, nv.value, 2000 + i * 11),
);

// 5 cases for schema_violator. Content immaterial.
const schemaViolator: Record[] = NORMAL_VALUES.slice(0, 5).map((nv, i) =>
  rec(nv.tag, nv.value, 3000 + i * 13),
);

// 5 NaN-input cases. Tags spread across the classes to verify by_tag
// bucketing on a NaN-heavy golden.
const NAN_TAGS: ReadonlyArray<Record['tag']> = [
  'happy',
  'edge',
  'adversarial',
  'fuzz',
  'mined',
];
const nanEquality: Record[] = NAN_TAGS.map((tag, i) =>
  rec(tag, NAN_WIRE, 4000 + i * 17),
);

const DIR = new URL('.', import.meta.url).pathname;

await writeJsonl(`${DIR}correct.jsonl`, correct);
await writeJsonl(`${DIR}broken.jsonl`, correct);
await writeJsonl(`${DIR}infloop.jsonl`, infloop);
await writeJsonl(`${DIR}schema_violator.jsonl`, schemaViolator);
await writeJsonl(`${DIR}nan_equality.jsonl`, nanEquality);

console.log('wrote 5 goldens:');
console.log(`  ${DIR}correct.jsonl          (${correct.length} cases)`);
console.log(`  ${DIR}broken.jsonl           (${correct.length} cases)`);
console.log(`  ${DIR}infloop.jsonl          (${infloop.length} cases)`);
console.log(`  ${DIR}schema_violator.jsonl  (${schemaViolator.length} cases)`);
console.log(`  ${DIR}nan_equality.jsonl     (${nanEquality.length} cases)`);
