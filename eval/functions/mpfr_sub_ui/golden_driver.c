/*
 * golden_driver.c — Golden master for MPFR's mpfr_sub_ui.
 *
 * Wire format: like add_ui but the operator is subtraction.
 * Tag distribution: 22/30/10/55/5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sub_ui golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr b, unsigned long c,
                             uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t y;
    mpfr_init2(y, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_sub_ui(y, b, c, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "b", b);
    jl_kv_u64(out, 0, "c", (uint64_t)c);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, y, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(y);
}

static inline void emit_d(FILE *out, const char *tag, double bd, uint64_t bprec,
                          unsigned long c, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t b; mpfr_init2(b, (mpfr_prec_t)bprec); mpfr_set_d(b, bd, MPFR_RNDN);
    emit_case(out, tag, b, c, prec, rnd);
    mpfr_clear(b);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    emit_d(out, "happy", 5.0, 53, 0, 53, MPFR_RNDN);
    emit_d(out, "happy", 5.0, 53, 1, 53, MPFR_RNDN);
    emit_d(out, "happy", 5.0, 53, 3, 53, MPFR_RNDN);
    emit_d(out, "happy", 5.0, 53, 5, 53, MPFR_RNDN);   /* 5 - 5 = 0 */
    emit_d(out, "happy", 5.0, 53, 10, 53, MPFR_RNDN);  /* 5 - 10 = -5 */
    emit_d(out, "happy", -5.0, 53, 3, 53, MPFR_RNDN);
    emit_d(out, "happy", 3.14, 53, 2, 53, MPFR_RNDN);
    emit_d(out, "happy", 3.14, 53, 5, 53, MPFR_RNDN);
    emit_d(out, "happy", -3.14, 53, 5, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.5, 53, 100, 53, MPFR_RNDN);
    emit_d(out, "happy", 1e10, 53, 100, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0/3.0, 53, 1, 24, MPFR_RNDN);
    emit_d(out, "happy", 0.0, 53, 5, 53, MPFR_RNDN);   /* +0 - 5 = -5 */
    /* Singulars. */
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_nan(b); emit_case(out, "happy", b, 5, 53, MPFR_RNDN); mpfr_clear(b); }
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_inf(b, +1); emit_case(out, "happy", b, 5, 53, MPFR_RNDN); mpfr_clear(b); }
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_inf(b, -1); emit_case(out, "happy", b, 5, 53, MPFR_RNDN); mpfr_clear(b); }
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_zero(b, +1); emit_case(out, "happy", b, 5, 53, MPFR_RNDN); mpfr_clear(b); }
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_zero(b, -1); emit_case(out, "happy", b, 5, 53, MPFR_RNDN); mpfr_clear(b); }
    /* c = 0. */
    emit_d(out, "happy", 3.14, 53, 0, 53, MPFR_RNDN);
    emit_d(out, "happy", -3.14, 53, 0, 24, MPFR_RNDN);
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_nan(b); emit_case(out, "happy", b, 0, 53, MPFR_RNDN); mpfr_clear(b); }
    /* c = ULONG_MAX. */
    { mpfr_t b; mpfr_init2(b, 64); mpfr_set_d(b, 1e20, MPFR_RNDN); emit_case(out, "happy", b, ULONG_MAX, 64, MPFR_RNDN); mpfr_clear(b); }

    /* edge: 30 */
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.5, 53, 0, 53, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 5.0, 53, 3, 53, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.0/3.0, 53, 1, 3, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", -1.0/3.0, 53, 1, 3, RNDS[i]);
    /* PREC_MIN. */
    emit_d(out, "edge", 5.0, 1, 1, 1, MPFR_RNDN);
    emit_d(out, "edge", 5.0, 1, 3, 1, MPFR_RNDU);
    /* Singulars at PREC_MIN. */
    { mpfr_t b; mpfr_init2(b, 1); mpfr_set_nan(b); emit_case(out, "edge", b, 0, 1, MPFR_RNDN); mpfr_clear(b); }
    { mpfr_t b; mpfr_init2(b, 1); mpfr_set_inf(b, -1); emit_case(out, "edge", b, 5, 1024, MPFR_RNDN); mpfr_clear(b); }
    /* ULONG_MAX. */
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_d(b, 1.5, MPFR_RNDN); emit_case(out, "edge", b, ULONG_MAX, 53, MPFR_RNDN); mpfr_clear(b); }
    /* Limb boundaries. */
    emit_d(out, "edge", 5.0, 63, 1, 53, MPFR_RNDN);
    emit_d(out, "edge", 5.0, 64, 1, 64, MPFR_RNDN);
    emit_d(out, "edge", 5.0, 65, 1, 64, MPFR_RNDN);
    emit_d(out, "edge", 5.0, 200, 100, 100, MPFR_RNDN);
    /* Sign-change cases. */
    emit_d(out, "edge", 0.5, 53, 1, 53, MPFR_RNDN);   /* 0.5 - 1 = -0.5 */
    emit_d(out, "edge", -0.5, 53, 1, 53, MPFR_RNDN);  /* -0.5 - 1 = -1.5 */
    emit_d(out, "edge", 100.0, 53, 50, 53, MPFR_RNDN);

    /* adversarial: 10 */
    emit_d(out, "adversarial", 1.0, 53, 1, 53, MPFR_RNDN);   /* exact cancel → +0 */
    emit_d(out, "adversarial", 1.0, 53, 1, 53, MPFR_RNDD);   /* exact cancel → -0 under RNDD */
    emit_d(out, "adversarial", 3.14, 53, 1, 24, MPFR_RNDU);
    emit_d(out, "adversarial", 3.14, 53, 1, 24, MPFR_RNDD);
    emit_d(out, "adversarial", -3.14, 53, 7, 24, MPFR_RNDU);
    emit_d(out, "adversarial", -3.14, 53, 7, 24, MPFR_RNDD);
    emit_d(out, "adversarial", 0.9999999999999999, 53, 1, 24, MPFR_RNDN);
    emit_d(out, "adversarial", 1e300, 53, 1, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 1e-300, 53, 1, 53, MPFR_RNDN);
    emit_d(out, "adversarial", -1.0, 53, 1, 53, MPFR_RNDN);

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5BBCAFE12345BEEFULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t kind_sel = xs64_below(&rng, 10);
            const uint64_t bprec = 1 + xs64_below(&rng, 200);
            const uint64_t prec = 1 + xs64_below(&rng, 200);
            const uint64_t c = xs64_next(&rng);
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            mpfr_t b;
            mpfr_init2(b, (mpfr_prec_t)bprec);
            if (kind_sel < 7) {
                const double mag = (double)(1 + xs64_below(&rng, 1000000));
                const int sx = (xs64_below(&rng, 2) == 0) ? +1 : -1;
                mpfr_set_d(b, sx * mag, MPFR_RNDN);
                if (!mpfr_regular_p(b)) { mpfr_clear(b); continue; }
            } else if (kind_sel == 7) {
                mpfr_set_zero(b, (xs64_below(&rng, 2) == 0) ? +1 : -1);
            } else if (kind_sel == 8) {
                mpfr_set_inf(b, (xs64_below(&rng, 2) == 0) ? +1 : -1);
            } else {
                mpfr_set_nan(b);
            }
            emit_case(out, "fuzz", b, (unsigned long)c, prec, RNDS[rnd_idx]);
            mpfr_clear(b);
        }
    }

    /* mined: 5 */
    emit_d(out, "mined", 5.0, 53, 3, 53, MPFR_RNDN);
    emit_d(out, "mined", 3.14, 53, 0, 53, MPFR_RNDN);
    emit_d(out, "mined", 1.0, 53, 1, 53, MPFR_RNDN);
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_zero(b, +1); emit_case(out, "mined", b, 7, 53, MPFR_RNDN); mpfr_clear(b); }
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_nan(b); emit_case(out, "mined", b, 1, 53, MPFR_RNDN); mpfr_clear(b); }

    return 0;
}
