/**
 * mutators.test.ts - tests for mutators.ts.
 *
 * Each mutator: positive (applies + transpiles), negative (no pattern ->
 * applied=false), plus defensive case. Round-trip validity via Bun.Transpiler.
 */

import { describe, expect, it } from 'bun:test';
import { applyMutation, listApplicableMutations } from './mutators.ts';

function transpiles(src: string): boolean {
  try { new Bun.Transpiler({ loader: 'ts' }).transformSync(src); return true; } catch { return false; }
}

describe('op-swap', () => {
  it('swaps mpfr_add( -> mpfr_sub(, leaves no-paren imports intact', () => {
    const src = `import { mpfr_add } from './add.ts';\nexport function f(a, b) { return mpfr_add(a, b); }\n`;
    const out = applyMutation(src, 'op-swap');
    expect(out.applied).toBe(true);
    expect(out.mutated).toContain('mpfr_sub(');
    expect(out.mutated).toContain("import { mpfr_add } from './add.ts'");
    expect(transpiles(out.mutated)).toBe(true);
  });
  it('swaps mpfr_mul( -> mpfr_div( when add/sub absent', () => {
    const out = applyMutation(`const r = mpfr_mul(x, y);\nconst z = mpfr_mul(a, b);\n`, 'op-swap');
    expect(out.applied).toBe(true);
    expect(out.mutated).toContain('mpfr_div(');
    expect(out.mutated).not.toContain('mpfr_mul(');
    expect(transpiles(out.mutated)).toBe(true);
  });
  it('applied=false when no add/sub/mul/div call', () => {
    const src = `export function noop(x: number): number { return x; }\n`;
    const out = applyMutation(src, 'op-swap');
    expect(out.applied).toBe(false);
    expect(out.mutated).toBe(src);
  });
  it('skips function declarations (export function mpfr_add(...))', () => {
    const out = applyMutation(`export function mpfr_add(a: number) { return a; }\n`, 'op-swap');
    expect(out.mutated).toContain('function mpfr_add('); expect(out.mutated).not.toContain('mpfr_sub('); expect(transpiles(out.mutated)).toBe(true);
  });
  it('still applies to call sites when declaration is present', () => {
    const out = applyMutation(`export function mpfr_add(a: number) { return a; }\nconst x = mpfr_add(1);\n`, 'op-swap');
    expect(out.applied).toBe(true); expect(out.mutated).toContain('function mpfr_add(');
    expect(out.mutated).toContain('mpfr_sub(1)'); expect(transpiles(out.mutated)).toBe(true);
  });
});

describe('rnd-swap', () => {
  it("swaps single-quoted 'RNDN' -> 'RNDZ'", () => {
    const out = applyMutation(`const r = 'RNDN'; const s: string = 'RNDN';\n`, 'rnd-swap');
    expect(out.applied).toBe(true);
    expect(out.mutated).toContain("'RNDZ'");
    expect(out.mutated).not.toContain("'RNDN'");
    expect(transpiles(out.mutated)).toBe(true);
  });
  it('swaps double-quoted "RNDU" <-> "RNDD"', () => {
    const out = applyMutation(`const r = "RNDU";\n`, 'rnd-swap');
    expect(out.applied).toBe(true);
    expect(out.mutated).toContain('"RNDD"');
    expect(transpiles(out.mutated)).toBe(true);
  });
  it('applied=false when no RND string literals', () => {
    expect(applyMutation(`export const x = 42;\n`, 'rnd-swap').applied).toBe(false);
  });
});

describe('ternary-negate', () => {
  it('negates a return-position ternary literal value', () => {
    const src = `export function f() { const t = 1; return { value: 0, ternary: t }; }\n`;
    const out = applyMutation(src, 'ternary-negate');
    expect(out.applied).toBe(true);
    expect(out.mutated).not.toBe(src);
    expect(transpiles(out.mutated)).toBe(true);
  });
  it('applied=false when ternary only in type declaration', () => {
    const src = `type X = { ternary: Ternary };\nexport function f(): number { return 0; }\n`;
    expect(applyMutation(src, 'ternary-negate').applied).toBe(false);
  });
  it('skips destructuring (const { ternary: tr } = ...)', () => {
    const out = applyMutation(`function f() { const { ternary: tr } = g(); return tr; }\n`, 'ternary-negate');
    expect(out.mutated).not.toContain('ternary: -(tr)'); expect(transpiles(out.mutated)).toBe(true);
  });
  it('still applies to object-literal returns when mixed with destructuring', () => {
    const out = applyMutation(`function f() { const { ternary: tr } = g(); return { value: 0, ternary: tr }; }\n`, 'ternary-negate');
    expect(out.applied).toBe(true); expect(out.mutated).toContain('ternary: -(tr)');
    expect(out.mutated).toContain('{ ternary: tr } = g()'); expect(transpiles(out.mutated)).toBe(true);
  });
});

