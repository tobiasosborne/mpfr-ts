/**
 * value_codec.test.ts — unit tests for the string-passthrough scalar arm.
 *
 * Run with: `bun test eval/harness/value_codec.test.ts`.
 *
 * Why these tests exist (the WHY, not the WHAT):
 *
 *   The codec previously threw on any string output that didn't match a
 *   numeric/sentinel branch. That blocked `mpfr_buildopt_tune_case` (returns
 *   the literal `"default"`) and `mpfr_fdump` (multi-line dump). Closing
 *   that gap requires (a) an opaque-string passthrough as the LAST branch
 *   in decodeExpectedOutput's string arm, and (b) a matching string arm in
 *   compareOutput's scalar switch. The earlier branches (decimal-integer,
 *   NaN/+-Infinity, finite-double) MUST keep taking precedence — the new
 *   branch is purely additive, so regression tests for those are part of
 *   this file too.
 *
 * Ref: CLAUDE.md Rule 5 (port-and-verify TDD), Rule 7 (golden coverage).
 */

import { describe, expect, it } from 'bun:test';

import { compareOutput, decodeExpectedOutput } from './value_codec.ts';

// ---------------------------------------------------------------------------
// Group 1 — decodeExpectedOutput string passthrough
// ---------------------------------------------------------------------------

describe('decodeExpectedOutput string passthrough', () => {
  it('decodes opaque string "default" as a scalar string', () => {
    const result = decodeExpectedOutput('default');
    expect(result).toEqual({ kind: 'scalar', value: 'default' });
  });

  it('decodes a multi-line string with embedded newlines', () => {
    const raw = 'line1\nline2\n';
    const result = decodeExpectedOutput(raw);
    expect(result).toEqual({ kind: 'scalar', value: 'line1\nline2\n' });
  });

  it('decimal-integer string "42" still decodes as bigint (precedence guard)', () => {
    const result = decodeExpectedOutput('42');
    expect(result).toEqual({ kind: 'scalar', value: 42n });
  });

  it('sentinel string "NaN" still decodes as Number.NaN (precedence guard)', () => {
    const result = decodeExpectedOutput('NaN');
    // Object.is needed because NaN !== NaN under ===.
    expect(result.kind).toBe('scalar');
    if (result.kind === 'scalar') {
      expect(typeof result.value).toBe('number');
      expect(Number.isNaN(result.value as number)).toBe(true);
    }
  });
});

// ---------------------------------------------------------------------------
// Group 2 — compareOutput string equality
// ---------------------------------------------------------------------------

describe('compareOutput string equality', () => {
  it('returns null when actual matches expected string', () => {
    const expected = decodeExpectedOutput('default');
    expect(compareOutput('default', expected)).toBeNull();
  });

  it('returns a mismatch message containing both expected and actual strings', () => {
    const expected = decodeExpectedOutput('default');
    const err = compareOutput('other', expected);
    expect(err).not.toBeNull();
    expect(err).toContain('"default"');
    expect(err).toContain('"other"');
  });

  it('returns a type-mismatch message when actual is a number', () => {
    const expected = decodeExpectedOutput('foo');
    const err = compareOutput(42, expected);
    expect(err).not.toBeNull();
    expect(err).toContain('got number');
  });
});
