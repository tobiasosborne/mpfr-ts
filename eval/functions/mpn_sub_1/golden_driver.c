/*
 * golden_driver.c -- Golden master for GMP's mpn_sub_1.
 *
 * Signature
 * ---------
 *
 *   mp_limb_t mpn_sub_1(mp_limb_t       *rp,
 *                       const mp_limb_t *sp,
 *                       mp_size_t        n,
 *                       mp_limb_t        x);
 *
 * Subtracts single-limb x from the n-limb integer sp, writes the
 * n-limb difference to rp, and returns the borrow-out (0 or 1).
 * Limb order: little-endian. Ref: GMP manual §8.3.
 *
 * n=0 case: GMP returns borrow=0 (x silently discarded). Mirrors
 * mpn_add_1's n=0 contract. Verified empirically.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>","inputs":{"s":["<dec>",...],"n":<int>,"x":"<dec>"},
 *    "output":{"result":["<dec>",...],"borrow":"<dec>"},"time_ns":<n>}
 *
 * Tag distribution:
 *   happy        :  20
 *   edge         :  32
 *   adversarial  :  12
 *   fuzz         :  56
 *   total        : 120
 *
 * (mined absent -- substrate carve-out)
 */
#include "common.h"

#include <assert.h>

#if GMP_NUMB_BITS != 64
#  error "mpn_sub_1 golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define MAX_N 64

