/*
 * golden_driver.c — Golden master for MPFR's mpfr_div_d.
 *
 * Trivial wrapper around mpfr_set_d + mpfr_div (mpfr/src/div_d.c L25-L50).
 * Tag distribution: happy 22, edge 32, adversarial 12, fuzz 60, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_div_d golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_CAP ((uint64_t)4096)

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
};

static inline double make_neg_zero(void) {
    const uint64_t bits = (uint64_t)1 << 63;
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr b, double c,
                             uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t a;
    mpfr_init2(a, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_div_d(a, b, c, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "b", b);
    jl_kv_double(out, 0, "c", c);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, a, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(a);
}

static inline void emit_dd(FILE *out, const char *tag,
                           double bd, uint64_t bprec,
                           double c, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t b; mpfr_init2(b, (mpfr_prec_t)bprec); mpfr_set_d(b, bd, MPFR_RNDN);
    emit_case(out, tag, b, c, prec, rnd);
    mpfr_clear(b);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    emit_dd(out, "happy", 6.0, 53, 2.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 53, 4.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 53, 3.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", -6.0, 53, 2.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", -6.0, 53, -2.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 24, 1.0, 24, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 24, 2.0, 24, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 100, 7.0, 100, MPFR_RNDN);
    emit_dd(out, "happy", 1e10, 53, 1e5, 53, MPFR_RNDN);
    emit_dd(out, "happy", 0.0, 53, 1.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 100.5, 32, 0.25, 32, MPFR_RNDA);
    emit_dd(out, "happy", -100.5, 32, 0.25, 32, MPFR_RNDA);
    emit_dd(out, "happy", 22.0, 53, 7.0, 24, MPFR_RNDN);
    emit_dd(out, "happy", 2.0, 53, 2.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", -1.0, 53, -1.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 7.0, 53, 11.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 0.1, 200, 10.0, 200, MPFR_RNDN);
    emit_dd(out, "happy", 1e-100, 53, 1e-100, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1e100, 53, 1.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 53, 1.0/3.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 53, -1.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", -1.0, 53, 2.0, 53, MPFR_RNDD);

    /* edge: 32 */
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_nan(b); emit_case(out, "edge", b, 1.0, 53, MPFR_RNDN); mpfr_clear(b); }
    emit_dd(out, "edge", 1.0, 53, NAN, 53, MPFR_RNDN);
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_inf(b, +1); emit_case(out, "edge", b, 1.0, 53, MPFR_RNDN); mpfr_clear(b); }
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_inf(b, -1); emit_case(out, "edge", b, -1.0, 53, MPFR_RNDN); mpfr_clear(b); }
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_inf(b, +1); emit_case(out, "edge", b, INFINITY, 53, MPFR_RNDN); mpfr_clear(b); }  /* Inf/Inf = NaN */
    { mpfr_t b; mpfr_init2(b, 53); mpfr_set_zero(b, +1); emit_case(out, "edge", b, 0.0, 53, MPFR_RNDN); mpfr_clear(b); }    /* 0/0 = NaN */
    emit_dd(out, "edge", 1.0, 53, 0.0, 53, MPFR_RNDN);   /* finite / +0 = +Inf */
    emit_dd(out, "edge", -1.0, 53, 0.0, 53, MPFR_RNDN);  /* neg / +0 = -Inf */
    emit_dd(out, "edge", 1.0, 53, make_neg_zero(), 53, MPFR_RNDN);  /* finite / -0 = -Inf */
    emit_dd(out, "edge", 1.0, 53, INFINITY, 53, MPFR_RNDN);   /* finite / +Inf = +0 */
    emit_dd(out, "edge", -1.0, 53, INFINITY, 53, MPFR_RNDN);  /* neg / +Inf = -0 */
    emit_dd(out, "edge", 0.0, 53, 5.0, 53, MPFR_RNDN);   /* +0 / +5 = +0 */
    emit_dd(out, "edge", make_neg_zero(), 53, 5.0, 53, MPFR_RNDN);   /* -0 / +5 = -0 */
    emit_dd(out, "edge", 0.0, 53, -5.0, 53, MPFR_RNDN);   /* +0 / -5 = -0 */
    for (int i = 0; i < 5; i++) emit_dd(out, "edge", 1.0, 1, 1.0, 1, RNDS[i]);
    for (int i = 0; i < 5; i++) emit_dd(out, "edge", 1.5, 53, 0.25, 2, RNDS[i]);
    emit_dd(out, "edge", 3.14, TS_PREC_CAP, 2.71, TS_PREC_CAP, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 1000, 1.0, 1000, MPFR_RNDN);
    emit_dd(out, "edge", DBL_MAX, 53, 0.5, 53, MPFR_RNDN);   /* overflow */
    emit_dd(out, "edge", DBL_MIN, 53, 2.0, 53, MPFR_RNDN);   /* underflow */
    emit_dd(out, "edge", 3.14, 200, 2.71, 53, MPFR_RNDN);
    emit_dd(out, "edge", 3.14, 24, 2.71, 200, MPFR_RNDN);
    emit_dd(out, "edge", 1.0/3.0, 53, 1.0/7.0, 24, MPFR_RNDZ);
    emit_dd(out, "edge", 1.0/3.0, 53, 1.0/7.0, 24, MPFR_RNDU);

    /* adversarial: 12 */
    emit_dd(out, "adversarial", 0.9999999999999999, 53, 1.0, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", -0.9999999999999999, 53, 1.0, 53, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 53, 1.0 + DBL_EPSILON, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.0, 53, 3.0, 53, MPFR_RNDU);
    emit_dd(out, "adversarial", 1.0, 53, 3.0, 53, MPFR_RNDD);
    emit_dd(out, "adversarial", DBL_MAX, 53, 0.5, 53, MPFR_RNDU);
    emit_dd(out, "adversarial", DBL_MAX, 53, 0.5, 53, MPFR_RNDZ);
    emit_dd(out, "adversarial", -DBL_MAX, 53, 0.5, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", DBL_MIN, 53, 2.0, 53, MPFR_RNDZ);
    emit_dd(out, "adversarial", 1.0, 53, 7.0, 1, MPFR_RNDN);
    emit_dd(out, "adversarial", -1.0, 53, 7.0, 1, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 53, 7.0, 1, MPFR_RNDU);

    /* fuzz: 60 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xD1D1D1D1D1D1D1D1ULL);
        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 256);
            const uint64_t bprec = 1 + xs64_below(&rng, 256);
            const uint64_t r1 = xs64_next(&rng);
            double bd = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            const uint64_t r2 = xs64_next(&rng);
            /* For div, ensure c != 0 to avoid div-by-zero overrun in fuzz
             * (those are well-defined but skew the distribution). */
            double c  = ((double)((r2 % 199998ULL) + 1) - 99999.0) / 100.0;
            if (c == 0.0) c = 1.0;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            emit_dd(out, "fuzz", bd, bprec, c, prec, RNDS[rnd_idx]);
        }
    }

    /* mined: 5 */
    emit_dd(out, "mined", 6.0, 53, 2.0, 53, MPFR_RNDN);
    emit_dd(out, "mined", 1.0, 53, 3.0, 53, MPFR_RNDU);
    emit_dd(out, "mined", 0.0, 53, 5.0, 53, MPFR_RNDN);
    emit_dd(out, "mined", -1.0, 53, 2.0, 53, MPFR_RNDN);
    emit_dd(out, "mined", 1.5e10, 53, 2.0, 53, MPFR_RNDU);

    return 0;
}
