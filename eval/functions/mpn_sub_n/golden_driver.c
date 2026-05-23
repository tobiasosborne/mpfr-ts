/*
 * golden_driver.c — Golden master for GMP's mpn_sub_n.
 *
 * Signature
 * ---------
 *
 *   mp_limb_t mpn_sub_n(mp_limb_t *rp,
 *                       const mp_limb_t *s1p,
 *                       const mp_limb_t *s2p,
 *                       mp_size_t        n);
 *
 * Computes the n-limb difference `s1p[0..n) - s2p[0..n)`, writes it to
 * `rp[0..n)`, and returns the borrow-out limb (0 or 1). Limb arrays are
 * stored LITTLE-ENDIAN by limb index: `s1p[0]` is the least-significant
 * 2^64 word, `s1p[n-1]` is the most-significant. The routine does NOT
 * require `s1 >= s2`; if `s1 < s2` (unsigned), the result limbs are the
 * n-limb two's-complement representation of the (negative) difference,
 * and the returned borrow is 1.
 *
 * Ref: GMP manual §8.3 — "The least significant limb is stored at the
 *   lowest address (i.e. limbs[0])."
 *
 * MPFR uses mpn_sub_n extensively for same-exponent significand
 * subtraction; see mpfr/src/sub1sp.c L1561:
 *
 *   mpn_sub_n (ap, bp, cp, n);
 *
 * which is precisely the contract this golden exercises (with the
 * caller's |b| > |c| invariant ensuring the discarded borrow is zero —
 * but the routine itself works on the unconstrained inputs we test
 * here, including the `s1 < s2` underflow cases).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>","inputs":{"s1":["<dec>",...],
 *                              "s2":["<dec>",...],
 *                              "n":<int>},
 *    "output":{"result":["<dec>",...],"borrow":"<dec>"},
 *    "time_ns":<n>}
 *
 *   - `s1`, `s2`, `result`: GMP-limb arrays, decimal-string per limb (so
 *     the TS side can BigInt() them losslessly); little-endian limb order.
 *   - `n`: raw JS number — width fits comfortably in 32 bits for every
 *     case here (max 32). The runner's value_codec.ts decodes `n` as a
 *     number because it's emitted via `jl_kv_int`, matching the TS port
 *     signature `(s1, s2, n: number) -> { result, borrow }`.
 *   - `borrow`: u64 decimal string (always 0 or 1 in practice, but typed
 *     as mp_limb_t in C and as bigint in TS for consistency).
 *
 * Tag distribution (mined absent — `grep -rn mpn_sub_n mpfr/tests/`
 * returns no direct mpn_sub_n test driver; every mpfr/src/ call site
 * exercises it indirectly through MPFR-level subtraction ops, which is
 * not isolatable as an mpn-only golden):
 *
 *   happy        :  25  (small n in {1..5}, generic random inputs with
 *                        per-limb s1[i] >= s2[i] bias to keep happy in
 *                        the no-borrow regime)
 *   edge         :  32  (boundary patterns; n in {1, 2, 3, 4})
 *   adversarial  :  15  (worst-case borrow-chain stress; n in {2..16})
 *   fuzz         :  80  (PRNG-driven; n in {1, 2, 4, 8, 16, 32}, no
 *                        ordering bias — exercises both s1>s2 and s1<s2)
 *   ------------ ----
 *   total        : 152
 *
 * Per-class PRNG seeds are distinct so a single failing fuzz case can
 * be reproduced without re-running the happy cases. Seeds are
 * documented at each block. Seeds DIFFER from the mpn_add_n driver so
 * cross-function coverage is broader than "the same numbers under a
 * different op".
 *
 * Build
 * -----
 *
 *   gcc -O2 -std=c11 -Wall -Wextra -Werror -pedantic -I../../golden_master \
 *       golden_driver.c $(pkg-config --cflags --libs mpfr) -lgmp -lm \
 *       -o golden_driver
 *
 *   The repo-wide build.sh (eval/golden_master/build.sh) finds this file
 *   automatically and uses the same flags (minus -pedantic, which we add
 *   here as belt-and-braces).
 *
 * Reproducibility
 * ---------------
 *
 * The driver is deterministic: every PRNG is seeded with a hardcoded
 * constant. Re-running produces a byte-identical golden.jsonl. The
 * runtime check below also asserts GMP_NUMB_BITS == 64 so a future port
 * to a non-x86_64 architecture fails loudly rather than silently
 * emitting 32-bit limbs the TS port doesn't expect.
 */
