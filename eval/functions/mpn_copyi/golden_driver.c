/*
 * golden_driver.c -- Golden master for GMP's mpn_copyi.
 *
 * Signature
 * ---------
 *
 *   void mpn_copyi(mp_limb_t *rp, const mp_limb_t *sp, mp_size_t n);
 *
 * Copies n limbs from sp to rp in INCREASING address order (rp[0] = sp[0],
 * then rp[1] = sp[1], ..., rp[n-1] = sp[n-1]). When src and dst overlap
 * with rp <= sp, the increasing direction is safe; when rp > sp, callers
 * use mpn_copyd instead. Ref: GMP manual §8.3.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>","inputs":{"s":["<dec>",...],"n":<int>},
 *    "output":{"result":["<dec>",...]},"time_ns":<n>}
 *
 *   - `s`, `result`: GMP-limb arrays, decimal-string per limb;
 *     little-endian limb order.
 *   - `n`: raw JS number (1..MAX_N).
 *
 * The output is wrapped in `{result}` rather than emitted as a bare array
 * because eval/harness/value_codec.ts has no bare-array output decoder
 * (see decodeExpectedOutput L274-L341: 5 supported shapes, none of which
 * is a top-level array). Wrapping in `{result}` reuses the generic-struct
 * decode path and matches the convention of mpn_add_n / mpn_sub_n.
 *
 * Tag distribution (mined absent -- substrate carve-out, mpfr/tests/ has
 * no isolated mpn_copyi test driver):
 *
 *   happy        :  20
 *   edge         :  30
 *   adversarial  :  10
 *   fuzz         :  50
 *   ------------ ----
 *   total        : 110
 *
 * Per-class PRNG seeds are distinct so a failing fuzz case can be
 * reproduced without re-running the happy block.
 */
#include "common.h"

#include <assert.h>

#if GMP_NUMB_BITS != 64
#  error "mpn_copyi golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define MAX_N 64

/* Emit one case. Performs mpn_copyi from s into a separate result buffer,
 * times the call, and writes a JSONL record. */
static inline void emit_case(FILE *out,
                             const char *tag,
                             const mp_limb_t *s,
                             size_t n) {
    assert(n >= 1 && n <= MAX_N);

    mp_limb_t result[MAX_N];
    const uint64_t t0 = now_ns();
    mpn_copyi(result, s, (mp_size_t)n);
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

    /* ============================================================== */
    /* happy: 20 cases -- n in {1..4}, 5 random fills per n           */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0791DEAULL);
        for (size_t n = 1; n <= 4; ++n) {
            for (int rep = 0; rep < 5; ++rep) {
                mp_limb_t s[MAX_N];
                fill_random(&rng, s, n);
                emit_case(out, "happy", s, n);
            }
        }
    }

    /* ============================================================== */
    /* edge: 30 cases -- boundary patterns                            */
    /* ============================================================== */
    {
        const mp_limb_t M = ~(mp_limb_t)0;

        /* n=1 patterns (8 cases) */
        { mp_limb_t s[1] = {0};            emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {1};            emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {M};            emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {M / 2};        emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {(mp_limb_t)1 << 63}; emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {0x5555555555555555ULL}; emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {0xAAAAAAAAAAAAAAAAULL}; emit_case(out, "edge", s, 1); }
        { mp_limb_t s[1] = {0x123456789ABCDEF0ULL}; emit_case(out, "edge", s, 1); }

        /* n=2 patterns (8 cases) */
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

        /* n=3 patterns (8 cases) */
        { mp_limb_t s[3] = {0,0,0};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {M,M,M};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {1,0,0};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {0,0,1};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {0,M,0};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {M,0,M};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {1,2,3};        emit_case(out, "edge", s, 3); }
        { mp_limb_t s[3] = {M-1, M-2, M-3}; emit_case(out, "edge", s, 3); }

        /* n=4 patterns (6 cases) */
        { mp_limb_t s[4] = {0,0,0,0};      emit_case(out, "edge", s, 4); }
        { mp_limb_t s[4] = {M,M,M,M};      emit_case(out, "edge", s, 4); }
        { mp_limb_t s[4] = {1,0,0,0};      emit_case(out, "edge", s, 4); }
        { mp_limb_t s[4] = {0,0,0,1};      emit_case(out, "edge", s, 4); }
        { mp_limb_t s[4] = {0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL};
          emit_case(out, "edge", s, 4); }
        { mp_limb_t s[4] = {1,2,3,4};      emit_case(out, "edge", s, 4); }
    }

    /* ============================================================== */
    /* adversarial: 10 cases -- patterns likely to break a naive copy */
    /* ============================================================== */
    {
        const mp_limb_t M = ~(mp_limb_t)0;

        /* All-MSB across n=8 (sign-bit pollution) */
        { mp_limb_t s[8]; fill_const(s, 8, (mp_limb_t)1 << 63);
          emit_case(out, "adversarial", s, 8); }
        /* All-max across n=8 */
        { mp_limb_t s[8]; fill_const(s, 8, M);
          emit_case(out, "adversarial", s, 8); }
        /* Alternating high-bit pattern */
        { mp_limb_t s[8] = {0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL,
                            0x5555555555555555ULL, 0xAAAAAAAAAAAAAAAAULL};
          emit_case(out, "adversarial", s, 8); }
        /* Single non-zero in middle, n=16 */
        { mp_limb_t s[16] = {0};
          s[8] = M;
          emit_case(out, "adversarial", s, 16); }
        /* Strictly increasing limbs (i+1) -- shape check */
        { mp_limb_t s[16];
          for (size_t i = 0; i < 16; ++i) s[i] = (mp_limb_t)(i + 1);
          emit_case(out, "adversarial", s, 16); }
        /* Strictly decreasing limbs (M - i) */
        { mp_limb_t s[16];
          for (size_t i = 0; i < 16; ++i) s[i] = M - (mp_limb_t)i;
          emit_case(out, "adversarial", s, 16); }
        /* n=1, just to keep the small-n adversarial covered */
        { mp_limb_t s[1] = {0xDEADBEEFCAFEBABEULL};
          emit_case(out, "adversarial", s, 1); }
        /* n=32 random (large copy) */
        { xs64_t rng; xs64_seed(&rng, 0xADC09EE0DEEDULL);
          mp_limb_t s[32]; fill_random(&rng, s, 32);
          emit_case(out, "adversarial", s, 32); }
        /* n=2 with low=1 high=0 (canonical "small bigint") */
        { mp_limb_t s[2] = {1, 0};
          emit_case(out, "adversarial", s, 2); }
        /* n=64 all-max (max supported) */
        { mp_limb_t s[64]; fill_const(s, 64, M);
          emit_case(out, "adversarial", s, 64); }
    }

    /* ============================================================== */
    /* fuzz: 50 cases -- PRNG-driven, n in {1, 2, 4, 8, 16, 32}        */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xF022C0791ULL);
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
