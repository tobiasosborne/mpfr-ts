/*
 * golden_driver.c -- Golden master for MPFR's mpfr_custom_get_exp.
 *
 * C: mpfr_exp_t mpfr_custom_get_exp(mpfr_srcptr x) { return MPFR_EXP(x); }
 * Ref: mpfr/src/stack_interface.c L46-L51.
 *
 * INPUT CARVE-OUT: only NORMAL (regular) MPFR values are tested. The C
 * function returns sentinel exponents __MPFR_EXP_NAN/INF/ZERO for
 * singular inputs; the TS schema has no such sentinels. The golden
 * sidesteps the divergence by restricting the input domain.
 *
 * Wire: {"inputs":{"x":<mpfr>},"output":"<dec>"}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <math.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_custom_get_exp golden_driver requires GMP_NUMB_BITS == 64"
#endif

extern void mpfr_setmin(mpfr_ptr, mpfr_exp_t);
extern void mpfr_setmax(mpfr_ptr, mpfr_exp_t);

static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    assert(mpfr_regular_p(x));  /* singular inputs excluded; see header. */
    const uint64_t t0 = now_ns();
    const mpfr_exp_t e = mpfr_custom_get_exp(x);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    jl_output_scalar_i64(out, (int64_t)e);
    jl_finish(out, elapsed);
}

static inline void emit_from_double(FILE *out, const char *tag, double d, mpfr_prec_t prec) {
    mpfr_t x;
    mpfr_init2(x, prec);
    mpfr_set_d(x, d, MPFR_RNDN);
    if (mpfr_regular_p(x)) emit_case(out, tag, x);
    mpfr_clear(x);
}

static inline void emit_pow2(FILE *out, const char *tag, mpfr_prec_t prec, mpfr_exp_t e) {
    mpfr_t x;
    mpfr_init2(x, prec);
    mpfr_set_ui(x, 1, MPFR_RNDN);
    mpfr_set_exp(x, e);
    emit_case(out, tag, x);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    /* happy: 20 -- common doubles across normal precs. */
    emit_from_double(out, "happy", 3.14, 53);
    emit_from_double(out, "happy", 2.71828, 53);
    emit_from_double(out, "happy", 1.0, 53);
    emit_from_double(out, "happy", 100.0, 53);
    emit_from_double(out, "happy", 0.5, 53);
    emit_from_double(out, "happy", 0.25, 53);
    emit_from_double(out, "happy", -3.14, 53);
    emit_from_double(out, "happy", 1e10, 53);
    emit_from_double(out, "happy", 1e-10, 53);
    emit_from_double(out, "happy", 1.5, 24);
    emit_from_double(out, "happy", 1.5, 64);
    emit_from_double(out, "happy", 1.5, 128);
    emit_from_double(out, "happy", 1.5, 256);
    emit_pow2(out, "happy", 53, 0);
    emit_pow2(out, "happy", 53, 1);
    emit_pow2(out, "happy", 53, 10);
    emit_pow2(out, "happy", 53, 100);
    emit_pow2(out, "happy", 53, 1000);
    emit_pow2(out, "happy", 53, -100);
    emit_pow2(out, "happy", 53, -1000);
    /* edge: 30 -- prec boundaries, exp extremes, prec=1. */
    emit_from_double(out, "edge", 1.5, 1);
    emit_from_double(out, "edge", 1.5, 2);
    emit_from_double(out, "edge", 1.5, 63);
    emit_from_double(out, "edge", 1.5, 64);
    emit_from_double(out, "edge", 1.5, 65);
    emit_from_double(out, "edge", 1.5, 127);
    emit_from_double(out, "edge", 1.5, 128);
    emit_from_double(out, "edge", 1.5, 129);
    emit_pow2(out, "edge", 1, 0);
    emit_pow2(out, "edge", 1, 1);
    emit_pow2(out, "edge", 1, -1);
    emit_pow2(out, "edge", 1, 100);
    emit_pow2(out, "edge", 1, -100);
    emit_pow2(out, "edge", 53, 1024);
    emit_pow2(out, "edge", 53, -1024);
    emit_pow2(out, "edge", 64, mpfr_get_emax() - 1);
    emit_pow2(out, "edge", 64, mpfr_get_emin() + 1);
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_setmin(x, mpfr_get_emin()); emit_case(out, "edge", x); mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_setmax(x, mpfr_get_emax()); emit_case(out, "edge", x); mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 200); mpfr_setmin(x, mpfr_get_emin()); emit_case(out, "edge", x); mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 200); mpfr_setmax(x, mpfr_get_emax()); emit_case(out, "edge", x); mpfr_clear(x);
    }
    emit_from_double(out, "edge", 3.14, 113);
    emit_from_double(out, "edge", 3.14, 200);
    emit_from_double(out, "edge", 3.14, 500);
    emit_from_double(out, "edge", 1234567.89, 53);
    emit_from_double(out, "edge", -1234567.89, 53);
    emit_pow2(out, "edge", 53, 5);
    emit_pow2(out, "edge", 53, -5);
    emit_from_double(out, "edge", 0.99999, 53);
    emit_from_double(out, "edge", 1.00001, 53);
    /* adversarial: 12 -- exponents near limit, big precs. */
    emit_pow2(out, "adversarial", 53, mpfr_get_emax());
    emit_pow2(out, "adversarial", 53, mpfr_get_emin());
    emit_pow2(out, "adversarial", 24, 0);
    emit_pow2(out, "adversarial", 24, mpfr_get_emax());
    emit_pow2(out, "adversarial", 24, mpfr_get_emin());
    emit_pow2(out, "adversarial", 1, mpfr_get_emax());
    emit_pow2(out, "adversarial", 1, mpfr_get_emin());
    emit_pow2(out, "adversarial", 1000, 1);
    emit_pow2(out, "adversarial", 1000, mpfr_get_emax());
    emit_pow2(out, "adversarial", 1000, mpfr_get_emin());
    emit_pow2(out, "adversarial", 53, 2);
    emit_pow2(out, "adversarial", 53, -2);
    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xBABE10ADC0DEFEEDULL);
        const mpfr_prec_t precs[7] = {1, 2, 24, 53, 64, 128, 256};
        int emitted = 0;
        while (emitted < 50) {
            const mpfr_prec_t p = precs[xs64_below(&rng, 7)];
            const int64_t exp_choice = (int64_t)(xs64_below(&rng, 2001)) - 1000;  /* [-1000, 1000] */
            mpfr_t x;
            mpfr_init2(x, p);
            mpfr_set_ui(x, 1, MPFR_RNDN);
            if (mpfr_set_exp(x, (mpfr_exp_t)exp_choice) != 0 || !mpfr_regular_p(x)) {
                mpfr_clear(x);
                continue;
            }
            emit_case(out, "fuzz", x);
            mpfr_clear(x);
            emitted++;
        }
    }
    /* mined: 5 -- from tcustom.c usage patterns. */
    emit_from_double(out, "mined", 1.0, 53);
    emit_from_double(out, "mined", 2.0, 53);
    emit_from_double(out, "mined", 0.5, 53);
    emit_from_double(out, "mined", 17.0, 53);
    emit_pow2(out, "mined", 100, 50);
    return 0;
}
