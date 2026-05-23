/*
 * golden_driver.c — Golden master for GMP's mpn_add_n.
 *
 * Signature
 * ---------
 *
 *   mp_limb_t mpn_add_n(mp_limb_t *rp,
 *                       const mp_limb_t *s1p,
 *                       const mp_limb_t *s2p,
 *                       mp_size_t        n);
 *
 * Adds the two n-limb non-negative integers `s1p[0..n)` and `s2p[0..n)`,
 * writes the n-limb sum to `rp[0..n)`, and returns the carry-out limb
 * (0 or 1). Limb arrays are stored LITTLE-ENDIAN by limb index: `s1p[0]`
 * is the least-significant 2^64 word, `s1p[n-1]` is the most-significant.
 * Ref: GMP manual §8.3 — "The least significant limb is stored at the
 *   lowest address (i.e. limbs[0])."
 *
 * MPFR uses mpn_add_n extensively for same-exponent significand addition;
 * see mpfr/src/add1sp.c L921:
 *
 *   limb = mpn_add_n (ap, MPFR_MANT(b), MPFR_MANT(c), n);
 *
 * which is precisely the contract this golden exercises.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>","inputs":{"s1":["<dec>",...],
 *                              "s2":["<dec>",...],
 *                              "n":<int>},
 *    "output":{"result":["<dec>",...],"carry":"<dec>"},
 *    "time_ns":<n>}
 *
 *   - `s1`, `s2`, `result`: GMP-limb arrays, decimal-string per limb (so
 *     the TS side can BigInt() them losslessly); little-endian limb order.
 *   - `n`: raw JS number — width fits comfortably in 32 bits for every
 *     case here (max 32). The runner's value_codec.ts decodes `n` as a
 *     number because it's emitted via `jl_kv_int`, matching the TS port
 *     signature `(s1, s2, n: number) -> { result, carry }`.
 *   - `carry`: u64 decimal string (always 0 or 1 in practice, but typed
 *     as mp_limb_t in C and as bigint in TS for consistency).
 *
 * Tag distribution (mined absent — mpfr/tests/ has no direct mpn_add_n
 * tests; every mpfr/src/ call site exercises it indirectly through
 * MPFR-level ops, which is not isolatable as an mpn-only golden):
 *
 *   happy        :  25  (small n in {1..5}, generic random inputs)
 *   edge         :  32  (boundary patterns; n in {1, 2, 3, 4})
 *   adversarial  :  15  (worst-case carry-chain stress; n in {2..16})
 *   fuzz         :  80  (PRNG-driven; n in {1, 2, 4, 8, 16, 32})
 *   ------------ ----
 *   total        : 152
 *
 * Per-class PRNG seeds are distinct so a single failing fuzz case can be
 * reproduced without re-running the happy cases. Seeds are documented at
 * each block.
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
 * to a non-x86_64 architecture fails loudly rather than silently emitting
 * 32-bit limbs the TS port doesn't expect.
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
#  error "mpn_add_n golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Cap on n. Largest case the driver uses is n=32 (fuzz). The fixed
 * stack size below is 64 — double the cap so subagents experimenting
 * with the driver can bump cases without overflowing. */
#define MAX_N 64

/* ------------------------------------------------------------------ */
/* Helpers                                                            */
/* ------------------------------------------------------------------ */

