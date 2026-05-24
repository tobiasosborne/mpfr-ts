/*
 * golden_driver.c -- Golden master for MPFR's mpfr_get_d_2exp.
 *
 * Decomposes x into (mantissa, exp) with |mantissa| in [0.5, 1.0) for
 * normal x, and (0/+/-Inf/NaN, 0) for singulars. C signature passes exp
 * via long*; we marshall to a wire object { "value": <double>, "exp":
 * "<int64 dec>" }.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR>,"rnd":"RND[NZUDA]"},
 *    "output":{"value":"<NaN|+/-Infinity|%.17g>","exp":"<int64 dec>"},
 *    "time_ns":<n>}
 *
 * Tag distribution: happy 22, edge 40, adversarial 12, fuzz 60, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_get_d_2exp golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, mpfr_rnd_t rnd) {
    long e = 0;
    const uint64_t t0 = now_ns();
    const double v = mpfr_get_d_2exp(&e, x, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_begin_object(out);
    jl_kv_double(out, 1, "value", v);
    jl_kv_i64(out, 0, "exp", (int64_t)e);
    jl_output_end_object(out);
    jl_finish(out, elapsed);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_pos_inf(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, 1); }
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, -1); }
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, 1); }
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, -1); }
static inline void init_nan(mpfr_ptr x, uint64_t prec)     { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_nan(x); }

#define E(tag, ie, rnd) do { mpfr_t _x; ie; emit_case(out, tag, _x, rnd); mpfr_clear(_x); } while (0)

int main(void) {
    FILE *out = stdout;

    /* happy: 22 -- typical normal values, RNDN. */
    E("happy", init_from_double(_x, 1.0, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, 2.0, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, 0.5, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, 3.14, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, -3.14, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, 1e10, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, 1e-10, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, 1e100, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, 1e-100, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, -1.0, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, 0.75, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, 0.875, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, 1.5, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, 100.0, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, 1.0/3.0, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, -1.0/7.0, 100), MPFR_RNDN);
    E("happy", init_from_double(_x, 1.0, 100), MPFR_RNDN);
    E("happy", init_from_double(_x, 1.0, 1), MPFR_RNDN);
    E("happy", init_from_double(_x, 2.71, 200), MPFR_RNDN);
    E("happy", init_from_double(_x, 1.0/3.0, 24), MPFR_RNDN);
    E("happy", init_from_double(_x, -0.5, 53), MPFR_RNDN);
    E("happy", init_from_double(_x, 42.0, 53), MPFR_RNDN);

    /* edge: 40 -- specials x rnd, prec extremes, near-half boundaries. */
    /* NaN under each rnd. */
    for (int i = 0; i < 5; ++i) E("edge", init_nan(_x, 53), RNDS[i]);
    /* +Inf under each rnd. */
    for (int i = 0; i < 5; ++i) E("edge", init_pos_inf(_x, 53), RNDS[i]);
    /* -Inf under each rnd. */
    for (int i = 0; i < 5; ++i) E("edge", init_neg_inf(_x, 53), RNDS[i]);
    /* +0 under each rnd. */
    for (int i = 0; i < 5; ++i) E("edge", init_pos_zero(_x, 53), RNDS[i]);
    /* -0 under each rnd. */
    for (int i = 0; i < 5; ++i) E("edge", init_neg_zero(_x, 53), RNDS[i]);
    /* Normal values under each rnd. */
    for (int i = 0; i < 5; ++i) E("edge", init_from_double(_x, 1.0/3.0, 53), RNDS[i]);
    for (int i = 0; i < 5; ++i) E("edge", init_from_double(_x, -1.0/3.0, 53), RNDS[i]);
    /* Power-of-two boundaries -- mantissa exactly 0.5. */
    E("edge", init_from_double(_x, 4.0, 53), MPFR_RNDN);
    E("edge", init_from_double(_x, 8.0, 53), MPFR_RNDN);
    E("edge", init_from_double(_x, 16.0, 53), MPFR_RNDN);
    E("edge", init_from_double(_x, -16.0, 53), MPFR_RNDN);
    E("edge", init_from_double(_x, 1024.0, 53), MPFR_RNDN);

    /* adversarial: 12 -- near-1.0 boundary that may bump exp. */
    E("adversarial", init_from_double(_x, 0.999999999999999, 53), MPFR_RNDN);
    E("adversarial", init_from_double(_x, 0.999999999999999, 53), MPFR_RNDU);
    E("adversarial", init_from_double(_x, -0.999999999999999, 53), MPFR_RNDD);
    E("adversarial", init_from_double(_x, 1.0, 1), MPFR_RNDU);
    E("adversarial", init_from_double(_x, 1.0, 1), MPFR_RNDN);
    E("adversarial", init_from_double(_x, 1.5, 2), MPFR_RNDN);
    E("adversarial", init_from_double(_x, 1.5, 2), MPFR_RNDU);
    E("adversarial", init_from_double(_x, -1.5, 2), MPFR_RNDD);
    E("adversarial", init_from_double(_x, 1.0, 4096), MPFR_RNDN);
    E("adversarial", init_from_double(_x, DBL_MAX, 53), MPFR_RNDN);
    E("adversarial", init_from_double(_x, DBL_MIN, 53), MPFR_RNDN);
    E("adversarial", init_from_double(_x, DBL_EPSILON, 53), MPFR_RNDN);

    /* fuzz: 60 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x9999999999999999ULL);
        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 256);
            const uint64_t r1 = xs64_next(&rng);
            double d = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            mpfr_t x;
            init_from_double(x, d, prec);
            emit_case(out, "fuzz", x, RNDS[rnd_idx]);
            mpfr_clear(x);
        }
    }

    /* mined: 5 -- drawn from mpfr/tests/tget_d_2exp.c shapes. */
    E("mined", init_from_double(_x, 1.0, 53), MPFR_RNDN);
    E("mined", init_from_double(_x, 0.5, 53), MPFR_RNDN);
    E("mined", init_pos_inf(_x, 53), MPFR_RNDN);
    E("mined", init_pos_zero(_x, 53), MPFR_RNDN);
    E("mined", init_nan(_x, 53), MPFR_RNDN);

    return 0;
}