#include "common.h"

#include <assert.h>

/* ------------------------------------------------------------------ */
/* Compile-time / runtime invariants                                  */
/* ------------------------------------------------------------------ */

/* mp_limb_t must be exactly 64 bits for the goldens to round-trip
 * losslessly through the TS port (which packs limbs into BigInt at 64
 * bits per word). Checked at compile time. */
#if GMP_NUMB_BITS != 64
#  error "mpn_sub_n golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Cap on n. Largest case the driver uses is n=32 (fuzz). The fixed
 * stack size below is 64 — double the cap so subagents experimenting
 * with the driver can bump cases without overflowing. */
#define MAX_N 64

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Emit one case. Computes mpn_sub_n into `result`, captures the borrow,
 * times the call, and writes a single JSONL record to `out`.
 *
 * Static-inline keeps every case-emit site compact at the cost of one
 * extra inlined copy of the helper per use — negligible at -O2. */
static inline void emit_case(FILE *out,
                             const char *tag,
                             const mp_limb_t *s1,
                             const mp_limb_t *s2,
                             size_t n) {
    assert(n >= 1 && n <= MAX_N);

    mp_limb_t result[MAX_N];
    const uint64_t t0 = now_ns();
    const mp_limb_t borrow = mpn_sub_n(result, s1, s2, (mp_size_t)n);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_limbs(out, 1, "s1", s1, n);
    jl_kv_limbs(out, 0, "s2", s2, n);
    jl_kv_int  (out, 0, "n",  (int)n);
    jl_end_inputs(out);

    jl_output_begin_object(out);
    jl_kv_limbs(out, 1, "result", result, n);
    jl_kv_u64  (out, 0, "borrow", (uint64_t)borrow);
    jl_output_end_object(out);

    jl_finish(out, elapsed);
}

/* Fill `dst[0..n)` with PRNG limbs. */
static inline void fill_random(xs64_t *rng, mp_limb_t *dst, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        dst[i] = (mp_limb_t)xs64_next(rng);
    }
}

