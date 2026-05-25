/*
 * golden_driver.c -- Golden master for GMP's mpn_add_1.
 *
 * Signature
 * ---------
 *
 *   mp_limb_t mpn_add_1(mp_limb_t       *rp,
 *                       const mp_limb_t *sp,
 *                       mp_size_t        n,
 *                       mp_limb_t        x);
 *
 * Adds the single-limb scalar x to the n-limb integer sp[0..n), writes
 * the n-limb sum to rp[0..n), and returns the carry-out (0 or 1).
 * Limb order: LITTLE-ENDIAN (sp[0] is LSB). Ref: GMP manual §8.3.
 *
 * Edge case n=0: GMP's mpn_add_1 with n=0 returns x directly as the
 * carry (no limbs to add into; x is the only "carry"). The driver
 * exercises this case explicitly.
 *
 * MPFR callers: mpfr_add1, mpfr_add1sp (significand += 1 ulp after a
 * round-up); also in mpfr/src/div.c via the division correction loop.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>","inputs":{"s":["<dec>",...],"n":<int>,"x":"<dec>"},
 *    "output":{"result":["<dec>",...],"carry":"<dec>"},"time_ns":<n>}
 *
 *   - `s`, `result`: GMP-limb arrays, decimal-string per limb;
 *     little-endian limb order.
 *   - `n`: raw JS number.
 *   - `x`, `carry`: u64 decimal string.
 *
 * Tag distribution (mined absent -- substrate carve-out):
 *
 *   happy        :  20
 *   edge         :  32  (boundary patterns; includes n=0)
 *   adversarial  :  12  (worst-case ripple)
 *   fuzz         :  56
 *   ------------ ----
 *   total        : 120
 */
#include "common.h"

#include <assert.h>

#if GMP_NUMB_BITS != 64
#  error "mpn_add_1 golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define MAX_N 64

static inline void emit_case(FILE *out,
                             const char *tag,
                             const mp_limb_t *s,
                             size_t n,
                             mp_limb_t x) {
    assert(n <= MAX_N);

    mp_limb_t result[MAX_N + 1];
    /* Pre-fill with a sentinel so a buggy GMP build that fails to
     * write to result is detected (the output JSONL would carry the
     * sentinel and the TS port wouldn't match it). */
    for (size_t i = 0; i <= MAX_N; ++i) result[i] = 0xDEADBEEFCAFEBABEULL;
    const uint64_t t0 = now_ns();
    const mp_limb_t carry = mpn_add_1(result, s, (mp_size_t)n, x);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_limbs(out, 1, "s", s, n);
    jl_kv_int  (out, 0, "n", (int)n);
    jl_kv_u64  (out, 0, "x", (uint64_t)x);
    jl_end_inputs(out);

    jl_output_begin_object(out);
    jl_kv_limbs(out, 1, "result", result, n);
    jl_kv_u64  (out, 0, "carry",  (uint64_t)carry);
    jl_output_end_object(out);

    jl_finish(out, elapsed);
}

static inline void fill_random(xs64_t *rng, mp_limb_t *dst, size_t n) {
    for (size_t i = 0; i < n; ++i) dst[i] = (mp_limb_t)xs64_next(rng);
}

static inline void fill_const(mp_limb_t *dst, size_t n, mp_limb_t v) {
    for (size_t i = 0; i < n; ++i) dst[i] = v;
}

