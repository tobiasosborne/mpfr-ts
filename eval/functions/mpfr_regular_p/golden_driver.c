/*
 * golden_driver.c — Golden master for MPFR's mpfr_regular_p.
 *
 * Returns true iff x is a regular (finite non-zero) value. The C body is
 * a one-line MPFR_IS_SINGULAR check. Output is a bare JSON boolean.
 *
 * Tag distribution: happy 25, edge 35, adversarial 12, fuzz 55, mined 5.
 *
 * Ref: mpfr/src/isregular.c — the C reference.
 * Ref: src/ops/regular_p.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_regular_p golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    const uint64_t t0 = now_ns();
    const int raw = mpfr_regular_p(x);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    jl_output_scalar_bool(out, raw);
    jl_finish(out, elapsed);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_pos_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, 1); }
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, -1); }
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, 1); }
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, -1); }
static inline void init_nan(mpfr_ptr x, uint64_t prec)      { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_nan(x); }

#define EMIT(tag, init_expr) do { mpfr_t _x; init_expr; emit_case(out, tag, _x); mpfr_clear(_x); } while (0)

int main(void) {
    FILE *out = stdout;

    /* happy: 25 — mostly regulars (true) + a couple singulars. */
    EMIT("happy", init_from_double(_x, 1.0, 53));
    EMIT("happy", init_from_double(_x, -1.0, 53));
    EMIT("happy", init_from_double(_x, 3.14, 53));
    EMIT("happy", init_from_double(_x, -2.71, 53));
    EMIT("happy", init_from_double(_x, 1.5e100, 53));
    EMIT("happy", init_from_double(_x, -1.5e-100, 53));
    EMIT("happy", init_from_double(_x, 42.0, 53));
    EMIT("happy", init_from_double(_x, 0.5, 53));
    EMIT("happy", init_from_double(_x, 0.25, 53));
    EMIT("happy", init_from_double(_x, 6.022e23, 53));
    EMIT("happy", init_from_double(_x, 1.0, 1));
    EMIT("happy", init_from_double(_x, 1.0, 256));
    EMIT("happy", init_from_double(_x, 1.0/3.0, 200));
    EMIT("happy", init_from_double(_x, -1.0/7.0, 100));
    EMIT("happy", init_from_double(_x, 100.5, 32));
    EMIT("happy", init_from_double(_x, 0.1, 24));
    EMIT("happy", init_from_double(_x, 2.0, 53));
    EMIT("happy", init_from_double(_x, 1e10, 53));
    EMIT("happy", init_pos_inf(_x, 53));
    EMIT("happy", init_neg_inf(_x, 53));
    EMIT("happy", init_pos_zero(_x, 53));
    EMIT("happy", init_neg_zero(_x, 53));
    EMIT("happy", init_nan(_x, 53));
    EMIT("happy", init_pos_inf(_x, 1));
    EMIT("happy", init_pos_zero(_x, 256));

    /* edge: 35 — all singular kinds at multiple precs + normal extremes. */
    EMIT("edge", init_nan(_x, 1));
    EMIT("edge", init_nan(_x, 53));
    EMIT("edge", init_nan(_x, 100));
    EMIT("edge", init_nan(_x, 256));
    EMIT("edge", init_nan(_x, 4096));
    EMIT("edge", init_pos_inf(_x, 1));
    EMIT("edge", init_pos_inf(_x, 53));
    EMIT("edge", init_pos_inf(_x, 100));
    EMIT("edge", init_pos_inf(_x, 4096));
    EMIT("edge", init_neg_inf(_x, 1));
    EMIT("edge", init_neg_inf(_x, 53));
    EMIT("edge", init_neg_inf(_x, 100));
    EMIT("edge", init_neg_inf(_x, 4096));
    EMIT("edge", init_pos_zero(_x, 1));
    EMIT("edge", init_pos_zero(_x, 53));
    EMIT("edge", init_pos_zero(_x, 100));
    EMIT("edge", init_pos_zero(_x, 4096));
    EMIT("edge", init_neg_zero(_x, 1));
    EMIT("edge", init_neg_zero(_x, 53));
    EMIT("edge", init_neg_zero(_x, 100));
    EMIT("edge", init_neg_zero(_x, 4096));
    EMIT("edge", init_from_double(_x, 1.0, 1));
    EMIT("edge", init_from_double(_x, -1.0, 1));
    EMIT("edge", init_from_double(_x, 1.0, 4096));
    EMIT("edge", init_from_double(_x, 1.5, 2));
    EMIT("edge", init_from_double(_x, 1.0, 64));
    EMIT("edge", init_from_double(_x, 1.0, 65));
    EMIT("edge", init_from_double(_x, 1.0, 128));
    EMIT("edge", init_from_double(_x, 1.0, 129));
    EMIT("edge", init_from_double(_x, 1.0, 192));
    EMIT("edge", init_from_double(_x, 1.0, 193));
    EMIT("edge", init_from_double(_x, 1.0, 256));
    EMIT("edge", init_from_double(_x, 1.0, 257));
    EMIT("edge", init_from_double(_x, 1e308, 53));
    EMIT("edge", init_from_double(_x, 1e-300, 53));

    /* adversarial: 12 — mostly singulars that the polarity-flip broken
     * gets wrong. */
    EMIT("adversarial", init_nan(_x, 53));
    EMIT("adversarial", init_nan(_x, 1));
    EMIT("adversarial", init_nan(_x, 4096));
    EMIT("adversarial", init_pos_inf(_x, 53));
    EMIT("adversarial", init_neg_inf(_x, 53));
    EMIT("adversarial", init_pos_zero(_x, 53));
    EMIT("adversarial", init_neg_zero(_x, 53));
    EMIT("adversarial", init_pos_zero(_x, 1));
    EMIT("adversarial", init_neg_zero(_x, 1));
    EMIT("adversarial", init_pos_inf(_x, 1));
    EMIT("adversarial", init_from_double(_x, 1.0, 53));
    EMIT("adversarial", init_from_double(_x, -1.0, 53));

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xABCD1234ABCD1234ULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 256);
            const uint64_t kind_choice = xs64_below(&rng, 10);
            mpfr_t x;
            if (kind_choice < 5) {
                /* normal */
                const uint64_t r1 = xs64_next(&rng);
                double xd = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
                if (xd == 0.0) xd = 1.0;
                init_from_double(x, xd, prec);
            } else if (kind_choice < 7) {
                init_pos_zero(x, prec);
            } else if (kind_choice < 8) {
                init_neg_zero(x, prec);
            } else if (kind_choice < 9) {
                init_pos_inf(x, prec);
            } else {
                init_nan(x, prec);
            }
            emit_case(out, "fuzz", x);
            mpfr_clear(x);
        }
    }

    /* mined: 5 — drawn from mpfr/tests/tisnan.c (predicate family). */
    EMIT("mined", init_from_double(_x, 1.0, 53));
    EMIT("mined", init_nan(_x, 53));
    EMIT("mined", init_pos_inf(_x, 53));
    EMIT("mined", init_pos_zero(_x, 53));
    EMIT("mined", init_from_double(_x, -1.0, 53));

    return 0;
}
