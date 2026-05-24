/**
 * ast_check.test.ts — unit tests for the schema-violation gate.
 *
 * Run with: `bun test eval/harness/ast_check.test.ts`.
 *
 * Goals:
 *
 *   1. The locked-schema redeclaration patterns MUST NOT false-positive
 *      on the mixed type-import syntax `import { type MPFR, ... }`, nor
 *      on `import type { MPFR }`. Issue mpfr-ts-wli.
 *   2. The gate MUST still fail on a real `interface MPFR {}`,
 *      `type RoundingMode = ...`, `class MPFRError extends Error`, etc.
 *   3. A port that legitimately imports a locked type AND ALSO
 *      redeclares it lower down MUST still fail (the import-strip must
 *      not swallow body-level redeclarations).
 *   4. Cyrillic homoglyph check, `: any` check, and the
 *      requireCoreImport gate must all keep working.
 *
 * Ref: CLAUDE.md Rule 5 (port-and-verify TDD), Rule 7 (golden coverage
 *   classes); these tests are the "mutation-prove" step for ast_check.
 */

import { describe, expect, it } from 'bun:test';

import { astCheck } from './ast_check.ts';

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** A minimal body that consumes an imported `MPFR` symbol so the file
 *  reads like a real port, not just a one-line import. */
const TRIVIAL_BODY = `
export function f(x: MPFR): MPFR {
  return x;
}
`;

const PUBLIC_OPTS = { requireCoreImport: true } as const;
const SUBSTRATE_OPTS = { requireCoreImport: false } as const;

// ---------------------------------------------------------------------------
// Group 1 — Mixed type-import syntax must not be flagged
// (these fail before the fix; they are the RED cases)
// ---------------------------------------------------------------------------

describe('mixed type-import syntax (regression: mpfr-ts-wli)', () => {
  it('accepts `import { type MPFR, RoundingMode, Result } from "../core.ts"`', () => {
    const src = `import { type MPFR, RoundingMode, Result } from "../core.ts";\n${TRIVIAL_BODY}`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.errors).toEqual([]);
    expect(result.ok).toBe(true);
  });

  it('accepts `import type { MPFR } from "../core.ts"`', () => {
    const src = `import type { MPFR } from "../core.ts";\n${TRIVIAL_BODY}`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.errors).toEqual([]);
    expect(result.ok).toBe(true);
  });

  it('accepts `import { type Result, type Ternary } from "../core.ts"`', () => {
    const src =
      `import { type Result, type Ternary } from "../core.ts";\n` +
      `export function g(): Result { return null as unknown as Result; }\n`;
    const result = astCheck(src, PUBLIC_OPTS);
    // Note: this body uses `as unknown as Result` (not `as any`), so the
    // `any` gate should not fire either.
    expect(result.errors).toEqual([]);
    expect(result.ok).toBe(true);
  });

  it('accepts a multi-line mixed import block', () => {
    const src =
      `import {\n` +
      `  type MPFR,\n` +
      `  type RoundingMode,\n` +
      `  posZero,\n` +
      `} from "../core.ts";\n` +
      TRIVIAL_BODY;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.errors).toEqual([]);
    expect(result.ok).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// Group 2 — Plain (non-type) imports must continue to be accepted
// ---------------------------------------------------------------------------

describe('plain named imports (regression guard)', () => {
  it('accepts `import { MPFR } from "../core.ts"`', () => {
    const src = `import { MPFR } from "../core.ts";\n${TRIVIAL_BODY}`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.errors).toEqual([]);
    expect(result.ok).toBe(true);
  });

  it('accepts the split-import workaround `import type` + `import { value }`', () => {
    const src =
      `import type { MPFR } from "../core.ts";\n` +
      `import { posZero } from "../core.ts";\n` +
      TRIVIAL_BODY;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.errors).toEqual([]);
    expect(result.ok).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// Group 3 — Real redeclarations MUST still be rejected
// ---------------------------------------------------------------------------

describe('locked-schema redeclarations (gate must not weaken)', () => {
  it('rejects `interface MPFR { ... }`', () => {
    const src =
      `import { posZero } from "../core.ts";\n` +
      `interface MPFR { kind: string }\n` +
      `export function f(): unknown { return posZero(53n); }\n`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.ok).toBe(false);
    expect(result.errors.some((e) => e.includes("'MPFR'"))).toBe(true);
  });

  it('rejects `type RoundingMode = ...`', () => {
    const src =
      `import { posZero } from "../core.ts";\n` +
      `type RoundingMode = "X";\n` +
      `export function f(): unknown { return posZero(53n); }\n`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.ok).toBe(false);
    expect(result.errors.some((e) => e.includes("'RoundingMode'"))).toBe(true);
  });

  it('rejects `class MPFRError extends Error {}`', () => {
    const src =
      `import { posZero } from "../core.ts";\n` +
      `class MPFRError extends Error {}\n` +
      `export function f(): unknown { return new MPFRError(); }\n`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.ok).toBe(false);
    expect(result.errors.some((e) => e.includes("'MPFRError'"))).toBe(true);
  });

  it('rejects `type Result = ...`', () => {
    const src =
      `import { posZero } from "../core.ts";\n` +
      `type Result = { foo: number };\n` +
      `export function f(): unknown { return posZero(53n); }\n`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.ok).toBe(false);
    expect(result.errors.some((e) => e.includes("'Result'"))).toBe(true);
  });

  it('rejects `type Ternary = ...`', () => {
    const src =
      `import { posZero } from "../core.ts";\n` +
      `type Ternary = 0;\n` +
      `export function f(): unknown { return posZero(53n); }\n`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.ok).toBe(false);
    expect(result.errors.some((e) => e.includes("'Ternary'"))).toBe(true);
  });

  it('rejects `type MPFRKind = ...`', () => {
    const src =
      `import { posZero } from "../core.ts";\n` +
      `type MPFRKind = "nan";\n` +
      `export function f(): unknown { return posZero(53n); }\n`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.ok).toBe(false);
    expect(result.errors.some((e) => e.includes("'MPFRKind'"))).toBe(true);
  });

  it('rejects a file that imports MPFR AND redeclares `type MPFR = number;` lower down', () => {
    // The dangerous case: the import-strip must apply only to import
    // lines, not the entire file. A body-level redeclaration must still
    // trip the gate.
    const src =
      `import { type MPFR, posZero } from "../core.ts";\n` +
      `\n` +
      `type MPFR = number;\n` +
      `export function f(): MPFR { return 0 as MPFR; }\n` +
      `void posZero;\n`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.ok).toBe(false);
    expect(result.errors.some((e) => e.includes("'MPFR'"))).toBe(true);
  });
});

