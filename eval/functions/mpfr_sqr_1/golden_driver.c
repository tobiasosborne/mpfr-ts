/*
 * golden_driver.c — Golden master for MPFR's mpfr_sqr_1.
 *
 * mpfr_sqr_1 is `static` in mpfr/src/sqr.c; we exercise it via the public
 * mpfr_sqr with arguments satisfying:
 *   prec(b) == prec(a) == p < GMP_NUMB_BITS (= 64)
 * which the dispatcher (sqr.c L539-L554) routes to mpfr_sqr_1.
 *
 * Tag distribution: happy 22, edge 30, adversarial 12, fuzz 60, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sqr_1 golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr b, uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= 1 && prec < 64);
    assert(mpfr_get_prec(b) == (mpfr_prec_t)prec);
    assert(mpfr_regular_p(b));
    mpfr_t a; mpfr_init2(a, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_sqr(a, b, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "b", b);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, a, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(a);
}

static inline void emit_d(FILE *out, const char *tag,
                          double bd, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t b; mpfr_init2(b, (mpfr_prec_t)prec); mpfr_set_d(b, bd, MPFR_RNDN);
    if (!mpfr_regular_p(b)) { mpfr_clear(b); return; }
    emit_case(out, tag, b, prec, rnd);
    mpfr_clear(b);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    emit_d(out, "happy", 1.0, 53, MPFR_RNDN);
    emit_d(out, "happy", 2.0, 53, MPFR_RNDN);
    emit_d(out, "happy", 3.0, 53, MPFR_RNDN);
    emit_d(out, "happy", -3.0, 53, MPFR_RNDN);   /* (-3)^2 = +9 */
    emit_d(out, "happy", 0.5, 53, MPFR_RNDN);
    emit_d(out, "happy", 3.14, 53, MPFR_RNDN);
    emit_d(out, "happy", -3.14, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.5, 53, MPFR_RNDN);
    emit_d(out, "happy", 2.5, 53, MPFR_RNDZ);
    emit_d(out, "happy", 1.0, 24, MPFR_RNDN);
    emit_d(out, "happy", 1.0/3.0, 24, MPFR_RNDN);
    emit_d(out, "happy", 1.0/3.0, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0/7.0, 53, MPFR_RNDU);
    emit_d(out, "happy", 7.0, 8, MPFR_RNDN);
    emit_d(out, "happy", 11.0, 8, MPFR_RNDA);
    emit_d(out, "happy", 100.0, 32, MPFR_RNDN);
    emit_d(out, "happy", -100.0, 32, MPFR_RNDA);
    emit_d(out, "happy", 1.5, 53, MPFR_RNDD);
    emit_d(out, "happy", 1.5, 53, MPFR_RNDU);
    emit_d(out, "happy", 0.1, 53, MPFR_RNDN);
    emit_d(out, "happy", 0.1, 24, MPFR_RNDN);
    emit_d(out, "happy", 1e5, 53, MPFR_RNDN);

    /* edge: 30 — prec extremes (1, 63), all rnd, mantissa boundaries. */
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.0, 1, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", -1.0, 1, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.0, 63, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 3.14, 8, RNDS[i]);
    emit_d(out, "edge", 1.5, 2, MPFR_RNDN);
    emit_d(out, "edge", 1.5, 2, MPFR_RNDU);
    emit_d(out, "edge", 1.5, 2, MPFR_RNDD);
    emit_d(out, "edge", 1.5, 2, MPFR_RNDZ);
    emit_d(out, "edge", 1.5, 2, MPFR_RNDA);
    emit_d(out, "edge", 1.0, 60, MPFR_RNDN);
    emit_d(out, "edge", -1.0, 60, MPFR_RNDD);
    emit_d(out, "edge", 1e10, 32, MPFR_RNDN);
    emit_d(out, "edge", 1e-10, 32, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 63, MPFR_RNDN);
    emit_d(out, "edge", 1.5, 63, MPFR_RNDN);

    /* adversarial: 12 — rounding-boundary cases. */
    emit_d(out, "adversarial", 0.99999999, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 0.99999999, 53, MPFR_RNDU);
    emit_d(out, "adversarial", -0.99999999, 53, MPFR_RNDD);
    emit_d(out, "adversarial", 1.0/3.0, 4, MPFR_RNDN);
    emit_d(out, "adversarial", 1.0/3.0, 4, MPFR_RNDU);
    emit_d(out, "adversarial", 1.0/3.0, 4, MPFR_RNDD);
    emit_d(out, "adversarial", 0.1, 8, MPFR_RNDN);
    emit_d(out, "adversarial", 0.1, 8, MPFR_RNDU);
    emit_d(out, "adversarial", 1.1, 16, MPFR_RNDN);
    emit_d(out, "adversarial", 1.0 + 1e-15, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 2.0 - 1e-15, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 1e8, 53, MPFR_RNDU);

    /* fuzz: 60 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5071A105071A1050ULL);
        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 63);
            const uint64_t r1 = xs64_next(&rng);
            double bd = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            if (bd == 0.0) bd = 1.0;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            emit_d(out, "fuzz", bd, prec, RNDS[rnd_idx]);
        }
    }

    /* mined: 5 */
    emit_d(out, "mined", 1.0, 53, MPFR_RNDN);
    emit_d(out, "mined", 3.14, 53, MPFR_RNDN);
    emit_d(out, "mined", -1.5, 24, MPFR_RNDN);
    emit_d(out, "mined", 0.5, 53, MPFR_RNDN);
    emit_d(out, "mined", 7.0, 8, MPFR_RNDU);

    return 0;
}
