/*
 * golden_driver.c — Golden master for MPFR's mpfr_mul_1.
 *
 * Static helper in mpfr/src/mul.c L270-L364; exercised via public mpfr_mul
 * with prec(a) == prec(b) == prec(c) < 64 (dispatcher routes here per
 * mpfr/src/mul.c L807).
 *
 * Tag distribution: happy 22, edge 30, adversarial 12, fuzz 60, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_mul_1 golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr b, mpfr_srcptr c,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= 1 && prec < 64);
    assert(mpfr_get_prec(b) == (mpfr_prec_t)prec);
    assert(mpfr_get_prec(c) == (mpfr_prec_t)prec);
    assert(mpfr_regular_p(b) && mpfr_regular_p(c));
    mpfr_t a; mpfr_init2(a, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_mul(a, b, c, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "b", b);
    jl_kv_mpfr(out, 0, "c", c);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, a, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(a);
}

static inline void emit_dd(FILE *out, const char *tag,
                           double bd, double cd, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t b, c;
    mpfr_init2(b, (mpfr_prec_t)prec); mpfr_set_d(b, bd, MPFR_RNDN);
    mpfr_init2(c, (mpfr_prec_t)prec); mpfr_set_d(c, cd, MPFR_RNDN);
    if (!mpfr_regular_p(b) || !mpfr_regular_p(c)) { mpfr_clear(b); mpfr_clear(c); return; }
    emit_case(out, tag, b, c, prec, rnd);
    mpfr_clear(b); mpfr_clear(c);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    emit_dd(out, "happy", 2.0, 3.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.5, 2.5, 53, MPFR_RNDN);
    emit_dd(out, "happy", 3.14, 2.71, 53, MPFR_RNDN);
    emit_dd(out, "happy", -2.0, 3.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", -2.0, -3.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 1.0, 24, MPFR_RNDN);
    emit_dd(out, "happy", 0.5, 0.5, 24, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 0.1, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1e5, 1e5, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0/3.0, 3.0, 24, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 7.0, 8, MPFR_RNDN);
    emit_dd(out, "happy", -1.0, -1.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 7.0, 6.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 0.1, 10.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1e-5, 1e-5, 53, MPFR_RNDN);
    emit_dd(out, "happy", 100.5, 0.25, 32, MPFR_RNDA);
    emit_dd(out, "happy", -100.5, 0.25, 32, MPFR_RNDA);
    emit_dd(out, "happy", 1.5, 1.5, 53, MPFR_RNDU);
    emit_dd(out, "happy", 1.5, 1.5, 53, MPFR_RNDD);
    emit_dd(out, "happy", 2.0, 2.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, -1.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 0.1, 0.2, 53, MPFR_RNDN);

    /* edge: 30 — prec extremes, all rnd. */
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.0, 1.0, 1, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.0, 1.0, 63, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.5, 1.5, 53, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 3.14, 2.71, 8, RNDS[i]);
    emit_dd(out, "edge", 1.5, 0.25, 2, MPFR_RNDN);
    emit_dd(out, "edge", 1.5, 0.25, 2, MPFR_RNDU);
    emit_dd(out, "edge", 1.5, 0.25, 2, MPFR_RNDD);
    emit_dd(out, "edge", 1e10, 1e-10, 53, MPFR_RNDN);
    emit_dd(out, "edge", 1.0/3.0, 3.0, 60, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 1e100, 53, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 1e-100, 53, MPFR_RNDN);
    emit_dd(out, "edge", -1.0/7.0, 7.0, 16, MPFR_RNDD);
    emit_dd(out, "edge", 1.0, 2.0, 63, MPFR_RNDN);
    emit_dd(out, "edge", -1.0, -2.0, 63, MPFR_RNDN);
    emit_dd(out, "edge", 1.5, 2.5, 63, MPFR_RNDA);

    /* adversarial: 12 */
    emit_dd(out, "adversarial", 0.99999999, 1.0, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.0, 1.0 + 1e-15, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.0/3.0, 1.0/7.0, 24, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.0/3.0, 1.0/7.0, 24, MPFR_RNDU);
    emit_dd(out, "adversarial", 1.0/3.0, 1.0/7.0, 24, MPFR_RNDD);
    emit_dd(out, "adversarial", -1.0/3.0, 1.0/7.0, 24, MPFR_RNDD);
    emit_dd(out, "adversarial", 0.1, 0.7, 8, MPFR_RNDN);
    emit_dd(out, "adversarial", 0.5, 0.5, 1, MPFR_RNDN);
    emit_dd(out, "adversarial", 0.5, 0.5, 1, MPFR_RNDU);
    emit_dd(out, "adversarial", 1.5, 3.0, 4, MPFR_RNDN);
    emit_dd(out, "adversarial", -1.5, 3.0, 4, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 1e308, 53, MPFR_RNDN);

    /* fuzz: 60 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x60D5BABE5A55B0B5ULL);
        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 63);
            const uint64_t r1 = xs64_next(&rng);
            const uint64_t r2 = xs64_next(&rng);
            double bd = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            double cd = ((double)(r2 % 200000ULL) - 100000.0) / 100.0;
            if (bd == 0.0) bd = 1.0;
            if (cd == 0.0) cd = 1.0;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            emit_dd(out, "fuzz", bd, cd, prec, RNDS[rnd_idx]);
        }
    }

    /* mined: 5 */
    emit_dd(out, "mined", 2.0, 3.0, 53, MPFR_RNDN);
    emit_dd(out, "mined", 1.0/3.0, 3.0, 24, MPFR_RNDU);
    emit_dd(out, "mined", 1.5, 1.5, 53, MPFR_RNDN);
    emit_dd(out, "mined", -1.0, 2.0, 53, MPFR_RNDN);
    emit_dd(out, "mined", 1.5e5, 2.0, 53, MPFR_RNDU);

    return 0;
}
