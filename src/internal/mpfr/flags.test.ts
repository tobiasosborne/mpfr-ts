/**
 * internal/mpfr/flags.test.ts — semantics tests for the flag register.
 *
 * Run with: `bun test src/internal/mpfr/flags.test.ts`.
 *
 * Tests use Bun's built-in test runner (no third-party dep, see Rule 12).
 * Each test calls `clearFlags()` first so module-level state from a
 * previous test never leaks.
 */
import { describe, expect, it, beforeEach } from 'bun:test';

import {
  MPFR_FLAGS_UNDERFLOW,
  MPFR_FLAGS_OVERFLOW,
  MPFR_FLAGS_NAN,
  MPFR_FLAGS_INEXACT,
  MPFR_FLAGS_ERANGE,
  MPFR_FLAGS_DIVBY0,
  MPFR_FLAGS_ALL,
  getFlags,
  setFlags,
  clearFlags,
} from './flags.ts';

describe('MPFR_FLAGS_* constants', () => {
  it('match /usr/include/mpfr.h L77–L82', () => {
    expect(MPFR_FLAGS_UNDERFLOW).toBe(1n);
    expect(MPFR_FLAGS_OVERFLOW).toBe(2n);
    expect(MPFR_FLAGS_NAN).toBe(4n);
    expect(MPFR_FLAGS_INEXACT).toBe(8n);
    expect(MPFR_FLAGS_ERANGE).toBe(16n);
    expect(MPFR_FLAGS_DIVBY0).toBe(32n);
  });

  it('MPFR_FLAGS_ALL is the OR of the six single bits', () => {
    expect(MPFR_FLAGS_ALL).toBe(63n);
    expect(MPFR_FLAGS_ALL).toBe(
      MPFR_FLAGS_UNDERFLOW |
        MPFR_FLAGS_OVERFLOW |
        MPFR_FLAGS_NAN |
        MPFR_FLAGS_INEXACT |
        MPFR_FLAGS_ERANGE |
        MPFR_FLAGS_DIVBY0,
    );
  });
});

describe('getFlags / setFlags / clearFlags', () => {
  beforeEach(() => clearFlags());

  it('register starts cleanable to 0n', () => {
    expect(getFlags()).toBe(0n);
  });

  it('setFlags is OR-combine, not replace', () => {
    setFlags(MPFR_FLAGS_UNDERFLOW);
    setFlags(MPFR_FLAGS_NAN);
    expect(getFlags()).toBe(MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_NAN);
  });

  it('clearFlags() with no arg clears everything', () => {
    setFlags(MPFR_FLAGS_ALL);
    clearFlags();
    expect(getFlags()).toBe(0n);
  });

  it('clearFlags(bits) clears only the named bits', () => {
    setFlags(MPFR_FLAGS_ALL);
    clearFlags(MPFR_FLAGS_NAN | MPFR_FLAGS_DIVBY0);
    expect(getFlags()).toBe(
      MPFR_FLAGS_UNDERFLOW | MPFR_FLAGS_OVERFLOW | MPFR_FLAGS_INEXACT | MPFR_FLAGS_ERANGE,
    );
  });

  it('setFlags masks bits outside MPFR_FLAGS_ALL', () => {
    setFlags(MPFR_FLAGS_OVERFLOW | (1n << 7n) | (1n << 63n));
    expect(getFlags()).toBe(MPFR_FLAGS_OVERFLOW);
  });

  it('clearFlags ignores bits outside MPFR_FLAGS_ALL', () => {
    setFlags(MPFR_FLAGS_ALL);
    clearFlags((1n << 7n) | (1n << 63n));
    expect(getFlags()).toBe(MPFR_FLAGS_ALL);
  });

  it('round-trip: clear -> set(mask) -> get == mask, for every mask in [0, 63]', () => {
    for (let m = 0n; m <= MPFR_FLAGS_ALL; m++) {
      clearFlags();
      setFlags(m);
      expect(getFlags()).toBe(m);
    }
  });

  it('setting the same bit twice is idempotent (sticky semantics)', () => {
    setFlags(MPFR_FLAGS_UNDERFLOW);
    setFlags(MPFR_FLAGS_UNDERFLOW);
    expect(getFlags()).toBe(MPFR_FLAGS_UNDERFLOW);
  });

  it('clear-of-already-clear is a no-op', () => {
    clearFlags(MPFR_FLAGS_NAN);
    expect(getFlags()).toBe(0n);
  });
});
