/*
 * golden_driver.c — Golden master for MPFR's mpfr_sqr_2.
 *
 * mpfr_sqr_2 is `static` in mpfr/src/sqr.c; exercised via public mpfr_sqr
 * with 64 < prec(b) == prec(a) < 128 (dispatcher routes here per
 * mpfr/src/sqr.c L545-L546).
 *
 * Tag distribution: happy 22, edge 30, adversarial 12, fuzz 55, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sqr_2 golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr b, uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec > 64 && prec < 128);
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
    emit_d(out, "happy", 1.0, 100, MPFR_RNDN);
    emit_d(out, "happy", 2.0, 100, MPFR_RNDN);
    emit_d(out, "happy", 3.0, 100, MPFR_RNDN);
    emit_d(out, "happy", -3.0, 100, MPFR_RNDN);
    emit_d(out, "happy", 0.5, 100, MPFR_RNDN);
    emit_d(out, "happy", 3.14, 100, MPFR_RNDN);
    emit_d(out, "happy", -3.14, 100, MPFR_RNDN);
    emit_d(out, "happy", 1.5, 100, MPFR_RNDN);
    emit_d(out, "happy", 1.0/3.0, 100, MPFR_RNDN);
    emit_d(out, "happy", 1.0/7.0, 100, MPFR_RNDU);
    emit_d(out, "happy", 7.0, 100, MPFR_RNDN);
    emit_d(out, "happy", 100.0, 100, MPFR_RNDN);
    emit_d(out, "happy", -100.0, 100, MPFR_RNDA);
    emit_d(out, "happy", 1.5, 100, MPFR_RNDD);
    emit_d(out, "happy", 1.5, 100, MPFR_RNDU);
    emit_d(out, "happy", 0.1, 100, MPFR_RNDN);
    emit_d(out, "happy", 1e5, 100, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 96, MPFR_RNDN);
    emit_d(out, "happy", 1.0/3.0, 96, MPFR_RNDN);
    emit_d(out, "happy", 3.14, 80, MPFR_RNDN);
    emit_d(out, "happy", -3.14, 80, MPFR_RNDN);
    emit_d(out, "happy", 2.5, 96, MPFR_RNDZ);

    /* edge: 30 — prec extremes (65, 127), all rnd. */
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.0, 65, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.0, 127, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", -3.14, 100, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.0/3.0, 100, RNDS[i]);
    emit_d(out, "edge", 0.99999999, 100, MPFR_RNDN);
    emit_d(out, "edge", 0.99999999, 100, MPFR_RNDU);
    emit_d(out, "edge", 2.0 - 1e-15, 100, MPFR_RNDN);
    emit_d(out, "edge", 1e100, 100, MPFR_RNDN);
    emit_d(out, "edge", 1e-100, 100, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 65, MPFR_RNDA);
    emit_d(out, "edge", 1.0, 127, MPFR_RNDA);
    emit_d(out, "edge", 1.5, 65, MPFR_RNDN);
    emit_d(out, "edge", 1.5, 127, MPFR_RNDN);
    emit_d(out, "edge", -1.5, 100, MPFR_RNDD);
    emit_d(out, "edge", 4.0, 100, MPFR_RNDN);

    /* adversarial: 12 */
    emit_d(out, "adversarial", 0.9999999999999999, 100, MPFR_RNDU);
    emit_d(out, "adversarial", 0.9999999999999999, 100, MPFR_RNDD);
    emit_d(out, "adversarial", 1.0/3.0, 65, MPFR_RNDN);
    emit_d(out, "adversarial", 1.0/3.0, 65, MPFR_RNDU);
    emit_d(out, "adversarial", 1.0/7.0, 96, MPFR_RNDD);
    emit_d(out, "adversarial", 1.5, 96, MPFR_RNDA);
    emit_d(out, "adversarial", 2.5, 100, MPFR_RNDD);
    emit_d(out, "adversarial", 3.5, 100, MPFR_RNDU);
    emit_d(out, "adversarial", 1e8, 100, MPFR_RNDN);
    emit_d(out, "adversarial", 1e8, 100, MPFR_RNDU);
    emit_d(out, "adversarial", 1.0 + 1e-15, 100, MPFR_RNDN);
    emit_d(out, "adversarial", 1.0 - 1e-15, 100, MPFR_RNDN);

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x7071A307071A3070ULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t prec = 65 + xs64_below(&rng, 63);  /* 65..127 */
            const uint64_t r1 = xs64_next(&rng);
            double bd = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            if (bd == 0.0) bd = 1.0;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            emit_d(out, "fuzz", bd, prec, RNDS[rnd_idx]);
        }
    }

    /* mined: 5 */
    emit_d(out, "mined", 1.0, 100, MPFR_RNDN);
    emit_d(out, "mined", 3.14, 100, MPFR_RNDN);
    emit_d(out, "mined", -1.5, 96, MPFR_RNDN);
    emit_d(out, "mined", 0.5, 100, MPFR_RNDN);
    emit_d(out, "mined", 7.0, 80, MPFR_RNDU);

    return 0;
}
