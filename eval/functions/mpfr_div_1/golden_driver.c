/*
 * golden_driver.c — Golden master for MPFR's mpfr_div_1.
 *
 * Static helper in mpfr/src/div.c L108-L251; exercised via public mpfr_div
 * with prec(q) == prec(u) == prec(v) < 64 (dispatcher per mpfr/src/div.c
 * L839).
 *
 * Tag distribution: happy 22, edge 30, adversarial 12, fuzz 60, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_div_1 golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr u, mpfr_srcptr v,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= 1 && prec < 64);
    assert(mpfr_get_prec(u) == (mpfr_prec_t)prec);
    assert(mpfr_get_prec(v) == (mpfr_prec_t)prec);
    assert(mpfr_regular_p(u) && mpfr_regular_p(v));
    mpfr_t q; mpfr_init2(q, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_div(q, u, v, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "u", u);
    jl_kv_mpfr(out, 0, "v", v);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, q, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(q);
}

static inline void emit_dd(FILE *out, const char *tag,
                           double ud, double vd, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t u, v;
    mpfr_init2(u, (mpfr_prec_t)prec); mpfr_set_d(u, ud, MPFR_RNDN);
    mpfr_init2(v, (mpfr_prec_t)prec); mpfr_set_d(v, vd, MPFR_RNDN);
    if (!mpfr_regular_p(u) || !mpfr_regular_p(v)) { mpfr_clear(u); mpfr_clear(v); return; }
    emit_case(out, tag, u, v, prec, rnd);
    mpfr_clear(u); mpfr_clear(v);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    emit_dd(out, "happy", 6.0, 2.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 4.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 3.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", -6.0, 2.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", -6.0, -2.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 1.0, 24, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 2.0, 24, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 7.0, 24, MPFR_RNDU);
    emit_dd(out, "happy", 22.0, 7.0, 24, MPFR_RNDN);
    emit_dd(out, "happy", 2.0, 2.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", -1.0, -1.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 7.0, 11.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 0.1, 10.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1e5, 1e3, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, -1.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", -1.0, 2.0, 53, MPFR_RNDD);
    emit_dd(out, "happy", 100.5, 0.25, 32, MPFR_RNDA);
    emit_dd(out, "happy", -100.5, 0.25, 32, MPFR_RNDA);
    emit_dd(out, "happy", 1.5, 1.5, 53, MPFR_RNDU);
    emit_dd(out, "happy", 1.5, 1.5, 53, MPFR_RNDD);
    emit_dd(out, "happy", 1.0, 1.0/3.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 1.0/7.0, 24, MPFR_RNDN);

    /* edge: 30 — prec extremes, all rnd. */
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.0, 1.0, 1, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.0, 1.0, 63, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.0, 3.0, 53, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 22.0, 7.0, 24, RNDS[i]);
    emit_dd(out, "edge", 0.99999999, 1.0, 53, MPFR_RNDN);
    emit_dd(out, "edge", 0.99999999, 1.0, 53, MPFR_RNDU);
    emit_dd(out, "edge", 1.0, 0.99999999, 53, MPFR_RNDN);
    emit_dd(out, "edge", 1e10, 1e-10, 53, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 1e100, 53, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 1e-100, 53, MPFR_RNDN);
    emit_dd(out, "edge", 2.0, 7.0, 60, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 7.0, 60, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 7.0, 8, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 3.0, 8, MPFR_RNDU);
    emit_dd(out, "edge", 1.0, 11.0, 16, MPFR_RNDN);

    /* adversarial: 12 */
    emit_dd(out, "adversarial", 1.0, 3.0, 24, MPFR_RNDU);
    emit_dd(out, "adversarial", 1.0, 3.0, 24, MPFR_RNDD);
    emit_dd(out, "adversarial", -1.0, 3.0, 24, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 7.0, 24, MPFR_RNDU);
    emit_dd(out, "adversarial", 1.0, 7.0, 24, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 7.0, 24, MPFR_RNDA);
    emit_dd(out, "adversarial", 1.0, 11.0, 8, MPFR_RNDN);
    emit_dd(out, "adversarial", 0.999, 1.001, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.001, 0.999, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.0, 9.0, 4, MPFR_RNDN);
    emit_dd(out, "adversarial", -1.0, 9.0, 4, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 13.0, 16, MPFR_RNDU);

    /* fuzz: 60 — bounded so v != 0. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xD11D11D11D11D11DULL);
        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 63);
            const uint64_t r1 = xs64_next(&rng);
            double ud = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            const uint64_t r2 = xs64_next(&rng);
            double vd = ((double)((r2 % 199998ULL) + 1) - 99999.0) / 100.0;
            if (ud == 0.0) ud = 1.0;
            if (vd == 0.0) vd = 1.0;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            emit_dd(out, "fuzz", ud, vd, prec, RNDS[rnd_idx]);
        }
    }

    /* mined: 5 */
    emit_dd(out, "mined", 6.0, 2.0, 53, MPFR_RNDN);
    emit_dd(out, "mined", 1.0, 3.0, 53, MPFR_RNDU);
    emit_dd(out, "mined", 1.5, 1.5, 53, MPFR_RNDN);
    emit_dd(out, "mined", -1.0, 2.0, 53, MPFR_RNDN);
    emit_dd(out, "mined", 22.0, 7.0, 24, MPFR_RNDN);

    return 0;
}