/* Fill `dst[0..n)` with a constant value. */
static inline void fill_const(mp_limb_t *dst, size_t n, mp_limb_t v) {
    for (size_t i = 0; i < n; ++i) dst[i] = v;
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 25 cases — n in {1..5}, 5 random inputs per n           */
    /*                                                                */
    /* Generic, non-pathological PRNG inputs. Biased so each per-limb */
    /* `s1[i] >= s2[i]` to keep the happy class in the no-borrow      */
    /* regime; borrow-out dynamics are exercised explicitly in        */
    /* edge/adversarial/fuzz. The bias is per-limb (not per-multi-    */
    /* precision-value), so the result is correct subtraction with    */
    /* every intermediate borrow_in equal to zero.                    */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5DB1AC7FEEDULL);
        for (size_t n = 1; n <= 5; ++n) {
            for (int rep = 0; rep < 5; ++rep) {
                mp_limb_t s1[MAX_N], s2[MAX_N];
                fill_random(&rng, s1, n);
                fill_random(&rng, s2, n);
                /* Bias toward "no overall borrow-out" by guaranteeing
                 * s1 > s2 at the most-significant limb: set the high
                 * bit of s1[n-1], clear the high bit of s2[n-1]. The
                 * high-limb inequality s1[n-1] >= MSB > s2[n-1] is
                 * enough on its own to keep the global borrow at 0,
                 * REGARDLESS of how the low limbs ripple — borrow
                 * propagation from below subtracts at most 1 from the
                 * high limb, and (MSB) - (MSB-1) - 1 = 0 with b=0. So
                 * happy-class cases stay in the happy regime while
                 * still exercising real intra-array borrow ripples;
                 * borrow-out dynamics are covered in edge/adversarial. */
                s1[n - 1] |= (mp_limb_t)1 << 63;
                s2[n - 1] &= (~(mp_limb_t)0) >> 1;
                emit_case(out, "happy", s1, s2, n);
            }
        }
    }

    /* ============================================================== */
    /* edge: 32 cases — hand-crafted boundary patterns                */
    /*                                                                */
    /* Each case is named in a comment so a grader-side failure is    */
    /* directly traceable back to the invariant being tested.         */
    /* ============================================================== */
    {
        const mp_limb_t M = ~(mp_limb_t)0;  /* UINT64_MAX */

        /* (1) n=1, 0 - 0 → 0, no borrow. */
        { mp_limb_t a[1] = {0}; mp_limb_t b[1] = {0};
          emit_case(out, "edge", a, b, 1); }
        /* (2) n=1, 1 - 0 → 1, no borrow. */
        { mp_limb_t a[1] = {1}; mp_limb_t b[1] = {0};
          emit_case(out, "edge", a, b, 1); }
        /* (3) n=1, 0 - 1 → M (= UINT64_MAX), borrow=1 (single-limb underflow). */
        { mp_limb_t a[1] = {0}; mp_limb_t b[1] = {1};
          emit_case(out, "edge", a, b, 1); }
        /* (4) n=1, M - 0 → M, no borrow. */
        { mp_limb_t a[1] = {M}; mp_limb_t b[1] = {0};
          emit_case(out, "edge", a, b, 1); }
        /* (5) n=1, M - M → 0, no borrow (exact cancellation). */
        { mp_limb_t a[1] = {M}; mp_limb_t b[1] = {M};
          emit_case(out, "edge", a, b, 1); }
        /* (6) n=1, M - 1 → M-1, no borrow. */
        { mp_limb_t a[1] = {M}; mp_limb_t b[1] = {1};
          emit_case(out, "edge", a, b, 1); }
        /* (7) n=1, 1 - M → 2, borrow=1 (boundary: result is 1 - M + 2^64 = 2). */
        { mp_limb_t a[1] = {1}; mp_limb_t b[1] = {M};
          emit_case(out, "edge", a, b, 1); }
        /* (8) n=1, MSB - MSB → 0, no borrow. */
        { mp_limb_t a[1] = {(mp_limb_t)1 << 63};
          mp_limb_t b[1] = {(mp_limb_t)1 << 63};
          emit_case(out, "edge", a, b, 1); }

        /* (9) n=2, {0,0} - {0,0} → {0,0}, no borrow. */
        { mp_limb_t a[2] = {0,0}; mp_limb_t b[2] = {0,0};
          emit_case(out, "edge", a, b, 2); }
        /* (10) n=2, {0,1} - {1,0} → {M, 0}, no borrow (intra-limb borrow
         *      from high into low; net result is positive, borrow-out=0). */
        { mp_limb_t a[2] = {0,1}; mp_limb_t b[2] = {1,0};
          emit_case(out, "edge", a, b, 2); }
        /* (11) n=2, {0,0} - {0,1} → {0, M}, borrow=1 (top-only underflow). */
        { mp_limb_t a[2] = {0,0}; mp_limb_t b[2] = {0,1};
          emit_case(out, "edge", a, b, 2); }
        /* (12) n=2, {0,0} - {1,0} → {M, M}, borrow=1 (max-ripple
         *      borrow: subtract 1 from zero, ripples through every
         *      limb). */
        { mp_limb_t a[2] = {0,0}; mp_limb_t b[2] = {1,0};
          emit_case(out, "edge", a, b, 2); }
        /* (13) n=2, {M,M} - {M,M} → {0,0}, no borrow (cancellation). */
        { mp_limb_t a[2] = {M,M}; mp_limb_t b[2] = {M,M};
          emit_case(out, "edge", a, b, 2); }
        /* (14) n=2, {M,M} - {1,0} → {M-1, M}, no borrow. */
        { mp_limb_t a[2] = {M,M}; mp_limb_t b[2] = {1,0};
          emit_case(out, "edge", a, b, 2); }
        /* (15) n=2, {0,1} - {1,1} → {M, M}, borrow=1 (low underflow,
         *      high cancellation, then borrow-out from the borrow
         *      propagation past the cancelled high limb). */
        { mp_limb_t a[2] = {0,1}; mp_limb_t b[2] = {1,1};
          emit_case(out, "edge", a, b, 2); }
        /* (16) n=2, {1,0} - {1,0} → {0,0}, no borrow. */
        { mp_limb_t a[2] = {1,0}; mp_limb_t b[2] = {1,0};
          emit_case(out, "edge", a, b, 2); }

        /* (17) n=3, all zeros minus all zeros → unchanged, no borrow. */
        { mp_limb_t a[3] = {0,0,0}; mp_limb_t b[3] = {0,0,0};
          emit_case(out, "edge", a, b, 3); }
        /* (18) n=3, maximal ripple: {0,0,0} - {1,0,0} → {M,M,M}, borrow=1. */
        { mp_limb_t a[3] = {0,0,0}; mp_limb_t b[3] = {1,0,0};
          emit_case(out, "edge", a, b, 3); }
        /* (19) n=3, "borrow stops in the middle": {0,1,M} - {1,0,0} →
         *      {M,0,M}, no borrow (low underflows, middle absorbs the
         *      borrow, top untouched). */
        { mp_limb_t a[3] = {0,1,M}; mp_limb_t b[3] = {1,0,0};
          emit_case(out, "edge", a, b, 3); }
        /* (20) n=3, borrow starts in middle: {0,0,1} - {0,1,0} →
         *      {0, M, 0}, no borrow. */
        { mp_limb_t a[3] = {0,0,1}; mp_limb_t b[3] = {0,1,0};
          emit_case(out, "edge", a, b, 3); }
        /* (21) n=3, borrow starts at top: {0,0,0} - {0,0,1} → {0,0,M},
         *      borrow=1 (no ripple — underflow only in the top limb). */
        { mp_limb_t a[3] = {0,0,0}; mp_limb_t b[3] = {0,0,1};
          emit_case(out, "edge", a, b, 3); }
        /* (22) n=3, alternating bit patterns: {M,M,M} - {0x5...,0x5...,
         *      0x5...} = {0xA...,0xA...,0xA...}, no borrow. */
        { mp_limb_t a[3] = {M,M,M};
          mp_limb_t b[3] = {0x5555555555555555ULL,
                            0x5555555555555555ULL,
                            0x5555555555555555ULL};
          emit_case(out, "edge", a, b, 3); }
        /* (23) n=3, MSB - (MSB+1): {1<<63,1<<63,1<<63} - {(1<<63)+1,
         *      1<<63, 1<<63} → {M, M, (1<<63)-1}, borrow=0 actually:
         *      low limb underflows (1<<63 - ((1<<63)+1) = -1 → M, b=1),
         *      middle 1<<63 - 1<<63 - 1 = -1 → M, b=1,
         *      top   1<<63 - 1<<63 - 1 = -1 → M, b=1. So {M,M,M}, b=1. */
        { mp_limb_t a[3] = {(mp_limb_t)1<<63,(mp_limb_t)1<<63,(mp_limb_t)1<<63};
          mp_limb_t b[3] = {((mp_limb_t)1<<63)+1,(mp_limb_t)1<<63,(mp_limb_t)1<<63};
          emit_case(out, "edge", a, b, 3); }
        /* (24) n=3, full underflow at top: {M,M,M} - {M,M,M} → {0,0,0}, no borrow. */
        { mp_limb_t a[3] = {M,M,M}; mp_limb_t b[3] = {M,M,M};
          emit_case(out, "edge", a, b, 3); }

        /* (25) n=4, all-zero identity. */
        { mp_limb_t a[4] = {0,0,0,0}; mp_limb_t b[4] = {0,0,0,0};
          emit_case(out, "edge", a, b, 4); }
        /* (26) n=4, single 1 in lowest meets single 1 in highest. */
        { mp_limb_t a[4] = {1,0,0,0}; mp_limb_t b[4] = {0,0,0,1};
          emit_case(out, "edge", a, b, 4); }
        /* (27) n=4, propagating borrow breaks mid-array. {M,0,M,M} -
         *      {0,1,0,0} = low: M-0=M no-b; second 0-1=-1→M with b=1;
         *      third M-0-1=M-1 no-b; top M-0=M. So {M,M,M-1,M} b=0. */
        { mp_limb_t a[4] = {M,0,M,M}; mp_limb_t b[4] = {0,1,0,0};
          emit_case(out, "edge", a, b, 4); }
        /* (28) n=4, full cancellation: {M,M,M,M} - {M,M,M,M} = {0,0,0,0}, b=0. */
        { mp_limb_t a[4] = {M,M,M,M}; mp_limb_t b[4] = {M,M,M,M};
          emit_case(out, "edge", a, b, 4); }
        /* (29) n=4, max ripple length: {0,0,0,0} - {1,0,0,0} =
         *      {M,M,M,M}, borrow=1. */
        { mp_limb_t a[4] = {0,0,0,0}; mp_limb_t b[4] = {1,0,0,0};
          emit_case(out, "edge", a, b, 4); }
        /* (30) n=4, ripple stops at penultimate limb: {0,0,0,1} -
         *      {1,0,0,0} = low: 0-1=-1→M, b=1; second 0-0-1=-1→M,b=1;
         *      third 0-0-1=-1→M, b=1; top 1-0-1=0, b=0. So {M,M,M,0}, b=0. */
        { mp_limb_t a[4] = {0,0,0,1}; mp_limb_t b[4] = {1,0,0,0};
          emit_case(out, "edge", a, b, 4); }
        /* (31) n=4, exactly-half-MSB pattern: each limb is the MSB
         *      only, subtracted from itself produces a pure zero. */
        { mp_limb_t a[4] = {(mp_limb_t)1<<63,(mp_limb_t)1<<63,
                            (mp_limb_t)1<<63,(mp_limb_t)1<<63};
          mp_limb_t b[4] = {(mp_limb_t)1<<63,(mp_limb_t)1<<63,
                            (mp_limb_t)1<<63,(mp_limb_t)1<<63};
          emit_case(out, "edge", a, b, 4); }
        /* (32) n=4, asymmetric: low limbs differ by a chosen amount,
         *      high limbs identical. {0xEDCBA9876543210F, 0, 0, 0} -
         *      {0x123456789ABCDEF0, 0, 0, 0} — both in low limb only,
         *      no borrow because the minuend's low limb is larger. */
        { mp_limb_t a[4] = {0xEDCBA9876543210FULL, 0, 0, 0};
          mp_limb_t b[4] = {0x123456789ABCDEF0ULL, 0, 0, 0};
          emit_case(out, "edge", a, b, 4); }
    }

    /* ============================================================== */
    /* adversarial: 15 cases — borrow-chain stress                    */
    /*                                                                */
    /* Specifically constructed to exercise long borrow chains and    */
    /* the underflow boundary (s1 < s2). These are the cases most     */
    /* likely to catch a naive port that breaks on the borrow-        */
    /* propagate fast path.                                           */
    /* ============================================================== */
    {
        const mp_limb_t M = ~(mp_limb_t)0;

        /* (1) n=2, max ripple from zero. */
        { mp_limb_t a[2]; mp_limb_t b[2];
          fill_const(a, 2, 0); b[0]=1; b[1]=0;
          emit_case(out, "adversarial", a, b, 2); }

        /* (2..5) n in {4,8,12,16}, max-length borrow ripple:
         *        {0,...,0} - {1,0,...,0} = {M,...,M}, borrow=1. */
        for (size_t n = 4; n <= 16; n += 4) {
            mp_limb_t a[MAX_N], b[MAX_N];
            fill_const(a, n, 0);
            b[0] = 1;
            for (size_t i = 1; i < n; ++i) b[i] = 0;
            emit_case(out, "adversarial", a, b, n);
        }

        /* (6..9) n in {4,8,12,16}, borrow starts in the high limb
         *        (must propagate OUT, no ripple). All-zero minuend
         *        minus single-bit-in-top subtrahend → {0,...,0,M},
         *        borrow=1. */
        for (size_t n = 4; n <= 16; n += 4) {
            mp_limb_t a[MAX_N], b[MAX_N];
            for (size_t i = 0; i < n; ++i) a[i] = 0;
            for (size_t i = 0; i < n; ++i) b[i] = 0;
            b[n - 1] = 1;
            emit_case(out, "adversarial", a, b, n);
        }

        /* (10) n=8, M everywhere - 0x5555... = 0xAAAA... everywhere, no borrow. */
        { mp_limb_t a[8], b[8];
          fill_const(a, 8, M);
          fill_const(b, 8, 0x5555555555555555ULL);
          emit_case(out, "adversarial", a, b, 8); }

        /* (11) n=8, all 0 - all M → ripple-through every limb, the
         *      result is {1, 0, 0, ..., 0} with borrow=1 (each limb
         *      0 - M - borrow_in: low 0-M=-M→1 b=1; subsequent
         *      0-M-1=-M-1→0 b=1; …; top 0-M-1=-M-1→0 b=1). So
         *      {1, 0, 0, 0, 0, 0, 0, 0}, b=1. */
        { mp_limb_t a[8], b[8];
          fill_const(a, 8, 0); fill_const(b, 8, M);
          emit_case(out, "adversarial", a, b, 8); }

        /* (12) n=16, exact underflow boundary: 0 - 1 across 16 limbs
         *      → all-M result, borrow=1. */
        { mp_limb_t a[16], b[16];
          fill_const(a, 16, 0);
          b[0] = 1; for (size_t i = 1; i < 16; ++i) b[i] = 0;
          emit_case(out, "adversarial", a, b, 16); }

        /* (13) n=16, identity subtraction at full width: M everywhere
         *      minus M everywhere → 0 everywhere, no borrow. Tests
         *      that the borrow chain is correctly NOT engaged. */
        { mp_limb_t a[16], b[16];
          fill_const(a, 16, M); fill_const(b, 16, M);
          emit_case(out, "adversarial", a, b, 16); }

        /* (14) n=8, alternating M / 0 in s1, all-1 in s2: borrow
         *      "ratchets" — at even indices M-1=M-1 no-b; at odd
         *      indices 0-1=-1→M b=1; subsequent even index M-1-1=M-2
         *      no-b; etc. Exercises every-other-limb borrow toggle. */
        { mp_limb_t a[8], b[8];
          for (size_t i = 0; i < 8; ++i) { a[i] = (i & 1) ? 0 : M; b[i] = 1; }
          emit_case(out, "adversarial", a, b, 8); }

        /* (15) n=8, borrow skips: 0 in even-indexed limbs of s1, M in
         *      odd. s2 = {1, 0, 0, 0, 0, 0, 0, 0}. Low: 0-1=-1→M, b=1;
         *      next M-0-1=M-1, b=0; rest unchanged. Tests "borrow
         *      stops at first non-zero high limb". */
        { mp_limb_t a[8] = {0, M, 0, M, 0, M, 0, M};
          mp_limb_t b[8] = {1, 0, 0, 0, 0, 0, 0, 0};
          emit_case(out, "adversarial", a, b, 8); }
    }

    /* ============================================================== */
    /* fuzz: 80 cases — PRNG-driven across n in {1,2,4,8,16,32}        */
    /*                                                                */
    /* Distinct seed from happy so a fuzz reproducer doesn't depend   */
    /* on the happy stream's state. NO ordering bias here — each      */
    /* operand is independent random, so roughly half the cases have  */
    /* s1 < s2 unsigned and exercise the underflow / borrow-out path. */
    /* n-distribution chosen to give equal weight to small (1,2,4)    */
    /* and large (8,16,32) regimes — small cases test the inner       */
    /* loop's setup, large cases test sustained iteration.            */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEE5DB7AC75ULL);
        const size_t ns[] = {1, 2, 4, 8, 16, 32};
        const size_t n_ns = sizeof ns / sizeof ns[0];

        /* 80 = 13 cases per size * 6 sizes (=78) + 2 extras at n=32.
         * The extras at large n stress the longest borrow chains. */
        for (size_t i = 0; i < n_ns; ++i) {
            const size_t n = ns[i];
            const int reps = (i == n_ns - 1) ? 15 : 13;
            for (int rep = 0; rep < reps; ++rep) {
                mp_limb_t s1[MAX_N], s2[MAX_N];
                fill_random(&rng, s1, n);
                fill_random(&rng, s2, n);
                emit_case(out, "fuzz", s1, s2, n);
            }
        }
    }

    return 0;
}
