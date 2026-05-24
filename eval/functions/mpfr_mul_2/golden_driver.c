/*
 * golden_driver.c — Golden master for MPFR's mpfr_mul_2.
 *
 * Static helper in mpfr/src/mul.c L469-L588; exercised via public mpfr_mul
 * with 64 < prec(a) == prec(b) == prec(c) < 128 (dispatcher routes here).
 *
 * Tag distribution: happy 22, edge 30, adversarial 12, fuzz 55, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_mul_2 golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr b, mpfr_srcptr c,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec > 64 && prec < 128);
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
    emit_dd(out, "happy", 2.0, 3.0, 100, MPFR_RNDN);
    emit_dd(out, "happy", 1.5, 2.5, 100, MPFR_RNDN);
    emit_dd(out, "happy", 3.14, 2.71, 100, MPFR_RNDN);
    emit_dd(out, "happy", -2.0, 3.0, 100, MPFR_RNDN);
    emit_dd(out, "happy", -2.0, -3.0, 100, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 1.0, 96, MPFR_RNDN);
    emit_dd(out, "happy", 0.5, 0.5, 96, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 0.1, 100, MPFR_RNDN);
    emit_dd(out, "happy", 1e5, 1e5, 100, MPFR_RNDN);
    emit_dd(out, "happy", 1.0/3.0, 3.0, 100, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 7.0, 96, MPFR_RNDN);
    emit_dd(out, "happy", -1.0, -1.0, 100, MPFR_RNDN);
    emit_dd(out, "happy", 7.0, 6.0, 100, MPFR_RNDN);
    emit_dd(out, "happy", 0.1, 10.0, 100, MPFR_RNDN);
    emit_dd(out, "happy", 1e-5, 1e-5, 100, MPFR_RNDN);
    emit_dd(out, "happy", 100.5, 0.25, 80, MPFR_RNDA);
    emit_dd(out, "happy", -100.5, 0.25, 80, MPFR_RNDA);
    emit_dd(out, "happy", 1.5, 1.5, 100, MPFR_RNDU);
    emit_dd(out, "happy", 1.5, 1.5, 100, MPFR_RNDD);
    emit_dd(out, "happy", 2.0, 2.0, 100, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, -1.0, 100, MPFR_RNDN);
    emit_dd(out, "happy", 0.1, 0.2, 100, MPFR_RNDN);

    /* edge: 30 — prec extremes (65, 127), all rnd. */
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.0, 1.0, 65, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.0, 1.0, 127, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.5, 1.5, 100, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.0/3.0, 1.0/7.0, 100, RNDS[i]);
    emit_dd(out, "edge", 0.99999999, 1.0, 100, MPFR_RNDN);
    emit_dd(out, "edge", 0.99999999, 1.0, 100, MPFR_RNDU);
    emit_dd(out, "edge", 1e10, 1e-10, 100, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 1e100, 100, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 1e-100, 100, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 2.0, 65, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 2.0, 127, MPFR_RNDN);
    emit_dd(out, "edge", 1.5, 3.0, 65, MPFR_RNDA);
    emit_dd(out, "edge", 1.5, 3.0, 127, MPFR_RNDD);
    emit_dd(out, "edge", -1.0/7.0, 7.0, 96, MPFR_RNDD);
    emit_dd(out, "edge", 1.0, 1e15, 80, MPFR_RNDN);

    /* adversarial: 12 */
    emit_dd(out, "adversarial", 0.99999999, 1.0, 100, MPFR_RNDU);
    emit_dd(out, "adversarial", 1.0, 1.0 + 1e-15, 100, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.0/3.0, 1.0/7.0, 65, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.0/3.0, 1.0/7.0, 65, MPFR_RNDU);
    emit_dd(out, "adversarial", 1.0/3.0, 1.0/7.0, 65, MPFR_RNDD);
    emit_dd(out, "adversarial", -1.0/3.0, 1.0/7.0, 65, MPFR_RNDD);
    emit_dd(out, "adversarial", 0.1, 0.7, 96, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.5, 3.0, 100, MPFR_RNDN);
    emit_dd(out, "adversarial", -1.5, 3.0, 100, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 1e308, 100, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.0 + 1e-30, 1.0 + 1e-30, 100, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.0/11.0, 11.0, 100, MPFR_RNDN);

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x6262626262626262ULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t prec = 65 + xs64_below(&rng, 63);
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
    emit_dd(out, "mined", 2.0, 3.0, 100, MPFR_RNDN);
    emit_dd(out, "mined", 1.0/3.0, 3.0, 100, MPFR_RNDU);
    emit_dd(out, "mined", 1.5, 1.5, 100, MPFR_RNDN);
    emit_dd(out, "mined", -1.0, 2.0, 100, MPFR_RNDN);
    emit_dd(out, "mined", 1.5e5, 2.0, 100, MPFR_RNDU);

    return 0;
}
