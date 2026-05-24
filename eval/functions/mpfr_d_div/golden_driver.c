/*
 * golden_driver.c — Golden master for MPFR's mpfr_d_div.
 *
 * Trivial wrapper around mpfr_set_d + mpfr_div (mpfr/src/d_div.c L25-L50).
 * REVERSE direction: b is the DOUBLE (numerator), c is the MPFR (denominator).
 *
 * Tag distribution: happy 22, edge 32, adversarial 12, fuzz 60, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_d_div golden_driver requires GMP_NUMB_BITS == 64"
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
                             double b, mpfr_srcptr c,
                             uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t a;
    mpfr_init2(a, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_d_div(a, b, c, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_double(out, 1, "b", b);
    jl_kv_mpfr(out, 0, "c", c);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, a, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(a);
}

static inline void emit_dd(FILE *out, const char *tag,
                           double b, double cd, uint64_t cprec,
                           uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t c; mpfr_init2(c, (mpfr_prec_t)cprec); mpfr_set_d(c, cd, MPFR_RNDN);
    emit_case(out, tag, b, c, prec, rnd);
    mpfr_clear(c);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    emit_dd(out, "happy", 6.0, 2.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 4.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 3.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", -6.0, 2.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", -6.0, -2.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 1.0, 24, 24, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 2.0, 24, 24, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 7.0, 100, 100, MPFR_RNDN);
    emit_dd(out, "happy", 1e10, 1e5, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", 0.0, 1.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", 100.5, 0.25, 32, 32, MPFR_RNDA);
    emit_dd(out, "happy", -100.5, 0.25, 32, 32, MPFR_RNDA);
    emit_dd(out, "happy", 22.0, 7.0, 24, 53, MPFR_RNDN);
    emit_dd(out, "happy", 2.0, 2.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", -1.0, -1.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", 7.0, 11.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", 0.1, 10.0, 200, 200, MPFR_RNDN);
    emit_dd(out, "happy", 1e-100, 1e-100, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1e100, 1.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 1.0/3.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, -1.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "happy", -1.0, 2.0, 53, 53, MPFR_RNDD);

    /* edge: 32 */
    emit_dd(out, "edge", NAN, 1.0, 53, 53, MPFR_RNDN);  /* NaN num */
    { mpfr_t c; mpfr_init2(c, 53); mpfr_set_nan(c); emit_case(out, "edge", 1.0, c, 53, MPFR_RNDN); mpfr_clear(c); }  /* NaN denom */
    { mpfr_t c; mpfr_init2(c, 53); mpfr_set_inf(c, +1); emit_case(out, "edge", 1.0, c, 53, MPFR_RNDN); mpfr_clear(c); }  /* finite/+Inf = +0 */
    { mpfr_t c; mpfr_init2(c, 53); mpfr_set_inf(c, -1); emit_case(out, "edge", 1.0, c, 53, MPFR_RNDN); mpfr_clear(c); }  /* finite/-Inf = -0 */
    { mpfr_t c; mpfr_init2(c, 53); mpfr_set_inf(c, +1); emit_case(out, "edge", INFINITY, c, 53, MPFR_RNDN); mpfr_clear(c); }  /* Inf/Inf = NaN */
    { mpfr_t c; mpfr_init2(c, 53); mpfr_set_zero(c, +1); emit_case(out, "edge", 0.0, c, 53, MPFR_RNDN); mpfr_clear(c); }    /* 0/0 = NaN */
    { mpfr_t c; mpfr_init2(c, 53); mpfr_set_zero(c, +1); emit_case(out, "edge", 1.0, c, 53, MPFR_RNDN); mpfr_clear(c); }    /* 1/+0 = +Inf */
    { mpfr_t c; mpfr_init2(c, 53); mpfr_set_zero(c, -1); emit_case(out, "edge", 1.0, c, 53, MPFR_RNDN); mpfr_clear(c); }    /* 1/-0 = -Inf */
    { mpfr_t c; mpfr_init2(c, 53); mpfr_set_zero(c, +1); emit_case(out, "edge", -1.0, c, 53, MPFR_RNDN); mpfr_clear(c); }   /* -1/+0 = -Inf */
    emit_dd(out, "edge", INFINITY, 2.0, 53, 53, MPFR_RNDN);  /* +Inf/2 = +Inf */
    emit_dd(out, "edge", -INFINITY, 2.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "edge", INFINITY, -2.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "edge", 0.0, 5.0, 53, 53, MPFR_RNDN);  /* +0/+5 = +0 */
    emit_dd(out, "edge", make_neg_zero(), 5.0, 53, 53, MPFR_RNDN);  /* -0/+5 = -0 */
    emit_dd(out, "edge", 0.0, -5.0, 53, 53, MPFR_RNDN);  /* +0/-5 = -0 */
    for (int i = 0; i < 5; i++) emit_dd(out, "edge", 1.0, 1.0, 1, 1, RNDS[i]);
    for (int i = 0; i < 5; i++) emit_dd(out, "edge", 1.5, 0.25, 2, 53, RNDS[i]);
    emit_dd(out, "edge", 3.14, 2.71, TS_PREC_CAP, TS_PREC_CAP, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 1.0, 1000, 1000, MPFR_RNDN);
    emit_dd(out, "edge", DBL_MAX, 0.5, 53, 53, MPFR_RNDN);   /* overflow */
    emit_dd(out, "edge", DBL_MIN, 2.0, 53, 53, MPFR_RNDN);   /* underflow */
    emit_dd(out, "edge", 3.14, 2.71, 200, 53, MPFR_RNDN);
    emit_dd(out, "edge", 3.14, 2.71, 24, 200, MPFR_RNDN);

    /* adversarial: 12 */
    emit_dd(out, "adversarial", 0.9999999999999999, 1.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", -0.9999999999999999, 1.0, 53, 53, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 1.0 + DBL_EPSILON, 53, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.0, 3.0, 53, 53, MPFR_RNDU);
    emit_dd(out, "adversarial", 1.0, 3.0, 53, 53, MPFR_RNDD);
    emit_dd(out, "adversarial", DBL_MAX, 0.5, 53, 53, MPFR_RNDU);
    emit_dd(out, "adversarial", DBL_MAX, 0.5, 53, 53, MPFR_RNDZ);
    emit_dd(out, "adversarial", -DBL_MAX, 0.5, 53, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", DBL_MIN, 2.0, 53, 53, MPFR_RNDZ);
    emit_dd(out, "adversarial", 1.0, 7.0, 53, 1, MPFR_RNDN);
    emit_dd(out, "adversarial", -1.0, 7.0, 53, 1, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 7.0, 53, 1, MPFR_RNDU);

    /* fuzz: 60 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xD1D2D3D4D5D6D7D8ULL);
        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 256);
            const uint64_t cprec = 1 + xs64_below(&rng, 256);
            const uint64_t r1 = xs64_next(&rng);
            double b = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            const uint64_t r2 = xs64_next(&rng);
            double cd = ((double)((r2 % 199998ULL) + 1) - 99999.0) / 100.0;
            if (cd == 0.0) cd = 1.0;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            emit_dd(out, "fuzz", b, cd, cprec, prec, RNDS[rnd_idx]);
        }
    }

    /* mined: 5 */
    emit_dd(out, "mined", 6.0, 2.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "mined", 1.0, 3.0, 53, 53, MPFR_RNDU);
    emit_dd(out, "mined", 0.0, 5.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "mined", -1.0, 2.0, 53, 53, MPFR_RNDN);
    emit_dd(out, "mined", 1.5e10, 2.0, 53, 53, MPFR_RNDU);

    return 0;
}
