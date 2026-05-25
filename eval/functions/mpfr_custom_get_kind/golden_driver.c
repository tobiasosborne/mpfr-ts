/*
 * golden_driver.c -- Golden master for MPFR's mpfr_custom_get_kind.
 *
 * C: int mpfr_custom_get_kind(mpfr_srcptr x);
 *   returns NAN_KIND (0), INF_KIND (1)*sign, ZERO_KIND (2)*sign, or
 *   REGULAR_KIND (3)*sign.
 * Ref: mpfr/src/stack_interface.c L91-L103, mpfr/src/mpfr.h L287-L292.
 *
 * Wire: {"inputs":{"x":<mpfr>},"output":<int>}. output via jl_output_scalar_int.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_custom_get_kind golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    const uint64_t t0 = now_ns();
    const int k = mpfr_custom_get_kind(x);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    jl_output_scalar_int(out, k);
    jl_finish(out, elapsed);
}

static inline void emit_from_double(FILE *out, const char *tag, double d, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_d(x, d, MPFR_RNDN);
    emit_case(out, tag, x); mpfr_clear(x);
}
static inline void emit_pos_inf(FILE *out, const char *tag, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_inf(x, +1); emit_case(out, tag, x); mpfr_clear(x);
}
static inline void emit_neg_inf(FILE *out, const char *tag, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_inf(x, -1); emit_case(out, tag, x); mpfr_clear(x);
}
static inline void emit_pos_zero(FILE *out, const char *tag, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_zero(x, +1); emit_case(out, tag, x); mpfr_clear(x);
}
static inline void emit_neg_zero(FILE *out, const char *tag, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_zero(x, -1); emit_case(out, tag, x); mpfr_clear(x);
}
static inline void emit_nan(FILE *out, const char *tag, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_nan(x); emit_case(out, tag, x); mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    /* happy: 20 -- mix of all kinds and signs. */
    emit_from_double(out, "happy", 3.14, 53);
    emit_from_double(out, "happy", -3.14, 53);
    emit_from_double(out, "happy", 1.0, 53);
    emit_from_double(out, "happy", -1.0, 53);
    emit_pos_inf(out, "happy", 53);
    emit_neg_inf(out, "happy", 53);
    emit_pos_zero(out, "happy", 53);
    emit_neg_zero(out, "happy", 53);
    emit_nan(out, "happy", 53);
    emit_from_double(out, "happy", 2.718, 53);
    emit_from_double(out, "happy", -2.718, 53);
    emit_from_double(out, "happy", 100.5, 53);
    emit_from_double(out, "happy", -100.5, 53);
    emit_from_double(out, "happy", 1e10, 53);
    emit_from_double(out, "happy", -1e10, 53);
    emit_from_double(out, "happy", 1e-10, 53);
    emit_from_double(out, "happy", -1e-10, 53);
    emit_from_double(out, "happy", 0.5, 53);
    emit_from_double(out, "happy", -0.5, 53);
    emit_from_double(out, "happy", 1234567.89, 53);
    /* edge: 30 -- precs from 1 to 200, all kinds. */
    emit_from_double(out, "edge", 1.5, 1);
    emit_from_double(out, "edge", -1.5, 1);
    emit_pos_inf(out, "edge", 1);
    emit_neg_inf(out, "edge", 1);
    emit_pos_zero(out, "edge", 1);
    emit_neg_zero(out, "edge", 1);
    emit_nan(out, "edge", 1);
    emit_from_double(out, "edge", 1.5, 24);
    emit_from_double(out, "edge", -1.5, 24);
    emit_from_double(out, "edge", 1.5, 64);
    emit_from_double(out, "edge", 1.5, 65);
    emit_from_double(out, "edge", 1.5, 128);
    emit_from_double(out, "edge", 1.5, 256);
    emit_pos_inf(out, "edge", 24);
    emit_neg_inf(out, "edge", 24);
    emit_pos_zero(out, "edge", 24);
    emit_neg_zero(out, "edge", 24);
    emit_pos_inf(out, "edge", 64);
    emit_neg_inf(out, "edge", 64);
    emit_pos_zero(out, "edge", 64);
    emit_neg_zero(out, "edge", 64);
    emit_pos_inf(out, "edge", 200);
    emit_neg_inf(out, "edge", 200);
    emit_pos_zero(out, "edge", 200);
    emit_neg_zero(out, "edge", 200);
    emit_nan(out, "edge", 24);
    emit_nan(out, "edge", 64);
    emit_nan(out, "edge", 128);
    emit_nan(out, "edge", 200);
    emit_from_double(out, "edge", -1.5, 200);
    /* adversarial: 12 -- denormal-ish, alternating signs. */
    emit_from_double(out, "adversarial", 1e-300, 53);
    emit_from_double(out, "adversarial", -1e-300, 53);
    emit_from_double(out, "adversarial", 1e300, 53);
    emit_from_double(out, "adversarial", -1e300, 53);
    emit_from_double(out, "adversarial", 1.0, 1);
    emit_from_double(out, "adversarial", -1.0, 1);
    emit_from_double(out, "adversarial", 0.999, 64);
    emit_from_double(out, "adversarial", -0.999, 64);
    emit_pos_inf(out, "adversarial", 128);
    emit_neg_inf(out, "adversarial", 128);
    emit_pos_zero(out, "adversarial", 128);
    emit_neg_zero(out, "adversarial", 128);
    /* fuzz: 50 -- random doubles + occasional special kinds. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xDEADBEEFCAFEBABEULL);
        const mpfr_prec_t precs[5] = {2, 24, 53, 64, 200};
        for (int rep = 0; rep < 50; ++rep) {
            const mpfr_prec_t p = precs[xs64_below(&rng, 5)];
            const uint64_t kind_choice = xs64_below(&rng, 10);
            mpfr_t x;
            mpfr_init2(x, p);
            switch (kind_choice) {
                case 0: mpfr_set_inf(x, +1); break;
                case 1: mpfr_set_inf(x, -1); break;
                case 2: mpfr_set_zero(x, +1); break;
                case 3: mpfr_set_zero(x, -1); break;
                case 4: mpfr_set_nan(x); break;
                default: {
                    const int neg = (xs64_below(&rng, 2) == 0) ? +1 : -1;
                    const double v = (double)(xs64_below(&rng, 1000) + 1) / 7.0 * neg;
                    mpfr_set_d(x, v, MPFR_RNDN);
                    break;
                }
            }
            emit_case(out, "fuzz", x);
            mpfr_clear(x);
        }
    }
    /* mined: 5 -- from tcustom.c kind-detection patterns. */
    emit_from_double(out, "mined", 1.0, 53);
    emit_pos_inf(out, "mined", 53);
    emit_neg_zero(out, "mined", 53);
    emit_nan(out, "mined", 53);
    emit_from_double(out, "mined", -1.0, 100);
    return 0;
}
