/*
 * golden_driver.c -- Golden master for MPFR's mpfr_add_d.
 *
 * The C function mpfr_add_d is a trivial wrapper around mpfr_set_d +
 * mpfr_add (mpfr/src/add_d.c L25-L50). The golden therefore exercises
 * the composite behaviour: any input (b, c, prec, rnd) is graded
 * against libmpfr's mpfr_add_d output.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"b":<MPFR>,"c":"<double>","prec":"<dec>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 *   - b via jl_kv_mpfr (full MPFR record).
 *   - c via jl_kv_double (quoted string for NaN/+/-Inf, "%.17g" for finite).
 *   - prec via jl_kv_u64 (decimal string).
 *   - rnd via jl_kv_rnd.
 *   - Output via jl_output_result.
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  22
 *   edge         :  32
 *   adversarial  :  12
 *   fuzz         :  60
 *   mined        :   5
 *
 * Ref: mpfr/src/add_d.c -- C reference.
 * Ref: src/ops/add_d.ts -- production port.
 * Ref: mpfr/tests/tadd_d.c -- mined source.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_add_d golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_CAP ((uint64_t)4096)

static const mpfr_rnd_t RNDS[5] = {
    MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA,
};

/* Construct a -0 double via bit-level memcpy (robust under -ffast-math). */
static inline double make_neg_zero(void) {
    const uint64_t bits = (uint64_t)1 << 63;
    double d;
    memcpy(&d, &bits, sizeof d);
    return d;
}

