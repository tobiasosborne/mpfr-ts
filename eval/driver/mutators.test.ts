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

describe('listApplicableMutations', () => {
  it('returns only mutations whose patterns are present', () => {
    const list = listApplicableMutations(`export function f() { const r = mpfr_add(1, 2); return { value: r, sign: 1 }; }\n`);
    expect(list).toContain('op-swap');
    expect(list).toContain('sign-flip');
    expect(list).not.toContain('rnd-swap');
    expect(list).not.toContain('ternary-negate');
  });
});
