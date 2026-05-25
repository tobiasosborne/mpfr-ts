/*
 * golden_driver.c -- Golden master for GMP's mpn_rshift.
 *
 * Signature
 * ---------
 *
 *   mp_limb_t mpn_rshift(mp_limb_t       *rp,
 *                        const mp_limb_t *sp,
 *                        mp_size_t        n,
 *                        unsigned int     count);
 *
 * Shifts {sp, n} right by count bits, writes the result to {rp, n}, and
 * returns the bits shifted out at the bottom packed into the HIGH count
 * bits of the return value (the rest is zero). Limb order: little-endian.
 * Ref: GMP manual §8.3.
 *
 * Preconditions (per GMP):
 *   - n > 0
 *   - 1 <= count < GMP_NUMB_BITS  (= 1..63 here)
 *   - count = 0 is UNDEFINED BEHAVIOUR; excluded from goldens.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>","inputs":{"s":["<dec>",...],"n":<int>,"count":<int>},
 *    "output":{"result":["<dec>",...],"out":"<dec>"},"time_ns":<n>}
 *
 * Tag distribution (mined absent -- substrate carve-out):
 *
 *   happy        :  20
 *   edge         :  32
 *   adversarial  :  12
 *   fuzz         :  56
 *   ------------ ----
 *   total        : 120
 */
#include "common.h"

#include <assert.h>

#if GMP_NUMB_BITS != 64
#  error "mpn_rshift golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define MAX_N 64