static inline void emit_case(FILE *out,
                             const char *tag,
                             const mp_limb_t *s,
                             size_t n,
                             mp_limb_t x) {
    assert(n <= MAX_N);

    mp_limb_t result[MAX_N + 1];
    for (size_t i = 0; i <= MAX_N; ++i) result[i] = 0xDEADBEEFCAFEBABEULL;
    const uint64_t t0 = now_ns();
    const mp_limb_t borrow = mpn_sub_1(result, s, (mp_size_t)n, x);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_limbs(out, 1, "s", s, n);
    jl_kv_int  (out, 0, "n", (int)n);
    jl_kv_u64  (out, 0, "x", (uint64_t)x);
    jl_end_inputs(out);

    jl_output_begin_object(out);
    jl_kv_limbs(out, 1, "result", result, n);
    jl_kv_u64  (out, 0, "borrow", (uint64_t)borrow);
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

    /* happy: 20 -- n in {1..4}, varied x, sp biased to avoid full borrow */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5DB1AB1ULL);
        for (size_t n = 1; n <= 4; ++n) {
            for (int rep = 0; rep < 5; ++rep) {
                mp_limb_t s[MAX_N];
                fill_random(&rng, s, n);
                /* Set top bit of high limb so s is large enough that a
                 * random x subtraction stays in the no-borrow regime. */
                s[n - 1] |= (mp_limb_t)1 << 63;
                const mp_limb_t x = (mp_limb_t)xs64_next(&rng);
                emit_case(out, "happy", s, n, x);
            }
        }
    }

    /* edge: 32 -- boundary patterns */
    {
        mp_limb_t scratch[1] = {0};

        /* (1-3) n=0 */
        emit_case(out, "edge", scratch, 0, (mp_limb_t)0);
        emit_case(out, "edge", scratch, 0, (mp_limb_t)1);
        emit_case(out, "edge", scratch, 0, M);

        /* (4) n=1, s=[0], x=0 -- result=[0], borrow=0 */
        { mp_limb_t s[1] = {0}; emit_case(out, "edge", s, 1, 0); }
        /* (5) n=1, s=[1], x=1 -- result=[0], borrow=0 */
        { mp_limb_t s[1] = {1}; emit_case(out, "edge", s, 1, 1); }
        /* (6) n=1, s=[0], x=1 -- result=[M], borrow=1 (underflow) */
        { mp_limb_t s[1] = {0}; emit_case(out, "edge", s, 1, 1); }
        /* (7) n=1, s=[M], x=M -- result=[0], borrow=0 */
        { mp_limb_t s[1] = {M}; emit_case(out, "edge", s, 1, M); }
        /* (8) n=1, s=[M], x=0 -- result=[M], borrow=0 */
        { mp_limb_t s[1] = {M}; emit_case(out, "edge", s, 1, 0); }
        /* (9) n=1, s=[1<<63], x=(1<<63)-1 -- result=[1], borrow=0 */
        { mp_limb_t s[1] = {(mp_limb_t)1 << 63};
          emit_case(out, "edge", s, 1, ((mp_limb_t)1 << 63) - 1); }
        /* (10) n=1, s=[M/2], x=M -- result=[M/2+1], borrow=1 (M/2 - M underflows) */
        { mp_limb_t s[1] = {M/2}; emit_case(out, "edge", s, 1, M); }

        /* (11) n=2, s=[0,0], x=0 -- result=[0,0], borrow=0 */
        { mp_limb_t s[2] = {0,0}; emit_case(out, "edge", s, 2, 0); }
        /* (12) n=2, s=[0,1], x=1 -- result=[M,0], borrow=0 (limb-0 underflows, limb-1 absorbs) */
        { mp_limb_t s[2] = {0,1}; emit_case(out, "edge", s, 2, 1); }
        /* (13) n=2, s=[0,0], x=1 -- result=[M,M], borrow=1 (full borrow chain) */
        { mp_limb_t s[2] = {0,0}; emit_case(out, "edge", s, 2, 1); }
        /* (14) n=2, s=[M,M], x=M -- result=[0,M], borrow=0 */
        { mp_limb_t s[2] = {M,M}; emit_case(out, "edge", s, 2, M); }
        /* (15) n=2, s=[0,M], x=M -- result=[1+0-M,M] -> borrow detail */
        { mp_limb_t s[2] = {0,M}; emit_case(out, "edge", s, 2, M); }
        /* (16) n=2, s=[1,0], x=1 -- result=[0,0], borrow=0 */
        { mp_limb_t s[2] = {1,0}; emit_case(out, "edge", s, 2, 1); }
        /* (17) n=2, s=[1,M], x=1 -- result=[0,M], borrow=0 */
        { mp_limb_t s[2] = {1,M}; emit_case(out, "edge", s, 2, 1); }
        /* (18) n=2, s=[M,0], x=M -- result=[0,0], borrow=0 */
        { mp_limb_t s[2] = {M,0}; emit_case(out, "edge", s, 2, M); }

        /* (19) n=3, s=[0,0,0], x=0 -- result=[0,0,0], borrow=0 */
        { mp_limb_t s[3] = {0,0,0}; emit_case(out, "edge", s, 3, 0); }
        /* (20) n=3, s=[0,0,0], x=1 -- result=[M,M,M], borrow=1 */
        { mp_limb_t s[3] = {0,0,0}; emit_case(out, "edge", s, 3, 1); }
        /* (21) n=3, s=[0,0,1], x=1 -- result=[M,M,0], borrow=0 (ripple to top, stops) */
        { mp_limb_t s[3] = {0,0,1}; emit_case(out, "edge", s, 3, 1); }
        /* (22) n=3, s=[0,1,0], x=1 -- result=[M,0,0], borrow=0 (ripple stops mid) */
        { mp_limb_t s[3] = {0,1,0}; emit_case(out, "edge", s, 3, 1); }
        /* (23) n=3, s=[M,M,M], x=1 -- result=[M-1,M,M], borrow=0 */
        { mp_limb_t s[3] = {M,M,M}; emit_case(out, "edge", s, 3, 1); }
        /* (24) n=3, s=[1,0,0], x=M -- result=[2-M,M-1,M], borrow=1 */
        { mp_limb_t s[3] = {1,0,0}; emit_case(out, "edge", s, 3, M); }
        /* (25) n=3, s=[M,0,0], x=M -- result=[0,0,0], borrow=0 */
        { mp_limb_t s[3] = {M,0,0}; emit_case(out, "edge", s, 3, M); }

        /* (26-32) n=4 boundary patterns */
        { mp_limb_t s[4] = {0,0,0,0}; emit_case(out, "edge", s, 4, 1); }
        { mp_limb_t s[4] = {M,M,M,M}; emit_case(out, "edge", s, 4, 1); }
        { mp_limb_t s[4] = {1,0,0,0}; emit_case(out, "edge", s, 4, 1); }
        { mp_limb_t s[4] = {0,0,0,1}; emit_case(out, "edge", s, 4, 1); }
        { mp_limb_t s[4] = {0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL};
          emit_case(out, "edge", s, 4, M); }
        { mp_limb_t s[4] = {0,0,0,1};
          emit_case(out, "edge", s, 4, M); }
        { mp_limb_t s[4] = {M,M,M,M};
          emit_case(out, "edge", s, 4, M); }
    }

    /* adversarial: 12 -- worst-case borrow chains */
    {
        /* (1) n=8, all-zeros - 1: full borrow chain */
        { mp_limb_t s[8]; fill_const(s, 8, 0);
          emit_case(out, "adversarial", s, 8, 1); }
        /* (2) n=16, all-zeros - 1 */
        { mp_limb_t s[16]; fill_const(s, 16, 0);
          emit_case(out, "adversarial", s, 16, 1); }
        /* (3) n=32, all-zeros - 1 */
        { mp_limb_t s[32]; fill_const(s, 32, 0);
          emit_case(out, "adversarial", s, 32, 1); }
        /* (4) n=8, all-zeros - M */
        { mp_limb_t s[8]; fill_const(s, 8, 0);
          emit_case(out, "adversarial", s, 8, M); }
        /* (5) n=8, [0,0,0,0,M,M,M,M] - 1: borrow ripples 4 limbs then stops */
        { mp_limb_t s[8] = {0,0,0,0,M,M,M,M};
          emit_case(out, "adversarial", s, 8, 1); }
        /* (6) n=8, [M,M,M,M,0,0,0,0] - 1: no ripple (low limb absorbs) */
        { mp_limb_t s[8] = {M,M,M,M,0,0,0,0};
          emit_case(out, "adversarial", s, 8, 1); }
        /* (7) n=16, [0..8, M..15] - 1 */
        { mp_limb_t s[16] = {0,0,0,0,0,0,0,0, M,M,M,M,M,M,M,M};
          emit_case(out, "adversarial", s, 16, 1); }
        /* (8) n=16, alternating + M */
        { mp_limb_t s[16];
          for (size_t i = 0; i < 16; ++i) s[i] = (i & 1) ? M : 0;
          emit_case(out, "adversarial", s, 16, M); }
        /* (9) n=32 random, x=M */
        { xs64_t rng; xs64_seed(&rng, 0x5DB1ADBEEDULL);
          mp_limb_t s[32]; fill_random(&rng, s, 32);
          emit_case(out, "adversarial", s, 32, M); }
        /* (10) n=64, all-zeros - 1: max-size borrow ripple */
        { mp_limb_t s[64]; fill_const(s, 64, 0);
          emit_case(out, "adversarial", s, 64, 1); }
        /* (11) n=64, all-max - M */
        { mp_limb_t s[64]; fill_const(s, 64, M);
          emit_case(out, "adversarial", s, 64, M); }
        /* (12) n=1, s=[MSB], x=MSB - exact-cancel */
        { mp_limb_t s[1] = {(mp_limb_t)1 << 63};
          emit_case(out, "adversarial", s, 1, (mp_limb_t)1 << 63); }
    }

    /* fuzz: 56 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xF0225DB1EE0ULL);
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
