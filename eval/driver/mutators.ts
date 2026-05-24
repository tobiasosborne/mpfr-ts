/**
 * mutators.ts - source-level mutators for mutation-proving goldens (CLAUDE.md
 * PIL.3). Each returns a broken port variant; grader confirms composite drops
 * below 0.95. Idempotent on non-matches. Regex-only, mirrors ast_check.ts.
 */
export type MutationName = 'op-swap' | 'rnd-swap' | 'ternary-negate' | 'sign-flip';
export interface MutationResult { readonly mutated: string; readonly applied: boolean; readonly description: string; }
const MUTATIONS: readonly MutationName[] = ['op-swap', 'rnd-swap', 'ternary-negate', 'sign-flip'];
const OP_PAIRS: ReadonlyArray<readonly [string, string]> = [['mpfr_add', 'mpfr_sub'], ['mpfr_mul', 'mpfr_div']];
const RND_PAIRS: ReadonlyArray<readonly [string, string]> = [['RNDN', 'RNDZ'], ['RNDU', 'RNDD']];
const s = (n: number) => n === 1 ? '' : 's';
const noop = (src: string, m: string): MutationResult => ({ mutated: src, applied: false, description: m });

function opSwap(src: string): MutationResult {
  // First pair where one side appears as a call (trailing `(`); imports lack
  // the paren and stay intact.
  for (const [a, b] of OP_PAIRS) {
    const reA = new RegExp(`\\b${a}\\(`, 'g'), reB = new RegExp(`\\b${b}\\(`, 'g');
    const na = (src.match(reA) ?? []).length, nb = (src.match(reB) ?? []).length;
    if (na > 0 && nb === 0) return { mutated: src.replace(reA, `${b}(`), applied: true, description: `op-swap: '${a}(' -> '${b}(' (${na} occurrence${s(na)})` };
    if (nb > 0 && na === 0) return { mutated: src.replace(reB, `${a}(`), applied: true, description: `op-swap: '${b}(' -> '${a}(' (${nb} occurrence${s(nb)})` };
  }
  return noop(src, 'op-swap: no add/sub/mul/div call patterns found');
}

function rndSwap(src: string): MutationResult {
  // Quoted RND tokens (single or double). Bidirectional via sentinel.
  for (const [a, b] of RND_PAIRS) {
    const reA = new RegExp(`(['"])${a}\\1`, 'g'), reB = new RegExp(`(['"])${b}\\1`, 'g');
    const na = (src.match(reA) ?? []).length, nb = (src.match(reB) ?? []).length;
    if (na + nb === 0) continue;
    const S = '__MUTATOR_RND_PLACEHOLDER__';
    let out = src.replace(reA, `$1${S}$1`).replace(reB, `$1${a}$1`);
    out = out.replace(new RegExp(`(['"])${S}\\1`, 'g'), `$1${b}$1`);
    return { mutated: out, applied: true, description: `rnd-swap: '${a}' <-> '${b}' (${na + nb} occurrence${s(na + nb)})` };
  }
  return noop(src, 'rnd-swap: no RND string literals found');
}

function ternaryNegate(src: string): MutationResult {
  // Object-literal entries `, ternary: <value>` or `{ ternary: <value>`. The
  // leading `,` or `{` excludes variable declarations like `const ternary:
  // Ternary = 1;` (type annotation) and bare type-literal members where the
  // value is a type identifier.
  let count = 0;
  const out = src.replace(/([{,])(\s*)ternary:\s*([^,};\n]+?)(\s*[,};])/g, (_f, head: string, ws: string, v: string, tail: string) => {
    const t = v.trim();
    if (t === 'Ternary') return `${head}${ws}ternary: ${v}${tail}`;
    count++; return `${head}${ws}ternary: -(${t})${tail}`;
  });
  if (count === 0) return noop(src, 'ternary-negate: no return-position ternary values found');
  return { mutated: out, applied: true, description: `ternary-negate: negated ${count} ternary value${s(count)}` };
}

function signFlip(src: string): MutationResult {
  // Object-literal `sign: 1` / `sign: -1` literals. `(?!\w)` blocks `1n`, `12`.
  const rP = /\bsign:\s*1(?!\w)/g, rN = /\bsign:\s*-1(?!\w)/g;
  const np = (src.match(rP) ?? []).length, nn = (src.match(rN) ?? []).length;
  if (np + nn === 0) return noop(src, 'sign-flip: no sign: 1 or sign: -1 literals found');
  const S = '__MUTATOR_SIGN_PLACEHOLDER__';
  let out = src.replace(rP, `sign: ${S}`).replace(rN, 'sign: 1');
  out = out.replace(new RegExp(`sign:\\s*${S}`, 'g'), 'sign: -1');
  return { mutated: out, applied: true, description: `sign-flip: 'sign: 1' <-> 'sign: -1' (${np + nn} occurrence${s(np + nn)})` };
}

export function applyMutation(src: string, m: MutationName): MutationResult {
  if (m === 'op-swap') return opSwap(src);
  if (m === 'rnd-swap') return rndSwap(src);
  if (m === 'ternary-negate') return ternaryNegate(src);
  return signFlip(src);
}
export function listApplicableMutations(src: string): MutationName[] {
  return MUTATIONS.filter((m) => applyMutation(src, m).applied);
}

async function main(argv: readonly string[]): Promise<number> {
  const usage = () => { process.stderr.write(`usage: bun mutators.ts apply --input <p> --output <p> --mutation <name>\n       bun mutators.ts list --input <p>\n       mutations: ${MUTATIONS.join(', ')}\n`); return 2; };
  const [cmd, ...rest] = argv;
  const a = new Map<string, string>();
  for (let i = 0; i < rest.length; i += 2) {
    const k = rest[i], v = rest[i + 1];
    if (!k || !k.startsWith('--') || v === undefined) return usage();
    a.set(k.slice(2), v);
  }
  if (cmd === 'apply') {
    const inp = a.get('input'), outp = a.get('output'), mut = a.get('mutation') as MutationName | undefined;
    if (!inp || !outp || !mut || !MUTATIONS.includes(mut)) return usage();
    const r = applyMutation(await Bun.file(inp).text(), mut);
    await Bun.write(outp, r.mutated);
    process.stdout.write(r.description + '\n');
    return r.applied ? 0 : 3;
  }
  if (cmd === 'list') {
    const inp = a.get('input');
    if (!inp) return usage();
    for (const m of listApplicableMutations(await Bun.file(inp).text())) process.stdout.write(m + '\n');
    return 0;
  }
  return usage();
}
if (import.meta.main) main(process.argv.slice(2)).then((c) => process.exit(c));