static inline void emit_case(FILE *out,
                             const char *tag,
                             const mp_limb_t *s,
                             size_t n,
                             unsigned int count) {
    assert(n >= 1 && n <= MAX_N);
    assert(count >= 1 && count <= 63);

    mp_limb_t result[MAX_N + 1];
    for (size_t i = 0; i <= MAX_N; ++i) result[i] = 0xDEADBEEFCAFEBABEULL;
    const uint64_t t0 = now_ns();
    const mp_limb_t shifted_out = mpn_rshift(result, s, (mp_size_t)n, count);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_limbs(out, 1, "s", s, n);
    jl_kv_int  (out, 0, "n", (int)n);
    jl_kv_int  (out, 0, "count", (int)count);
    jl_end_inputs(out);

    jl_output_begin_object(out);
    jl_kv_limbs(out, 1, "result", result, n);
    jl_kv_u64  (out, 0, "out", (uint64_t)shifted_out);
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

    /* happy: 20 -- n in {1..4}, count cycled through [1..63] */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x4DD15B17ULL); /* trim */
        xs64_seed(&rng, 0x4DD15B17ULL);
        const unsigned int counts[] = {1, 7, 16, 31, 32, 47, 56, 62, 63};
        int idx = 0;
        for (size_t n = 1; n <= 4; ++n) {
            for (int rep = 0; rep < 5; ++rep) {
                mp_limb_t s[MAX_N];
                fill_random(&rng, s, n);
                const unsigned int c = counts[(idx++) % 9];
                emit_case(out, "happy", s, n, c);
            }
        }
    }

    /* edge: 32 -- boundary patterns */
    {
        /* (1-4) n=1 specific patterns at count=1 */
        { mp_limb_t s[1] = {0};            emit_case(out, "edge", s, 1, 1); }
        { mp_limb_t s[1] = {1};            emit_case(out, "edge", s, 1, 1); }
        { mp_limb_t s[1] = {M};            emit_case(out, "edge", s, 1, 1); }
        { mp_limb_t s[1] = {(mp_limb_t)1 << 63}; emit_case(out, "edge", s, 1, 1); }

        /* (5-8) n=1 at count=63 (max shift) */
        { mp_limb_t s[1] = {0};            emit_case(out, "edge", s, 1, 63); }
        { mp_limb_t s[1] = {1};            emit_case(out, "edge", s, 1, 63); }
        { mp_limb_t s[1] = {M};            emit_case(out, "edge", s, 1, 63); }
        { mp_limb_t s[1] = {(mp_limb_t)1 << 63}; emit_case(out, "edge", s, 1, 63); }

        /* (9-12) n=1 at count=32 (mid shift) */
        { mp_limb_t s[1] = {0};            emit_case(out, "edge", s, 1, 32); }
        { mp_limb_t s[1] = {M};            emit_case(out, "edge", s, 1, 32); }
        { mp_limb_t s[1] = {0x123456789ABCDEF0ULL}; emit_case(out, "edge", s, 1, 32); }
        { mp_limb_t s[1] = {0xFFFFFFFF00000000ULL}; emit_case(out, "edge", s, 1, 32); }

        /* (13-18) n=2 various */
        { mp_limb_t s[2] = {0,0};          emit_case(out, "edge", s, 2, 1); }
        { mp_limb_t s[2] = {M,M};          emit_case(out, "edge", s, 2, 1); }
        { mp_limb_t s[2] = {0,M};          emit_case(out, "edge", s, 2, 1); }
        { mp_limb_t s[2] = {M,0};          emit_case(out, "edge", s, 2, 1); }
        { mp_limb_t s[2] = {1,0};          emit_case(out, "edge", s, 2, 1); }
        { mp_limb_t s[2] = {0,1};          emit_case(out, "edge", s, 2, 1); }

        /* (19-22) n=2 at count=63 */
        { mp_limb_t s[2] = {0,M};          emit_case(out, "edge", s, 2, 63); }
        { mp_limb_t s[2] = {M,0};          emit_case(out, "edge", s, 2, 63); }
        { mp_limb_t s[2] = {M,M};          emit_case(out, "edge", s, 2, 63); }
        { mp_limb_t s[2] = {1,1};          emit_case(out, "edge", s, 2, 63); }

        /* (23-26) n=3 specific patterns */
        { mp_limb_t s[3] = {0,0,0};        emit_case(out, "edge", s, 3, 1); }
        { mp_limb_t s[3] = {M,M,M};        emit_case(out, "edge", s, 3, 1); }
        { mp_limb_t s[3] = {0,0,M};        emit_case(out, "edge", s, 3, 16); }
        { mp_limb_t s[3] = {M,0,0};        emit_case(out, "edge", s, 3, 16); }

        /* (27-32) n=4 various counts */
        { mp_limb_t s[4] = {0,0,0,0};      emit_case(out, "edge", s, 4, 1); }
        { mp_limb_t s[4] = {M,M,M,M};      emit_case(out, "edge", s, 4, 1); }
        { mp_limb_t s[4] = {M,M,M,M};      emit_case(out, "edge", s, 4, 32); }
        { mp_limb_t s[4] = {M,M,M,M};      emit_case(out, "edge", s, 4, 63); }
        { mp_limb_t s[4] = {0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL};
          emit_case(out, "edge", s, 4, 1); }
        { mp_limb_t s[4] = {0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL};
          emit_case(out, "edge", s, 4, 63); }
    }

    /* adversarial: 12 -- mid-shift patterns and large-n */
    {
        /* Cross-limb shift patterns at varied count */
        { mp_limb_t s[8]; fill_const(s, 8, M);
          emit_case(out, "adversarial", s, 8, 1); }
        { mp_limb_t s[8]; fill_const(s, 8, M);
          emit_case(out, "adversarial", s, 8, 32); }
        { mp_limb_t s[8]; fill_const(s, 8, M);
          emit_case(out, "adversarial", s, 8, 63); }
        { mp_limb_t s[16]; fill_const(s, 16, M);
          emit_case(out, "adversarial", s, 16, 1); }
        { mp_limb_t s[16]; fill_const(s, 16, M);
          emit_case(out, "adversarial", s, 16, 63); }
        { mp_limb_t s[16]; fill_const(s, 16, (mp_limb_t)1 << 63);
          emit_case(out, "adversarial", s, 16, 1); }
        { mp_limb_t s[16];
          for (size_t i = 0; i < 16; ++i) s[i] = (mp_limb_t)(i + 1);
          emit_case(out, "adversarial", s, 16, 7); }
        { mp_limb_t s[32];
          for (size_t i = 0; i < 32; ++i) s[i] = (i & 1) ? M : 0;
          emit_case(out, "adversarial", s, 32, 31); }
        { xs64_t rng; xs64_seed(&rng, 0xADC05E1FBEEDULL);
          mp_limb_t s[32]; fill_random(&rng, s, 32);
          emit_case(out, "adversarial", s, 32, 1); }
        { xs64_t rng; xs64_seed(&rng, 0xADC05E1FBEEEULL);
          mp_limb_t s[32]; fill_random(&rng, s, 32);
          emit_case(out, "adversarial", s, 32, 32); }
        { mp_limb_t s[64]; fill_const(s, 64, M);
          emit_case(out, "adversarial", s, 64, 1); }
        { mp_limb_t s[64]; fill_const(s, 64, M);
          emit_case(out, "adversarial", s, 64, 63); }
    }

    /* fuzz: 56 -- PRNG-driven */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xF022451FEE0ULL);
        const size_t ns[] = {1, 2, 4, 8, 16, 32};
        for (int rep = 0; rep < 56; ++rep) {
            const size_t n = ns[(size_t)xs64_below(&rng, 6)];
            mp_limb_t s[MAX_N];
            fill_random(&rng, s, n);
            const unsigned int c = (unsigned int)(1 + xs64_below(&rng, 63));
            emit_case(out, "fuzz", s, n, c);
        }
    }

    return 0;
}