// ---------------------------------------------------------------------------
// Group 4 — Other regressions to guard
// ---------------------------------------------------------------------------

describe('other invariants', () => {
  it('flags a Cyrillic homoglyph and reports the codepoint', () => {
    // U+0430 is the Cyrillic 'а' — visually indistinguishable from
    // ASCII 'a' but a different codepoint.
    const src =
      `import { posZero } from "../core.ts";\n` +
      `// scаle\n` + // 'а' here is U+0430
      `export const x = posZero(53n);\n`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.ok).toBe(false);
    expect(result.errors.some((e) => e.includes('U+0430'))).toBe(true);
  });

  it('flags `: any` annotations', () => {
    const src =
      `import { posZero } from "../core.ts";\n` +
      `export function f(x: any): unknown { void x; return posZero(53n); }\n`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.ok).toBe(false);
    expect(result.errors.some((e) => e.includes(': any annotation'))).toBe(true);
  });

  it('flags missing core import when requireCoreImport: true', () => {
    const src = `export function f(x: bigint): bigint { return x; }\n`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.ok).toBe(false);
    expect(result.errors.some((e) => e.includes('missing required import'))).toBe(true);
  });

  it('does NOT require a core import for substrate ports', () => {
    const src = `export function add_n(a: bigint, b: bigint): bigint { return a + b; }\n`;
    const result = astCheck(src, SUBSTRATE_OPTS);
    expect(result.errors).toEqual([]);
    expect(result.ok).toBe(true);
  });

  it('records imported symbol names in coreImports', () => {
    const src = `import { type MPFR, posZero } from "../core.ts";\n${TRIVIAL_BODY}`;
    const result = astCheck(src, PUBLIC_OPTS);
    expect(result.coreImports).toEqual(['MPFR', 'posZero']);
  });
});