describe('sign-flip', () => {
  it('flips sign: 1 <-> sign: -1 bidirectionally', () => {
    const out = applyMutation(`const a = { sign: 1, prec: 53n };\nconst b = { sign: -1, prec: 53n };\n`, 'sign-flip');
    expect(out.applied).toBe(true);
    expect(out.mutated).toContain('sign: -1, prec: 53n }');
    expect(out.mutated).toContain('sign: 1, prec: 53n }');
    expect(transpiles(out.mutated)).toBe(true);
  });
  it('does NOT match sign: identifier', () => {
    const src = `const r = { sign: signBit, prec: 53n };\n`;
    const out = applyMutation(src, 'sign-flip');
    expect(out.applied).toBe(false);
    expect(out.mutated).toBe(src);
  });
  it('does NOT match sign: 1n (bigint suffix)', () => {
    expect(applyMutation(`const r = { sign: 1n, prec: 53n };\n`, 'sign-flip').applied).toBe(false);
  });
});

describe('bigint-bump', () => {
  it('bumps the FIRST bigint literal whose value is >= 2 by 1', () => {
    const out = applyMutation(`const p = 53n; const q = 64n;\n`, 'bigint-bump');
    expect(out.applied).toBe(true);
    expect(out.mutated).toContain('54n');
    expect(out.mutated).toContain('64n');  // second occurrence untouched
    expect(transpiles(out.mutated)).toBe(true);
  });
  it('skips 0n and 1n; applied=false when only those present', () => {
    const src = `const a = 0n; const b = 1n; const c = 1n;\n`;
    const out = applyMutation(src, 'bigint-bump');
    expect(out.applied).toBe(false);
    expect(out.mutated).toBe(src);
  });
  it('applied=false on a file with no bigint literals', () => {
    expect(applyMutation(`export const x = 42;\n`, 'bigint-bump').applied).toBe(false);
  });
});

describe('comparison-swap', () => {
  it('swaps < to <= and >= to > in the first applicable occurrence', () => {
    const a = applyMutation(`if (a < b) return 1;\n`, 'comparison-swap');
    expect(a.applied).toBe(true); expect(a.mutated).toContain('a <= b'); expect(transpiles(a.mutated)).toBe(true);
    const b = applyMutation(`if (x >= y) return 0;\n`, 'comparison-swap');
    expect(b.applied).toBe(true); expect(b.mutated).toContain('x > y'); expect(transpiles(b.mutated)).toBe(true);
  });
  it('does NOT match generic type params like Map<K, V>', () => {
    const src = `const m: Map<K, V> = new Map<K, V>(); const a: Array<T> = [];\n`;
    expect(applyMutation(src, 'comparison-swap').applied).toBe(false);
  });
  it('applied=false on a file with no comparisons', () => {
    expect(applyMutation(`export const x = 42;\n`, 'comparison-swap').applied).toBe(false);
  });
});

describe('shift-direction-swap', () => {
  it('swaps >> and << bidirectionally across the whole file', () => {
    const src = `const a = x >> 5n; const b = y << 3n; const c = z >> 1n;\n`;
    const out = applyMutation(src, 'shift-direction-swap');
    expect(out.applied).toBe(true);
    expect(out.mutated).toContain('x << 5n');
    expect(out.mutated).toContain('y >> 3n');
    expect(out.mutated).toContain('z << 1n');
    expect(transpiles(out.mutated)).toBe(true);
  });
  it('swaps >>= and <<= compound assignments too', () => {
    const out = applyMutation(`let h = 0n; h >>= 4n; h <<= 2n;\n`, 'shift-direction-swap');
    expect(out.applied).toBe(true);
    expect(out.mutated).toContain('h <<= 4n');
    expect(out.mutated).toContain('h >>= 2n');
    expect(transpiles(out.mutated)).toBe(true);
  });
  it('applied=false on a file with no shifts', () => {
    expect(applyMutation(`export const x = 42;\n`, 'shift-direction-swap').applied).toBe(false);
  });
});

describe('listApplicableMutations', () => {
  it('returns only mutations whose patterns are present', () => {
    const list = listApplicableMutations(`export function f() { const r = mpfr_add(1, 2); return { value: r, sign: 1 }; }\n`);
    expect(list).toContain('op-swap');
    expect(list).toContain('sign-flip');
    expect(list).not.toContain('rnd-swap');
    expect(list).not.toContain('ternary-negate');
  });
  it('includes new mutations when their patterns are present', () => {
    const list = listApplicableMutations(`const p = 53n; if (a < b) { return x >> 1n; }\n`);
    expect(list).toContain('bigint-bump');
    expect(list).toContain('comparison-swap');
    expect(list).toContain('shift-direction-swap');
  });
});