/* Emit one case. Computes mpn_add_n into `result`, captures the carry,
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
    const mp_limb_t carry = mpn_add_n(result, s1, s2, (mp_size_t)n);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_limbs(out, 1, "s1", s1, n);
    jl_kv_limbs(out, 0, "s2", s2, n);
    jl_kv_int  (out, 0, "n",  (int)n);
    jl_end_inputs(out);

    jl_output_begin_object(out);
    jl_kv_limbs(out, 1, "result", result, n);
    jl_kv_u64  (out, 0, "carry",  (uint64_t)carry);
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
    /* Generic, non-pathological PRNG inputs. Exercises the common    */
    /* case where every limb is "in the middle" of u64 range and the  */
    /* carry chain has both runs and breaks.                          */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xADD0FEEDULL);
        for (size_t n = 1; n <= 5; ++n) {
            for (int rep = 0; rep < 5; ++rep) {
                mp_limb_t s1[MAX_N], s2[MAX_N];
                fill_random(&rng, s1, n);
                fill_random(&rng, s2, n);
                /* Bias toward "no overall carry-out" by clearing the
                 * top bit of the high limb of each operand. That keeps
                 * happy-class cases in the happy regime; carry-out
                 * dynamics are exercised explicitly in edge/adversarial. */
                s1[n - 1] &= (~(mp_limb_t)0) >> 1;
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

        /* (1) n=1, 0 + 0 → 0, no carry. */
        { mp_limb_t a[1] = {0}; mp_limb_t b[1] = {0};
          emit_case(out, "edge", a, b, 1); }
        /* (2) n=1, 0 + 1 → 1, no carry. */
        { mp_limb_t a[1] = {0}; mp_limb_t b[1] = {1};
          emit_case(out, "edge", a, b, 1); }
        /* (3) n=1, 1 + 0 → 1, no carry (commutativity check). */
        { mp_limb_t a[1] = {1}; mp_limb_t b[1] = {0};
          emit_case(out, "edge", a, b, 1); }
        /* (4) n=1, M + 1 → 0, carry=1 (single-limb overflow). */
        { mp_limb_t a[1] = {M}; mp_limb_t b[1] = {1};
          emit_case(out, "edge", a, b, 1); }
        /* (5) n=1, M + 0 → M, no carry. */
        { mp_limb_t a[1] = {M}; mp_limb_t b[1] = {0};
          emit_case(out, "edge", a, b, 1); }
        /* (6) n=1, M + M → M-1, carry=1 (max single-limb sum). */
        { mp_limb_t a[1] = {M}; mp_limb_t b[1] = {M};
          emit_case(out, "edge", a, b, 1); }
        /* (7) n=1, (M/2) + (M/2 + 1) → M, no carry (boundary, MSB just set). */
        { mp_limb_t a[1] = {M / 2}; mp_limb_t b[1] = {(M / 2) + 1};
          emit_case(out, "edge", a, b, 1); }
        /* (8) n=1, MSB + MSB → 0, carry=1 (two MSBs collide). */
        { mp_limb_t a[1] = {(mp_limb_t)1 << 63};
          mp_limb_t b[1] = {(mp_limb_t)1 << 63};
          emit_case(out, "edge", a, b, 1); }

        /* (9) n=2, {0,0} + {0,0} → {0,0}, no carry. */
        { mp_limb_t a[2] = {0,0}; mp_limb_t b[2] = {0,0};
          emit_case(out, "edge", a, b, 2); }
        /* (10) n=2, {M,0} + {1,0} → {0,1}, no carry (intra-limb carry). */
        { mp_limb_t a[2] = {M,0}; mp_limb_t b[2] = {1,0};
          emit_case(out, "edge", a, b, 2); }
        /* (11) n=2, {0,M} + {0,1} → {0,0}, carry=1 (overflow only in high). */
        { mp_limb_t a[2] = {0,M}; mp_limb_t b[2] = {0,1};
          emit_case(out, "edge", a, b, 2); }
        /* (12) n=2, {M,M} + {1,0} → {0,0}, carry=1 (full ripple n=2). */
        { mp_limb_t a[2] = {M,M}; mp_limb_t b[2] = {1,0};
          emit_case(out, "edge", a, b, 2); }
        /* (13) n=2, {M,M} + {M,M} → {M-1,M}, carry=1. */
        { mp_limb_t a[2] = {M,M}; mp_limb_t b[2] = {M,M};
          emit_case(out, "edge", a, b, 2); }
        /* (14) n=2, {M,0} + {M,M} → {M-1,1}, no carry. */
        { mp_limb_t a[2] = {M,0}; mp_limb_t b[2] = {M,M};
          emit_case(out, "edge", a, b, 2); }
        /* (15) n=2, {1,M} + {M,0} → {0,0}, carry=1 (intra-low overflow into
         *      max-high triggers external carry). */
        { mp_limb_t a[2] = {1,M}; mp_limb_t b[2] = {M,0};
          emit_case(out, "edge", a, b, 2); }
        /* (16) n=2, {0,1} + {0,0} → {0,1}, no carry. */
        { mp_limb_t a[2] = {0,1}; mp_limb_t b[2] = {0,0};
          emit_case(out, "edge", a, b, 2); }

        /* (17) n=3, all zeros + something → unchanged, no carry. */
        { mp_limb_t a[3] = {0,0,0}; mp_limb_t b[3] = {1,2,3};
          emit_case(out, "edge", a, b, 3); }
        /* (18) n=3, maximal ripple: {M,M,M} + {1,0,0} → {0,0,0}, carry=1. */
        { mp_limb_t a[3] = {M,M,M}; mp_limb_t b[3] = {1,0,0};
          emit_case(out, "edge", a, b, 3); }
        /* (19) n=3, "carry stops in the middle": {M,0,M}+{1,0,0}→{0,1,M}, no carry. */
        { mp_limb_t a[3] = {M,0,M}; mp_limb_t b[3] = {1,0,0};
          emit_case(out, "edge", a, b, 3); }
        /* (20) n=3, carry starts in middle: {0,M,0}+{0,1,0}→{0,0,1}, no carry. */
        { mp_limb_t a[3] = {0,M,0}; mp_limb_t b[3] = {0,1,0};
          emit_case(out, "edge", a, b, 3); }
        /* (21) n=3, carry starts at top: {0,0,M}+{0,0,1}→{0,0,0}, carry=1. */
        { mp_limb_t a[3] = {0,0,M}; mp_limb_t b[3] = {0,0,1};
          emit_case(out, "edge", a, b, 3); }
        /* (22) n=3, alternating bit patterns: {0x5...,0x5...,0x5...} +
         *      {0xA...,0xA...,0xA...} = {M,M,M}, no carry. */
        { mp_limb_t a[3] = {0x5555555555555555ULL,
                            0x5555555555555555ULL,
                            0x5555555555555555ULL};
          mp_limb_t b[3] = {0xAAAAAAAAAAAAAAAAULL,
                            0xAAAAAAAAAAAAAAAAULL,
                            0xAAAAAAAAAAAAAAAAULL};
          emit_case(out, "edge", a, b, 3); }
        /* (23) n=3, both operands all-MSB: {1<<63, 1<<63, 1<<63} +
         *      itself → {0, 1, 1<<63 ... }. Computes exact value below. */
        { mp_limb_t a[3] = {(mp_limb_t)1<<63,(mp_limb_t)1<<63,(mp_limb_t)1<<63};
          mp_limb_t b[3] = {(mp_limb_t)1<<63,(mp_limb_t)1<<63,(mp_limb_t)1<<63};
          emit_case(out, "edge", a, b, 3); }
        /* (24) n=3, carry-out from top: {M,M,M}+{M,M,M}→{M-1,M,M}, carry=1. */
        { mp_limb_t a[3] = {M,M,M}; mp_limb_t b[3] = {M,M,M};
          emit_case(out, "edge", a, b, 3); }

        /* (25) n=4, all-zero identity. */
        { mp_limb_t a[4] = {0,0,0,0}; mp_limb_t b[4] = {0,0,0,0};
          emit_case(out, "edge", a, b, 4); }
        /* (26) n=4, single 1 in lowest meets single 1 in highest. */
        { mp_limb_t a[4] = {1,0,0,0}; mp_limb_t b[4] = {0,0,0,1};
          emit_case(out, "edge", a, b, 4); }
        /* (27) n=4, propagating carry breaks mid-array. */
        { mp_limb_t a[4] = {M,M,0,M}; mp_limb_t b[4] = {1,0,0,0};
          emit_case(out, "edge", a, b, 4); }
        /* (28) n=4, full carry from top: {M,M,M,M}+{M,M,M,M}={M-1,M,M,M},c=1. */
        { mp_limb_t a[4] = {M,M,M,M}; mp_limb_t b[4] = {M,M,M,M};
          emit_case(out, "edge", a, b, 4); }
        /* (29) n=4, max ripple length: {M,M,M,M}+{1,0,0,0}={0,0,0,0},c=1. */
        { mp_limb_t a[4] = {M,M,M,M}; mp_limb_t b[4] = {1,0,0,0};
          emit_case(out, "edge", a, b, 4); }
        /* (30) n=4, ripple stops at penultimate limb. */
        { mp_limb_t a[4] = {M,M,M,0}; mp_limb_t b[4] = {1,0,0,0};
          emit_case(out, "edge", a, b, 4); }
        /* (31) n=4, exactly-half-MSB pattern: each limb is the MSB only,
         *      doubled produces a pure carry chain. */
        { mp_limb_t a[4] = {(mp_limb_t)1<<63,(mp_limb_t)1<<63,
                            (mp_limb_t)1<<63,(mp_limb_t)1<<63};
          mp_limb_t b[4] = {(mp_limb_t)1<<63,(mp_limb_t)1<<63,
                            (mp_limb_t)1<<63,(mp_limb_t)1<<63};
          emit_case(out, "edge", a, b, 4); }
        /* (32) n=4, asymmetric: low limbs sum to UINT64_MAX exactly (no
         *      carry), high limbs both 0. */
        { mp_limb_t a[4] = {0x123456789ABCDEF0ULL, 0, 0, 0};
          mp_limb_t b[4] = {0xEDCBA9876543210FULL, 0, 0, 0};
          emit_case(out, "edge", a, b, 4); }
    }

    /* ============================================================== */
    /* adversarial: 15 cases — carry-chain stress                     */
    /*                                                                */
    /* Specifically constructed to exercise long carry chains and the */
    /* overflow boundary (sum == 2^(n*64)). These are the cases most  */
    /* likely to catch a naive port that breaks on the carry-propagate */
    /* fast path.                                                     */
    /* ============================================================== */
    {
        const mp_limb_t M = ~(mp_limb_t)0;

        /* (1) n=2, max ripple. */
        { mp_limb_t a[2]; mp_limb_t b[2];
          fill_const(a, 2, M); b[0]=1; b[1]=0;
          emit_case(out, "adversarial", a, b, 2); }

        /* (2..5) n in {4,8,12,16}, max ripple (carry starts low). */
        for (size_t n = 4; n <= 16; n += 4) {
            mp_limb_t a[MAX_N], b[MAX_N];
            fill_const(a, n, M);
            b[0] = 1;
            for (size_t i = 1; i < n; ++i) b[i] = 0;
            emit_case(out, "adversarial", a, b, n);
        }

        /* (6..9) n in {4,8,12,16}, carry starts in the high limb (must
         *        propagate OUT, no ripple). */
        for (size_t n = 4; n <= 16; n += 4) {
            mp_limb_t a[MAX_N], b[MAX_N];
            for (size_t i = 0; i < n; ++i) a[i] = 0;
            a[n - 1] = M;
            for (size_t i = 0; i < n; ++i) b[i] = 0;
            b[n - 1] = 1;
            emit_case(out, "adversarial", a, b, n);
        }

        /* (10) n=8, 0x5555... + 0xAAAA... = M everywhere, no carry. */
        { mp_limb_t a[8], b[8];
          fill_const(a, 8, 0x5555555555555555ULL);
          fill_const(b, 8, 0xAAAAAAAAAAAAAAAAULL);
          emit_case(out, "adversarial", a, b, 8); }

        /* (11) n=8, all M + all M → {M-1, M, M, ..., M}, carry=1. */
        { mp_limb_t a[8], b[8];
          fill_const(a, 8, M); fill_const(b, 8, M);
          emit_case(out, "adversarial", a, b, 8); }

        /* (12) n=16, exact overflow boundary: 2^(16*64) - 1 + 1 = 2^(16*64). */
        { mp_limb_t a[16], b[16];
          fill_const(a, 16, M);
          b[0] = 1; for (size_t i = 1; i < 16; ++i) b[i] = 0;
          emit_case(out, "adversarial", a, b, 16); }

        /* (13) n=16, sum exactly hits 2^(n*64) - 1 (carry break at top). */
        { mp_limb_t a[16], b[16];
          fill_const(a, 16, M);
          fill_const(b, 16, 0);
          /* a + 0 = a, no carry — but checks the all-M result is preserved. */
          emit_case(out, "adversarial", a, b, 16); }

        /* (14) n=8, alternating M / 0 in s1, all-1 in s2 (carry "ratchets"
         *      through every-other limb). */
        { mp_limb_t a[8], b[8];
          for (size_t i = 0; i < 8; ++i) { a[i] = (i & 1) ? 0 : M; b[i] = 1; }
          emit_case(out, "adversarial", a, b, 8); }

        /* (15) n=8, carry skips: M in even-indexed limbs of s1, 1 in lowest
         *      of s2 — carry ripples through low M's, stops at first 0. */
        { mp_limb_t a[8] = {M, 0, M, 0, M, 0, M, 0};
          mp_limb_t b[8] = {1, 0, 0, 0, 0, 0, 0, 0};
          emit_case(out, "adversarial", a, b, 8); }
    }

    /* ============================================================== */
    /* fuzz: 80 cases — PRNG-driven across n in {1,2,4,8,16,32}        */
    /*                                                                */
    /* Distinct seed from happy so a fuzz reproducer doesn't depend on */
    /* the happy stream's state. n-distribution chosen to give equal   */
    /* weight to small (1,2,4) and large (8,16,32) regimes — small    */
    /* cases test the inner loop's setup, large cases test sustained   */
    /* iteration.                                                      */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xF0F1F2F3F4F5F6F7ULL);
        const size_t ns[] = {1, 2, 4, 8, 16, 32};
        const size_t n_ns = sizeof ns / sizeof ns[0];

        /* 80 = 13 cases per size * 6 sizes (=78) + 2 extras at n=32.
         * The extras at large n stress the longest carry chains. */
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
