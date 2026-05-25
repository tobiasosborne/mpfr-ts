/*
 * golden_driver.c -- Golden master for MPFR's mpfr_cbrt.
 *
 * C: int mpfr_cbrt(mpfr_t y, mpfr_srcptr x, mpfr_rnd_t rnd)
 *    Sets y = x^(1/3) at prec=MPFR_PREC(y), rounded per rnd.
 *    Ref: mpfr/src/cbrt.c L47-L175.
 *
 * Wire: {"inputs":{"x":<mpfr>,"prec":"<dec>","rnd":"RND_"},
 *        "output":{"value":<mpfr>,"ternary":<int>}}.
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * Class: transcendental (200ms per-case budget).
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_cbrt golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define MAX_PREC 256

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= 1 && prec <= MAX_PREC);
    mpfr_t y;
    mpfr_init2(y, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_cbrt(y, x, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, y, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(y);
}

static inline void emit_d(FILE *out, const char *tag, double d, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, d, MPFR_RNDN);
    emit_case(out, tag, x, prec, rnd);
    mpfr_clear(x);
}

static inline void emit_inf(FILE *out, const char *tag, int sign, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, sign);
    emit_case(out, tag, x, prec, rnd);
    mpfr_clear(x);
}

static inline void emit_zero(FILE *out, const char *tag, int sign, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, sign);
    emit_case(out, tag, x, prec, rnd);
    mpfr_clear(x);
}

static inline void emit_nan(FILE *out, const char *tag, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x);
    emit_case(out, tag, x, prec, rnd);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = { MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA };

    /* happy: 20 -- common cubes and near-cubes. */
    emit_d(out, "happy", 1.0, 53, MPFR_RNDN);    /* cbrt(1) = 1 exact */
    emit_d(out, "happy", 8.0, 53, MPFR_RNDN);    /* cbrt(8) = 2 exact */
    emit_d(out, "happy", 27.0, 53, MPFR_RNDN);   /* cbrt(27) = 3 exact */
    emit_d(out, "happy", 64.0, 53, MPFR_RNDN);
    emit_d(out, "happy", 125.0, 53, MPFR_RNDN);
    emit_d(out, "happy", 1000.0, 53, MPFR_RNDN);
    emit_d(out, "happy", 2.0, 53, MPFR_RNDN);
    emit_d(out, "happy", 3.0, 53, MPFR_RNDN);
    emit_d(out, "happy", 0.5, 53, MPFR_RNDN);
    emit_d(out, "happy", 0.125, 53, MPFR_RNDN);
    emit_d(out, "happy", -8.0, 53, MPFR_RNDN);
    emit_d(out, "happy", -1.0, 53, MPFR_RNDN);
    emit_d(out, "happy", -27.0, 53, MPFR_RNDN);
    emit_d(out, "happy", -2.0, 53, MPFR_RNDN);
    emit_d(out, "happy", 100.0, 24, MPFR_RNDN);
    emit_d(out, "happy", 100.0, 64, MPFR_RNDN);
    emit_d(out, "happy", 1e6, 53, MPFR_RNDN);
    emit_d(out, "happy", 1e-6, 53, MPFR_RNDN);
    emit_d(out, "happy", 0.001, 53, MPFR_RNDZ);
    emit_d(out, "happy", 2.0, 53, MPFR_RNDU);

    /* edge: 30 -- singulars + sign symmetries + prec extremes. */
    emit_nan(out, "edge", 53, MPFR_RNDN);
    emit_nan(out, "edge", 1, MPFR_RNDN);
    emit_inf(out, "edge", +1, 53, MPFR_RNDN);
    emit_inf(out, "edge", -1, 53, MPFR_RNDN);
    emit_inf(out, "edge", +1, 1, MPFR_RNDN);
    emit_inf(out, "edge", -1, 1, MPFR_RNDN);
    emit_zero(out, "edge", +1, 53, MPFR_RNDN);
    emit_zero(out, "edge", -1, 53, MPFR_RNDN);
    emit_zero(out, "edge", +1, 1, MPFR_RNDN);
    emit_zero(out, "edge", -1, 1, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 1, MPFR_RNDN);
    emit_d(out, "edge", 8.0, 1, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 53, MPFR_RNDZ);
    emit_d(out, "edge", 1.0, 53, MPFR_RNDU);
    emit_d(out, "edge", 1.0, 53, MPFR_RNDD);
    emit_d(out, "edge", 1.0, 53, MPFR_RNDA);
    emit_d(out, "edge", 2.0, 24, MPFR_RNDN);
    emit_d(out, "edge", 2.0, 53, MPFR_RNDN);
    emit_d(out, "edge", 2.0, 64, MPFR_RNDN);
    emit_d(out, "edge", 2.0, 128, MPFR_RNDN);
    emit_d(out, "edge", 2.0, 256, MPFR_RNDN);
    emit_d(out, "edge", -2.0, 53, MPFR_RNDN);
    emit_d(out, "edge", -2.0, 53, MPFR_RNDZ);
    emit_d(out, "edge", -2.0, 53, MPFR_RNDU);
    emit_d(out, "edge", -2.0, 53, MPFR_RNDD);
    emit_d(out, "edge", -2.0, 53, MPFR_RNDA);
    emit_d(out, "edge", 1e10, 53, MPFR_RNDN);
    emit_d(out, "edge", 1e-10, 53, MPFR_RNDN);
    emit_d(out, "edge", 0.5, 1, MPFR_RNDN);
    emit_d(out, "edge", 0.5, 2, MPFR_RNDN);

    /* adversarial: 12 -- known-tricky cubes / near-tie rounding. */
    emit_d(out, "adversarial", 7.0, 3, MPFR_RNDN);    /* cbrt(7) near tie at low prec */
    emit_d(out, "adversarial", 9.0, 3, MPFR_RNDN);
    emit_d(out, "adversarial", 0.3333333333333333, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 2.718281828459045, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 1.4142135623730951, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 1e100, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 1e-100, 53, MPFR_RNDN);
    emit_d(out, "adversarial", -1e10, 53, MPFR_RNDU);
    emit_d(out, "adversarial", -1e10, 53, MPFR_RNDD);
    emit_d(out, "adversarial", 1.0 / 9.0, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 1.0 / 9.0, 100, MPFR_RNDN);
    emit_d(out, "adversarial", 1.0 / 27.0, 53, MPFR_RNDN);

    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xCBCBCBCBABEFACE0ULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, MAX_PREC);
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            const uint64_t bits = xs64_next(&rng);
            /* Map bits to a positive double in (1e-30, 1e30). */
            double d = ((double)bits / (double)UINT64_MAX) * 60.0 - 30.0;
            d = pow(10.0, d);
            if (xs64_below(&rng, 2)) d = -d;
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, d, MPFR_RNDN);
            emit_case(out, "fuzz", x, prec, rnd);
            mpfr_clear(x);
        }
    }

    /* mined: 5 -- canonical tcbrt.c patterns. */
    emit_d(out, "mined", 1.0, 53, MPFR_RNDN);
    emit_d(out, "mined", 8.0, 53, MPFR_RNDN);
    emit_d(out, "mined", -27.0, 53, MPFR_RNDN);
    emit_d(out, "mined", 0.125, 53, MPFR_RNDN);
    emit_zero(out, "mined", -1, 53, MPFR_RNDN);

    return 0;
}
