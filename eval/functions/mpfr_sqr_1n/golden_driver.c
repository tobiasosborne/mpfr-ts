/*
 * golden_driver.c — Golden master for MPFR's mpfr_sqr_1n.
 *
 * mpfr_sqr_1n is `static` in mpfr/src/sqr.c; exercised via public mpfr_sqr
 * with prec(b) == prec(a) == 64 (the dispatcher routes prec=64 here per
 * mpfr/src/sqr.c L548).
 *
 * Tag distribution: happy 22, edge 30, adversarial 12, fuzz 55, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sqr_1n golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr b, mpfr_rnd_t rnd) {
    assert(mpfr_get_prec(b) == 64);
    assert(mpfr_regular_p(b));
    mpfr_t a; mpfr_init2(a, 64);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_sqr(a, b, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "b", b);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, a, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(a);
}

static inline void emit_d(FILE *out, const char *tag,
                          double bd, mpfr_rnd_t rnd) {
    mpfr_t b; mpfr_init2(b, 64); mpfr_set_d(b, bd, MPFR_RNDN);
    if (!mpfr_regular_p(b)) { mpfr_clear(b); return; }
    emit_case(out, tag, b, rnd);
    mpfr_clear(b);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    emit_d(out, "happy", 1.0, MPFR_RNDN);
    emit_d(out, "happy", 2.0, MPFR_RNDN);
    emit_d(out, "happy", 3.0, MPFR_RNDN);
    emit_d(out, "happy", -3.0, MPFR_RNDN);
    emit_d(out, "happy", 0.5, MPFR_RNDN);
    emit_d(out, "happy", 3.14, MPFR_RNDN);
    emit_d(out, "happy", -3.14, MPFR_RNDN);
    emit_d(out, "happy", 1.5, MPFR_RNDN);
    emit_d(out, "happy", 2.5, MPFR_RNDZ);
    emit_d(out, "happy", 1.0/3.0, MPFR_RNDN);
    emit_d(out, "happy", 1.0/7.0, MPFR_RNDU);
    emit_d(out, "happy", 7.0, MPFR_RNDN);
    emit_d(out, "happy", 11.0, MPFR_RNDA);
    emit_d(out, "happy", 100.0, MPFR_RNDN);
    emit_d(out, "happy", -100.0, MPFR_RNDA);
    emit_d(out, "happy", 1.5, MPFR_RNDD);
    emit_d(out, "happy", 1.5, MPFR_RNDU);
    emit_d(out, "happy", 0.1, MPFR_RNDN);
    emit_d(out, "happy", 1e5, MPFR_RNDN);
    emit_d(out, "happy", 1e-5, MPFR_RNDN);
    emit_d(out, "happy", 1e10, MPFR_RNDN);
    emit_d(out, "happy", -1e10, MPFR_RNDN);

    /* edge: 30 — all rnd, various magnitudes. */
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.0, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", -1.0, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.5, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.0/3.0, RNDS[i]);
    emit_d(out, "edge", 0.99999999, MPFR_RNDN);
    emit_d(out, "edge", 0.99999999, MPFR_RNDU);
    emit_d(out, "edge", 2.0 - 1e-15, MPFR_RNDN);
    emit_d(out, "edge", 1e100, MPFR_RNDN);
    emit_d(out, "edge", 1e-100, MPFR_RNDN);
    emit_d(out, "edge", 1.0 + 1e-15, MPFR_RNDN);
    emit_d(out, "edge", 1.0 - 1e-15, MPFR_RNDN);
    emit_d(out, "edge", 2.0, MPFR_RNDA);
    emit_d(out, "edge", 4.0, MPFR_RNDN);
    emit_d(out, "edge", 0.25, MPFR_RNDN);

    /* adversarial: 12 */
    emit_d(out, "adversarial", 0.9999999999, MPFR_RNDU);
    emit_d(out, "adversarial", 0.9999999999, MPFR_RNDD);
    emit_d(out, "adversarial", -0.9999999999, MPFR_RNDU);
    emit_d(out, "adversarial", 1.0/3.0, MPFR_RNDA);
    emit_d(out, "adversarial", 1.0/7.0, MPFR_RNDD);
    emit_d(out, "adversarial", 1.0/11.0, MPFR_RNDU);
    emit_d(out, "adversarial", 1.5, MPFR_RNDA);
    emit_d(out, "adversarial", 2.5, MPFR_RNDD);
    emit_d(out, "adversarial", 3.5, MPFR_RNDU);
    emit_d(out, "adversarial", 1e8, MPFR_RNDN);
    emit_d(out, "adversarial", 1e8, MPFR_RNDU);
    emit_d(out, "adversarial", -1e8, MPFR_RNDD);

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x6071A206071A2060ULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t r1 = xs64_next(&rng);
            double bd = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            if (bd == 0.0) bd = 1.0;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            emit_d(out, "fuzz", bd, RNDS[rnd_idx]);
        }
    }

    /* mined: 5 */
    emit_d(out, "mined", 1.0, MPFR_RNDN);
    emit_d(out, "mined", 3.14, MPFR_RNDN);
    emit_d(out, "mined", -1.5, MPFR_RNDN);
    emit_d(out, "mined", 0.5, MPFR_RNDN);
    emit_d(out, "mined", 1.0/3.0, MPFR_RNDU);

    return 0;
}