int main(void) {
    FILE *out = stdout;
    const mp_limb_t M = ~(mp_limb_t)0;

    /* ============================================================== */
    /* happy: 20 -- n in {1..4}, varied x and random sp               */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xADD1AB1ULL);
        for (size_t n = 1; n <= 4; ++n) {
            for (int rep = 0; rep < 5; ++rep) {
                mp_limb_t s[MAX_N];
                fill_random(&rng, s, n);
                /* Bias x to mid-range and clear high-limb top bit so
                 * happy cases stay in the no-overall-carry regime. */
                s[n - 1] &= (~(mp_limb_t)0) >> 1;
                const mp_limb_t x = (mp_limb_t)xs64_next(&rng);
                emit_case(out, "happy", s, n, x);
            }
        }
    }

    /* ============================================================== */
    /* edge: 32 -- boundary patterns including n=0                    */
    /* ============================================================== */
    {
        /* n=0 cases need a valid pointer (GMP may deref even though
         * it reads zero limbs). Use a stack scratch buffer. */
        mp_limb_t scratch[1] = {0};
        /* (1) n=0, x=0  -- empty result, carry=0 */
        emit_case(out, "edge", scratch, 0, (mp_limb_t)0);
        /* (2) n=0, x=1  -- carry=1 */
        emit_case(out, "edge", scratch, 0, (mp_limb_t)1);
        /* (3) n=0, x=M  -- carry=M */
        emit_case(out, "edge", scratch, 0, M);

        /* (4) n=1, s=[0], x=0  -- result=[0], carry=0 */
        { mp_limb_t s[1] = {0}; emit_case(out, "edge", s, 1, 0); }
        /* (5) n=1, s=[0], x=1  -- result=[1], carry=0 */
        { mp_limb_t s[1] = {0}; emit_case(out, "edge", s, 1, 1); }
        /* (6) n=1, s=[M], x=0  -- result=[M], carry=0 */
        { mp_limb_t s[1] = {M}; emit_case(out, "edge", s, 1, 0); }
        /* (7) n=1, s=[M], x=1  -- result=[0], carry=1 */
        { mp_limb_t s[1] = {M}; emit_case(out, "edge", s, 1, 1); }
        /* (8) n=1, s=[M], x=M  -- result=[M-1], carry=1 */
        { mp_limb_t s[1] = {M}; emit_case(out, "edge", s, 1, M); }
        /* (9) n=1, s=[M/2], x=M/2+1  -- result=[M], carry=0 */
        { mp_limb_t s[1] = {M/2}; emit_case(out, "edge", s, 1, (M/2)+1); }
        /* (10) n=1, s=[1<<63], x=1<<63  -- result=[0], carry=1 */
        { mp_limb_t s[1] = {(mp_limb_t)1 << 63};
          emit_case(out, "edge", s, 1, (mp_limb_t)1 << 63); }

        /* (11) n=2, s=[0,0], x=0  -- result=[0,0], carry=0 */
        { mp_limb_t s[2] = {0,0}; emit_case(out, "edge", s, 2, 0); }
        /* (12) n=2, s=[M,0], x=1  -- result=[0,1], carry=0 (carry to limb 1) */
        { mp_limb_t s[2] = {M,0}; emit_case(out, "edge", s, 2, 1); }
        /* (13) n=2, s=[M,M], x=1  -- result=[0,0], carry=1 (full ripple) */
        { mp_limb_t s[2] = {M,M}; emit_case(out, "edge", s, 2, 1); }
        /* (14) n=2, s=[M,M], x=0  -- result=[M,M], carry=0 */
        { mp_limb_t s[2] = {M,M}; emit_case(out, "edge", s, 2, 0); }
        /* (15) n=2, s=[M,M], x=M  -- result=[M-1,M], carry=1 */
        { mp_limb_t s[2] = {M,M}; emit_case(out, "edge", s, 2, M); }
        /* (16) n=2, s=[M-1,M], x=1  -- result=[M,M], carry=0 (no ripple) */
        { mp_limb_t s[2] = {M-1,M}; emit_case(out, "edge", s, 2, 1); }
        /* (17) n=2, s=[M,M-1], x=1  -- result=[0,M], carry=0 (single-limb ripple) */
        { mp_limb_t s[2] = {M,M-1}; emit_case(out, "edge", s, 2, 1); }
        /* (18) n=2, s=[0,M], x=1  -- result=[1,M], carry=0 */
        { mp_limb_t s[2] = {0,M}; emit_case(out, "edge", s, 2, 1); }

        /* (19) n=3, s=[0,0,0], x=0  -- result=[0,0,0], carry=0 */
        { mp_limb_t s[3] = {0,0,0}; emit_case(out, "edge", s, 3, 0); }
        /* (20) n=3, s=[M,M,M], x=1  -- result=[0,0,0], carry=1 */
        { mp_limb_t s[3] = {M,M,M}; emit_case(out, "edge", s, 3, 1); }
        /* (21) n=3, s=[M,M,0], x=1  -- result=[0,0,1], carry=0 */
        { mp_limb_t s[3] = {M,M,0}; emit_case(out, "edge", s, 3, 1); }
        /* (22) n=3, s=[M,0,M], x=1  -- result=[0,1,M], carry=0 (stops middle) */
        { mp_limb_t s[3] = {M,0,M}; emit_case(out, "edge", s, 3, 1); }
        /* (23) n=3, s=[0,0,M], x=1  -- result=[1,0,M], carry=0 */
        { mp_limb_t s[3] = {0,0,M}; emit_case(out, "edge", s, 3, 1); }
        /* (24) n=3, s=[0,M,M], x=M  -- result=[M,M,M], carry=0 */
        { mp_limb_t s[3] = {0,M,M}; emit_case(out, "edge", s, 3, M); }
        /* (25) n=3, s=[M,M,M-1], x=1  -- result=[0,0,M], carry=0 */
        { mp_limb_t s[3] = {M,M,M-1}; emit_case(out, "edge", s, 3, 1); }

        /* (26) n=4, all-zeros + 1  -- result=[1,0,0,0], carry=0 */
        { mp_limb_t s[4] = {0,0,0,0}; emit_case(out, "edge", s, 4, 1); }
        /* (27) n=4, all-max + 1  -- result=[0,0,0,0], carry=1 */
        { mp_limb_t s[4] = {M,M,M,M}; emit_case(out, "edge", s, 4, 1); }
        /* (28) n=4, alternating bit pattern + M */
        { mp_limb_t s[4] = {0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL};
          emit_case(out, "edge", s, 4, M); }
        /* (29) n=4, [M,M,M,0] + 1  -- result=[0,0,0,1], carry=0 */
        { mp_limb_t s[4] = {M,M,M,0}; emit_case(out, "edge", s, 4, 1); }
        /* (30) n=4, [M,M,0,M] + 1  -- result=[0,0,1,M], carry=0 */
        { mp_limb_t s[4] = {M,M,0,M}; emit_case(out, "edge", s, 4, 1); }
        /* (31) n=4, single-bit set in low limb, x=MSB */
        { mp_limb_t s[4] = {1,0,0,0};
          emit_case(out, "edge", s, 4, (mp_limb_t)1 << 63); }
        /* (32) n=4, large random pattern + M */
        { mp_limb_t s[4] = {0x123456789ABCDEF0ULL, 0xFEDCBA9876543210ULL,
                            0x0F0F0F0F0F0F0F0FULL, 0xF0F0F0F0F0F0F0F0ULL};
          emit_case(out, "edge", s, 4, M); }
    }

    /* ============================================================== */
    /* adversarial: 12 -- worst-case ripples + max-x patterns         */
    /* ============================================================== */
    {
        /* (1) n=8, all-ones + 1: full 8-limb ripple */
        { mp_limb_t s[8]; fill_const(s, 8, M);
          emit_case(out, "adversarial", s, 8, 1); }
        /* (2) n=16, all-ones + 1: full 16-limb ripple */
        { mp_limb_t s[16]; fill_const(s, 16, M);
          emit_case(out, "adversarial", s, 16, 1); }
        /* (3) n=32, all-ones + 1: full 32-limb ripple */
        { mp_limb_t s[32]; fill_const(s, 32, M);
          emit_case(out, "adversarial", s, 32, 1); }
        /* (4) n=8, all-ones + M: full ripple, carry=1, result=[M-1,M,..,M] */
        { mp_limb_t s[8]; fill_const(s, 8, M);
          emit_case(out, "adversarial", s, 8, M); }
        /* (5) n=8, [M,M,M,M,0,0,0,0] + 1: 4-limb partial ripple */
        { mp_limb_t s[8] = {M,M,M,M,0,0,0,0};
          emit_case(out, "adversarial", s, 8, 1); }
        /* (6) n=8, [0,0,0,0,M,M,M,M] + 1: low limb increments, no ripple */
        { mp_limb_t s[8] = {0,0,0,0,M,M,M,M};
          emit_case(out, "adversarial", s, 8, 1); }
        /* (7) n=16, [M repeated 8 times, 0 repeated 8]: middle-of-array carry stop */
        { mp_limb_t s[16] = {M,M,M,M,M,M,M,M, 0,0,0,0,0,0,0,0};
          emit_case(out, "adversarial", s, 16, 1); }
        /* (8) n=16, MSB-set pattern across all limbs */
        { mp_limb_t s[16]; fill_const(s, 16, (mp_limb_t)1 << 63);
          emit_case(out, "adversarial", s, 16, (mp_limb_t)1 << 63); }
        /* (9) n=32 random, x=M */
        { xs64_t rng; xs64_seed(&rng, 0xADD1ADBEEDULL);
          mp_limb_t s[32]; fill_random(&rng, s, 32);
          emit_case(out, "adversarial", s, 32, M); }
        /* (10) n=64, all-ones + 1: max-size ripple */
        { mp_limb_t s[64]; fill_const(s, 64, M);
          emit_case(out, "adversarial", s, 64, 1); }
        /* (11) n=64, all-zeros + M: just place x in s[0] */
        { mp_limb_t s[64]; fill_const(s, 64, 0);
          emit_case(out, "adversarial", s, 64, M); }
        /* (12) n=1, MSB s, x=MSB: overflow-by-1 */
        { mp_limb_t s[1] = {(mp_limb_t)1 << 63};
          emit_case(out, "adversarial", s, 1, (mp_limb_t)1 << 63); }
    }

    /* ============================================================== */
    /* fuzz: 56 -- PRNG-seeded, n in {1, 2, 4, 8, 16, 32}             */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xF022ADD1EE0ULL);
        const size_t ns[] = {1, 2, 4, 8, 16, 32};
        for (int rep = 0; rep < 56; ++rep) {
            const size_t n = ns[(size_t)xs64_below(&rng, 6)];
            mp_limb_t s[MAX_N];
            fill_random(&rng, s, n);
            const mp_limb_t x = (mp_limb_t)xs64_next(&rng);
            emit_case(out, "fuzz", s, n, x);
        }
    }

    return 0;
}