/* Emit one case at (b, c, prec, rnd). */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr b, double c,
                             uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t a;
    mpfr_init2(a, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_add_d(a, b, c, rnd);
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

/* Build an MPFR from a double at the given prec, then emit. */
static inline void emit_dd(FILE *out, const char *tag,
                           double bd, uint64_t bprec,
                           double c, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t b;
    mpfr_init2(b, (mpfr_prec_t)bprec);
    mpfr_set_d(b, bd, MPFR_RNDN);
    emit_case(out, tag, b, c, prec, rnd);
    mpfr_clear(b);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 22 -- common pairs at prec=53.                            */
    /* ============================================================== */
    emit_dd(out, "happy", 1.0, 53, 1.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 53, 2.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 3.14, 53, 2.71, 53, MPFR_RNDN);
    emit_dd(out, "happy", -1.5, 53, -2.5, 53, MPFR_RNDN);
    emit_dd(out, "happy", 0.5, 53, 0.25, 53, MPFR_RNDZ);
    emit_dd(out, "happy", 1.0, 24, 1.0, 24, MPFR_RNDN);
    emit_dd(out, "happy", -3.14, 24, -1.5, 24, MPFR_RNDD);
    emit_dd(out, "happy", 100.0, 100, 0.5, 100, MPFR_RNDN);
    emit_dd(out, "happy", 1e10, 53, 1.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 53, 1e10, 53, MPFR_RNDN);
    emit_dd(out, "happy", 4.0, 53, 1.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", -8.0, 53, -1.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 100.5, 32, 200.25, 32, MPFR_RNDA);
    emit_dd(out, "happy", -100.5, 32, -200.25, 32, MPFR_RNDA);
    emit_dd(out, "happy", 0.1, 200, 0.2, 200, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 53, -1.0, 53, MPFR_RNDN);   /* exact cancel -> +0 */
    emit_dd(out, "happy", -1.0, 53, 1.0, 53, MPFR_RNDD);   /* exact cancel -> -0 (RNDD) */
    emit_dd(out, "happy", 2.0, 53, -1.5, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.5, 53, 2.5, 53, MPFR_RNDN);
    emit_dd(out, "happy", 7.0, 53, 7.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1e-100, 53, 1e-100, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1e100, 53, 1e100, 53, MPFR_RNDN);

    /* ============================================================== */
    /* edge: 32 -- specials, prec extremes, signed zero.                */
    /* ============================================================== */
    /* NaN inputs propagate. */
    {
        mpfr_t b; mpfr_init2(b, 53); mpfr_set_nan(b);
        emit_case(out, "edge", b, 1.0, 53, MPFR_RNDN);
        mpfr_clear(b);
    }
    emit_dd(out, "edge", 1.0, 53, NAN, 53, MPFR_RNDN);
    /* +/-Inf. */
    {
        mpfr_t b; mpfr_init2(b, 53); mpfr_set_inf(b, +1);
        emit_case(out, "edge", b, 1.0, 53, MPFR_RNDN);
        mpfr_clear(b);
    }
    {
        mpfr_t b; mpfr_init2(b, 53); mpfr_set_inf(b, -1);
        emit_case(out, "edge", b, 1.0, 53, MPFR_RNDN);
        mpfr_clear(b);
    }
    {
        mpfr_t b; mpfr_init2(b, 53); mpfr_set_inf(b, +1);
        emit_case(out, "edge", b, -INFINITY, 53, MPFR_RNDN); /* +Inf + -Inf = NaN */
        mpfr_clear(b);
    }
    emit_dd(out, "edge", 1.0, 53, INFINITY, 53, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 53, -INFINITY, 53, MPFR_RNDN);
    /* Signed zero. */
    {
        mpfr_t b; mpfr_init2(b, 53); mpfr_set_zero(b, +1);
        emit_case(out, "edge", b, make_neg_zero(), 53, MPFR_RNDN);  /* +0 + -0 = +0 RNDN */
        mpfr_clear(b);
    }
    {
        mpfr_t b; mpfr_init2(b, 53); mpfr_set_zero(b, +1);
        emit_case(out, "edge", b, make_neg_zero(), 53, MPFR_RNDD);  /* +0 + -0 = -0 RNDD */
        mpfr_clear(b);
    }
    {
        mpfr_t b; mpfr_init2(b, 53); mpfr_set_zero(b, -1);
        emit_case(out, "edge", b, make_neg_zero(), 53, MPFR_RNDN);  /* -0 + -0 = -0 */
        mpfr_clear(b);
    }
    /* prec=1 / prec=2 boundaries. */
    for (int i = 0; i < 5; i++) emit_dd(out, "edge", 1.0, 1, 1.0, 1, RNDS[i]);
    for (int i = 0; i < 5; i++) emit_dd(out, "edge", 1.5, 53, 0.25, 2, RNDS[i]);
    /* PREC_CAP. */
    emit_dd(out, "edge", 3.14, TS_PREC_CAP, 2.71, TS_PREC_CAP, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 1000, 1.0, 1000, MPFR_RNDN);
    /* Very large / very small magnitudes. */
    emit_dd(out, "edge", DBL_MAX, 53, DBL_MAX, 53, MPFR_RNDN);
    emit_dd(out, "edge", DBL_MIN, 53, DBL_MIN, 53, MPFR_RNDN);
    /* Mixed-prec b. */
    emit_dd(out, "edge", 3.14, 200, 2.71, 53, MPFR_RNDN);
    emit_dd(out, "edge", 3.14, 24, 2.71, 200, MPFR_RNDN);
    /* +0 input b, normal c. */
    {
        mpfr_t b; mpfr_init2(b, 53); mpfr_set_zero(b, +1);
        emit_case(out, "edge", b, 3.14, 53, MPFR_RNDN);
        mpfr_clear(b);
    }
    /* c=0.0 input. */
    emit_dd(out, "edge", 3.14, 53, 0.0, 53, MPFR_RNDN);
    /* c=-0.0 input. */
    emit_dd(out, "edge", 3.14, 53, make_neg_zero(), 53, MPFR_RNDN);
    /* Both rounding modes for an inexact pair. */
    emit_dd(out, "edge", 1.0/3.0, 53, 1.0/7.0, 24, MPFR_RNDZ);
    emit_dd(out, "edge", 1.0/3.0, 53, 1.0/7.0, 24, MPFR_RNDU);
    emit_dd(out, "edge", 1.0/3.0, 53, 1.0/7.0, 24, MPFR_RNDD);
    emit_dd(out, "edge", 1.0/3.0, 53, 1.0/7.0, 24, MPFR_RNDA);

    /* ============================================================== */
    /* adversarial: 12 -- rounding boundary cases, ULP traps.           */
    /* ============================================================== */
    emit_dd(out, "adversarial", 0.9999999999999999, 53, 1.0, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", -0.9999999999999999, 53, -1.0, 53, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 53, DBL_EPSILON / 2.0, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.0, 53, DBL_EPSILON / 2.0, 53, MPFR_RNDU);
    emit_dd(out, "adversarial", 1.0, 53, DBL_EPSILON / 2.0, 53, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 53, DBL_EPSILON, 53, MPFR_RNDN);
    /* Near-overflow target prec; result should round to +/-Inf for some rnd. */
    emit_dd(out, "adversarial", DBL_MAX, 53, DBL_MAX, 53, MPFR_RNDU);
    emit_dd(out, "adversarial", DBL_MAX, 53, DBL_MAX, 53, MPFR_RNDD);
    emit_dd(out, "adversarial", DBL_MAX, 53, DBL_MAX, 53, MPFR_RNDZ);
    /* Catastrophic-cancellation pair. */
    emit_dd(out, "adversarial", 1.0, 53, -1.0 + DBL_EPSILON, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", 1e16, 53, 1.0, 24, MPFR_RNDN);
    emit_dd(out, "adversarial", 1e16, 53, 1.0, 24, MPFR_RNDU);

    /* ============================================================== */
    /* fuzz: 60 -- PRNG-driven.                                         */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xADDD0DD0DEADBEEFULL);

        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 256);
            const uint64_t bprec = 1 + xs64_below(&rng, 256);
            /* Doubles drawn from a moderate magnitude band; sign random. */
            const uint64_t r1 = xs64_next(&rng);
            double bd = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            const uint64_t r2 = xs64_next(&rng);
            double c  = ((double)(r2 % 200000ULL) - 100000.0) / 100.0;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            emit_dd(out, "fuzz", bd, bprec, c, prec, RNDS[rnd_idx]);
        }
    }

    /* ============================================================== */
    /* mined: 5 -- drawn from mpfr/tests/tadd_d.c shapes.               */
    /* ============================================================== */
    emit_dd(out, "mined", 123.0, 53, 456.0, 53, MPFR_RNDN);
    emit_dd(out, "mined", 1.0, 53, 1.0, 53, MPFR_RNDZ);
    emit_dd(out, "mined", 0.0, 53, 0.0, 53, MPFR_RNDN);
    emit_dd(out, "mined", -1.0, 53, 1.0, 53, MPFR_RNDN);
    emit_dd(out, "mined", 1.5e10, 53, 2.5e10, 53, MPFR_RNDU);

    return 0;
}
