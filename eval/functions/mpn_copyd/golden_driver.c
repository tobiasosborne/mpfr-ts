/*
 * golden_driver.c -- Golden master for GMP's mpn_copyd.
 *
 * Signature
 * ---------
 *
 *   void mpn_copyd(mp_limb_t *rp, const mp_limb_t *sp, mp_size_t n);
 *
 * Copies n limbs from sp to rp in DECREASING address order (rp[n-1] =
 * sp[n-1], ..., rp[0] = sp[0]). When src and dst overlap with rp > sp,
 * the decreasing direction is safe; when rp <= sp, callers use mpn_copyi.
 * The visible output is identical to mpn_copyi for non-overlapping
 * buffers; this driver only exercises the non-overlapping contract since
 * the TS port always returns a fresh array.
 * Ref: GMP manual §8.3.
 *
 * Wire format / tag distribution / structure: identical to mpn_copyi's
 * driver. See eval/functions/mpn_copyi/golden_driver.c header for the
 * full rationale.
 */
#include "common.h"

#include <assert.h>

#if GMP_NUMB_BITS != 64
#  error "mpn_copyd golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define MAX_N 64

static inline void emit_case(FILE *out,
                             const char *tag,
                             const mp_limb_t *s,
                             size_t n) {
    assert(n >= 1 && n <= MAX_N);

    mp_limb_t result[MAX_N];
    const uint64_t t0 = now_ns();
    mpn_copyd(result, s, (mp_size_t)n);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_limbs(out, 1, "s", s, n);
    jl_kv_int  (out, 0, "n", (int)n);
    jl_end_inputs(out);

    jl_output_begin_object(out);
    jl_kv_limbs(out, 1, "result", result, n);
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

    /* happy: 20 -- n in {1..4}, 5 random per n */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0791DEDULL);
        for (size_t n = 1; n <= 4; ++n) {
            for (int rep = 0; rep < 5; ++rep) {
                mp_limb_t s[MAX_N];
                fill_random(&rng, s, n);
                emit_case(out, "happy", s, n);
            }
        }
    }

    /* edge: 30 -- boundary patterns mirroring the copyi driver */
    {
        const mp_limb_t M = ~(mp_limb_t)0;

        { mp_limb_t s[1] = {0};            emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {1};            emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {M};            emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {M / 2};        emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {(mp_limb_t)1 << 63}; emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {0x5555555555555555ULL}; emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {0xAAAAAAAAAAAAAAAAULL}; emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {0x123456789ABCDEF0ULL}; emit_case(out, "edge", s, 1); }

        { mp_limb_t s[2] = {0,0};          emit_case(out, "edge", s, 2); }
        { mp_limb_t s[2] = {M,M};          emit_case(out, "edge", s, 2); }
        { mp_limb_t s[2] = {0,M};          emit_case(out, "edge", s, 2); }
        { mp_limb_t s[2] = {M,0};          emit_case(out, "edge", s, 2); }
        { mp_limb_t s[2] = {1,0};          emit_case(out, "edge", s, 2); }
        { mp_limb_t s[2] = {0,1};          emit_case(out, "edge", s, 2); }
        { mp_limb_t s[2] = {0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL};
          emit_case(out, "edge", s, 2); }
        { mp_limb_t s[2] = {(mp_limb_t)1 << 63, (mp_limb_t)1 << 63};
          emit_case(out, "edge", s, 2); }

        { mp_limb_t s[3] = {0,0,0};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {M,M,M};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {1,0,0};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {0,0,1};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {0,M,0};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {M,0,M};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {1,2,3};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {M-1, M-2, M-3}; emit_case(out, "edge", s, 3); }

        { mp_limb_t s[4] = {0,0,0,0};      emit_case(out, "edge", s, 4); }
        { mp_limb_t s[4] = {M,M,M,M};      emit_case(out, "edge", s, 4); }
        { mp_limb_t s[4] = {1,0,0,0};      emit_case(out, "edge", s, 4); }
        { mp_limb_t s[4] = {0,0,0,1};      emit_case(out, "edge", s, 4); }
        { mp_limb_t s[4] = {0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL};
          emit_case(out, "edge", s, 4); }
        { mp_limb_t s[4] = {1,2,3,4};      emit_case(out, "edge", s, 4); }
    }

    /* adversarial: 10 -- patterns mirroring the copyi driver */
    {
        const mp_limb_t M = ~(mp_limb_t)0;

        { mp_limb_t s[8]; fill_const(s, 8, (mp_limb_t)1 << 63);
          emit_case(out, "adversarial", s, 8); }
        { mp_limb_t s[8]; fill_const(s, 8, M);
          emit_case(out, "adversarial", s, 8); }
        { mp_limb_t s[8] = {0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL};
          emit_case(out, "adversarial", s, 8); }
        { mp_limb_t s[16] = {0}; s[8] = M;
          emit_case(out, "adversarial", s, 16); }
        { mp_limb_t s[16];
          for (size_t i = 0; i < 16; ++i) s[i] = (mp_limb_t)(i + 1);
          emit_case(out, "adversarial", s, 16); }
        { mp_limb_t s[16];
          for (size_t i = 0; i < 16; ++i) s[i] = M - (mp_limb_t)i;
          emit_case(out, "adversarial", s, 16); }
        { mp_limb_t s[1] = {0xDEADBEEFCAFEBABEULL};
          emit_case(out, "adversarial", s, 1); }
        { xs64_t rng; xs64_seed(&rng, 0xADC09EE0DEDDULL);
          mp_limb_t s[32]; fill_random(&rng, s, 32);
          emit_case(out, "adversarial", s, 32); }
        { mp_limb_t s[2] = {1, 0};
          emit_case(out, "adversarial", s, 2); }
        { mp_limb_t s[64]; fill_const(s, 64, M);
          emit_case(out, "adversarial", s, 64); }
    }

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xF022C0791DULL);
        const size_t ns[] = {1, 2, 4, 8, 16, 32};
        for (int rep = 0; rep < 50; ++rep) {
            const size_t n = ns[(size_t)xs64_below(&rng, 6)];
            mp_limb_t s[MAX_N];
            fill_random(&rng, s, n);
            emit_case(out, "fuzz", s, n);
        }
    }

    return 0;
}
