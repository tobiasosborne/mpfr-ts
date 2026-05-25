/*
 * golden_driver.c -- Golden master for MPFR's mpfr_custom_move.
 *
 * The TS port is the IDENTITY function (input MPFR -> same MPFR);
 * see spec.json divergence_from_c. The C function rebinds a mantissa
 * pointer, but the SHAPE of the MPFR is unchanged.
 *
 * To make the test surface meaningful, we test that the output MPFR
 * equals the input MPFR shape-wise for every well-formed input.
 *
 * Wire: {"inputs":{"x":<mpfr>},"output":<mpfr>}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_custom_move golden_driver requires GMP_NUMB_BITS == 64"
#endif

extern void mpfr_setmin(mpfr_ptr, mpfr_exp_t);
extern void mpfr_setmax(mpfr_ptr, mpfr_exp_t);

static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    const uint64_t t0 = now_ns();
    /* C body rebinds the mantissa pointer; we don't call it here
     * because we want the OUTPUT to be the same MPFR shape. */
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    jl_output_mpfr(out, x);  /* identity */
    jl_finish(out, elapsed);
}

static inline void emit_from_double(FILE *out, const char *tag, double d, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_d(x, d, MPFR_RNDN); emit_case(out, tag, x); mpfr_clear(x);
}
static inline void emit_inf(FILE *out, const char *tag, int sign, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_inf(x, sign); emit_case(out, tag, x); mpfr_clear(x);
}
static inline void emit_zero(FILE *out, const char *tag, int sign, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_zero(x, sign); emit_case(out, tag, x); mpfr_clear(x);
}
static inline void emit_nan(FILE *out, const char *tag, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_nan(x); emit_case(out, tag, x); mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    /* happy: 20 */
    emit_from_double(out, "happy", 3.14, 53);
    emit_from_double(out, "happy", -3.14, 53);
    emit_from_double(out, "happy", 1.0, 53);
    emit_from_double(out, "happy", -1.0, 53);
    emit_from_double(out, "happy", 0.5, 53);
    emit_from_double(out, "happy", 100.5, 53);
    emit_from_double(out, "happy", 1e10, 53);
    emit_from_double(out, "happy", 1e-10, 53);
    emit_from_double(out, "happy", 1.5, 24);
    emit_from_double(out, "happy", 1.5, 64);
    emit_from_double(out, "happy", 1.5, 128);
    emit_from_double(out, "happy", 1.5, 200);
    emit_inf(out, "happy", +1, 53);
    emit_inf(out, "happy", -1, 53);
    emit_zero(out, "happy", +1, 53);
    emit_zero(out, "happy", -1, 53);
    emit_nan(out, "happy", 53);
    emit_from_double(out, "happy", 2.71828, 100);
    emit_from_double(out, "happy", -2.71828, 100);
    emit_from_double(out, "happy", 1234567.89, 100);
    /* edge: 30 -- precs from 1, limb boundaries, all kinds. */
    emit_from_double(out, "edge", 1.5, 1);
    emit_from_double(out, "edge", -1.5, 1);
    emit_from_double(out, "edge", 1.5, 2);
    emit_from_double(out, "edge", 1.5, 63);
    emit_from_double(out, "edge", 1.5, 64);
    emit_from_double(out, "edge", 1.5, 65);
    emit_from_double(out, "edge", 1.5, 127);
    emit_from_double(out, "edge", 1.5, 128);
    emit_from_double(out, "edge", 1.5, 129);
    emit_inf(out, "edge", +1, 1);
    emit_inf(out, "edge", -1, 1);
    emit_inf(out, "edge", +1, 64);
    emit_inf(out, "edge", +1, 128);
    emit_inf(out, "edge", +1, 200);
    emit_zero(out, "edge", +1, 1);
    emit_zero(out, "edge", -1, 1);
    emit_zero(out, "edge", +1, 64);
    emit_zero(out, "edge", -1, 64);
    emit_zero(out, "edge", +1, 200);
    emit_nan(out, "edge", 1);
    emit_nan(out, "edge", 64);
    emit_nan(out, "edge", 128);
    emit_nan(out, "edge", 200);
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_setmin(x, mpfr_get_emin()); emit_case(out, "edge", x); mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_setmax(x, mpfr_get_emax()); emit_case(out, "edge", x); mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 1); mpfr_setmin(x, mpfr_get_emin()); emit_case(out, "edge", x); mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 200); mpfr_setmax(x, mpfr_get_emax()); emit_case(out, "edge", x); mpfr_clear(x);
    }
    emit_from_double(out, "edge", 1e300, 53);
    emit_from_double(out, "edge", -1e300, 53);
    emit_from_double(out, "edge", 7.0, 3);
    /* adversarial: 12 */
    emit_from_double(out, "adversarial", 1.0/3.0, 53);
    emit_from_double(out, "adversarial", 1.0/3.0, 200);
    emit_from_double(out, "adversarial", -1.0/3.0, 53);
    emit_from_double(out, "adversarial", 1.0, 1);
    emit_from_double(out, "adversarial", -1.0, 1);
    emit_inf(out, "adversarial", +1, 4096);
    emit_nan(out, "adversarial", 4096);
    emit_zero(out, "adversarial", -1, 4096);
    emit_from_double(out, "adversarial", 1e-100, 200);
    emit_from_double(out, "adversarial", 1e100, 200);
    emit_from_double(out, "adversarial", 0.1, 53);
    emit_from_double(out, "adversarial", -0.1, 53);
    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xFFFFAAAACCCC0001ULL);
        const mpfr_prec_t precs[5] = {2, 24, 53, 64, 200};
        for (int rep = 0; rep < 50; ++rep) {
            const mpfr_prec_t p = precs[xs64_below(&rng, 5)];
            const uint64_t kc = xs64_below(&rng, 10);
            mpfr_t x;
            mpfr_init2(x, p);
            switch (kc) {
                case 0: mpfr_set_inf(x, +1); break;
                case 1: mpfr_set_inf(x, -1); break;
                case 2: mpfr_set_zero(x, +1); break;
                case 3: mpfr_set_zero(x, -1); break;
                case 4: mpfr_set_nan(x); break;
                default: {
                    const int neg = (xs64_below(&rng, 2) == 0) ? +1 : -1;
                    const double v = (double)(xs64_below(&rng, 100000) + 1) / 7.0 * neg;
                    mpfr_set_d(x, v, MPFR_RNDN);
                    break;
                }
            }
            emit_case(out, "fuzz", x);
            mpfr_clear(x);
        }
    }
    /* mined: 5 */
    emit_from_double(out, "mined", 1.0, 53);
    emit_from_double(out, "mined", 17.0, 53);
    emit_inf(out, "mined", +1, 53);
    emit_nan(out, "mined", 53);
    emit_zero(out, "mined", -1, 53);
    return 0;
}
