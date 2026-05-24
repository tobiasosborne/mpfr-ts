/*
 * golden_driver.c — Golden master for MPFR's mpfr_mul_2exp.
 *
 * mpfr_mul_2exp is an obsolete alias for mpfr_mul_2ui (mpfr/src/mul_2exp.c
 * L28-L32). The C reference body is a one-line delegate. The golden
 * therefore tests the same input distribution as mul_2ui.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR>,"n":"<decimal>","prec":"<decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution: happy 22, edge 30, adversarial 10, fuzz 55, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_mul_2exp golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_CAP ((uint64_t)4096)

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, unsigned long n,
                             uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t y;
    mpfr_init2(y, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_mul_2exp(y, x, n, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_u64(out, 0, "n", (uint64_t)n);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, y, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(y);
}

static inline void emit_d(FILE *out, const char *tag,
                          double xd, uint64_t xprec, unsigned long n,
                          uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec); mpfr_set_d(x, xd, MPFR_RNDN);
    emit_case(out, tag, x, n, prec, rnd);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    emit_d(out, "happy", 1.0, 53, 0, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 53, 1, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 53, 2, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 53, 10, 53, MPFR_RNDN);
    emit_d(out, "happy", 3.14, 53, 5, 53, MPFR_RNDN);
    emit_d(out, "happy", -3.14, 53, 5, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.5, 53, 100, 53, MPFR_RNDN);
    emit_d(out, "happy", -1.5, 53, 100, 53, MPFR_RNDN);
    emit_d(out, "happy", 0.5, 53, 1, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0/3.0, 53, 0, 24, MPFR_RNDN);
    emit_d(out, "happy", 1.0/3.0, 53, 5, 24, MPFR_RNDN);
    emit_d(out, "happy", 2.71, 53, 7, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 100, 50, 100, MPFR_RNDN);
    emit_d(out, "happy", 1.5, 32, 16, 32, MPFR_RNDA);
    emit_d(out, "happy", -1.5, 32, 16, 32, MPFR_RNDA);
    emit_d(out, "happy", 7.0, 24, 0, 24, MPFR_RNDN);
    emit_d(out, "happy", 100.0, 53, 3, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0/7.0, 200, 100, 200, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 53, 1000, 53, MPFR_RNDN);
    emit_d(out, "happy", -1.0, 53, 1000, 53, MPFR_RNDD);
    emit_d(out, "happy", 0.1, 53, 30, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0e10, 53, 5, 53, MPFR_RNDN);

    /* edge: 30 */
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x); emit_case(out, "edge", x, 5, 53, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, +1); emit_case(out, "edge", x, 5, 53, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, -1); emit_case(out, "edge", x, 5, 53, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1); emit_case(out, "edge", x, 5, 53, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1); emit_case(out, "edge", x, 5, 53, MPFR_RNDN); mpfr_clear(x); }
    for (int i = 0; i < 5; i++) emit_d(out, "edge", 1.5, 53, 0, 53, RNDS[i]);
    for (int i = 0; i < 5; i++) emit_d(out, "edge", 1.5, 53, 5, 53, RNDS[i]);
    for (int i = 0; i < 5; i++) emit_d(out, "edge", 1.0/3.0, 53, 5, 3, RNDS[i]);
    emit_d(out, "edge", 1.5, 1, 0, 1, MPFR_RNDN);
    emit_d(out, "edge", 1.5, 1, 5, 1, MPFR_RNDN);
    emit_d(out, "edge", 1.5, TS_PREC_CAP, 1, TS_PREC_CAP, MPFR_RNDN);
    emit_d(out, "edge", 1.5, 53, 1, 1, MPFR_RNDN);
    emit_d(out, "edge", 1.5, 1, 1, 53, MPFR_RNDN);
    /* n=0 across all rnd. */
    for (int i = 0; i < 5; i++) emit_d(out, "edge", 3.14, 53, 0, 53, RNDS[i]);

    /* adversarial: 10 */
    emit_d(out, "adversarial", 0.9999999999999999, 53, 1, 24, MPFR_RNDN);
    emit_d(out, "adversarial", 0.9999999999999999, 53, 1, 24, MPFR_RNDU);
    emit_d(out, "adversarial", -0.9999999999999999, 53, 1, 24, MPFR_RNDD);
    emit_d(out, "adversarial", 1.0/3.0, 53, 10, 4, MPFR_RNDN);
    emit_d(out, "adversarial", 1.0/3.0, 53, 10, 4, MPFR_RNDU);
    emit_d(out, "adversarial", 1.0/3.0, 53, 10, 4, MPFR_RNDD);
    emit_d(out, "adversarial", -1.0/3.0, 53, 10, 4, MPFR_RNDD);
    emit_d(out, "adversarial", 1.0, 53, 500, 1, MPFR_RNDN);
    emit_d(out, "adversarial", 1.5, 2, 0, 2, MPFR_RNDN);
    emit_d(out, "adversarial", 1.5, 2, 0, 2, MPFR_RNDZ);

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x2E2E2E2E2E2E2E2EULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 256);
            const uint64_t xprec = 1 + xs64_below(&rng, 256);
            const uint64_t r1 = xs64_next(&rng);
            double xd = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            if (xd == 0.0) xd = 1.0;
            const unsigned long n = (unsigned long)xs64_below(&rng, 1000);
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            emit_d(out, "fuzz", xd, xprec, n, prec, RNDS[rnd_idx]);
        }
    }

    /* mined: 5 */
    emit_d(out, "mined", 1.0, 53, 1, 53, MPFR_RNDN);
    emit_d(out, "mined", 3.14, 53, 0, 53, MPFR_RNDN);
    emit_d(out, "mined", 1.5, 24, 10, 24, MPFR_RNDN);
    emit_d(out, "mined", -2.5, 53, 5, 53, MPFR_RNDN);
    emit_d(out, "mined", 1.0/7.0, 53, 3, 24, MPFR_RNDU);

    return 0;
}
