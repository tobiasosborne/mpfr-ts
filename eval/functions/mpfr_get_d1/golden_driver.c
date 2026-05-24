/*
 * golden_driver.c -- Golden master for MPFR's mpfr_get_d1.
 *
 * Returns the double closest to x, using the library default rounding
 * mode. The TS port reads its own module-level default-rounding-mode
 * (initialised to 'RNDN'); the driver does NOT mutate libmpfr's default
 * across cases, so every golden case is graded under RNDN.
 *
 * Wire: bare double output (sentinel strings for NaN/+/-Inf).
 *
 * Tag distribution: happy 25, edge 30, adversarial 12, fuzz 55, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_get_d1 golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    /* mpfr_get_d1 reads the library default; ensure it's RNDN. */
    mpfr_set_default_rounding_mode(MPFR_RNDN);
    const uint64_t t0 = now_ns();
    const double v = mpfr_get_d1(x);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    jl_output_scalar_double(out, v);
    jl_finish(out, elapsed);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_pos_inf(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, 1); }
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, -1); }
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, 1); }
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, -1); }
static inline void init_nan(mpfr_ptr x, uint64_t prec)     { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_nan(x); }

#define E1(tag, ie) do { mpfr_t _x; ie; emit_case(out, tag, _x); mpfr_clear(_x); } while (0)

int main(void) {
    FILE *out = stdout;

    /* happy: 25 */
    E1("happy", init_from_double(_x, 1.0, 53));
    E1("happy", init_from_double(_x, 2.0, 53));
    E1("happy", init_from_double(_x, 3.14, 53));
    E1("happy", init_from_double(_x, -3.14, 53));
    E1("happy", init_from_double(_x, 0.5, 53));
    E1("happy", init_from_double(_x, 0.25, 53));
    E1("happy", init_from_double(_x, 0.125, 53));
    E1("happy", init_from_double(_x, 1e10, 53));
    E1("happy", init_from_double(_x, 1e-10, 53));
    E1("happy", init_from_double(_x, 1e100, 53));
    E1("happy", init_from_double(_x, 1e-100, 53));
    E1("happy", init_from_double(_x, -1.0, 53));
    E1("happy", init_from_double(_x, 0.0, 53));
    E1("happy", init_from_double(_x, 1.0, 1));
    E1("happy", init_from_double(_x, -1.0, 1));
    E1("happy", init_from_double(_x, 1.0/3.0, 53));
    E1("happy", init_from_double(_x, 1.0/3.0, 100));
    E1("happy", init_from_double(_x, 1.0/3.0, 200));
    E1("happy", init_from_double(_x, 100.0, 53));
    E1("happy", init_from_double(_x, 6.022e23, 53));
    E1("happy", init_from_double(_x, 1.4142135623730951, 53));
    E1("happy", init_from_double(_x, -3.141592653589793, 53));
    E1("happy", init_from_double(_x, 2.718281828459045, 53));
    E1("happy", init_from_double(_x, 17.0, 24));
    E1("happy", init_from_double(_x, -42.0, 64));

    /* edge: 30 */
    E1("edge", init_nan(_x, 53));
    E1("edge", init_nan(_x, 100));
    E1("edge", init_nan(_x, 4096));
    E1("edge", init_pos_inf(_x, 53));
    E1("edge", init_pos_inf(_x, 100));
    E1("edge", init_pos_inf(_x, 4096));
    E1("edge", init_neg_inf(_x, 53));
    E1("edge", init_neg_inf(_x, 100));
    E1("edge", init_neg_inf(_x, 4096));
    E1("edge", init_pos_zero(_x, 53));
    E1("edge", init_neg_zero(_x, 53));
    E1("edge", init_pos_zero(_x, 1));
    E1("edge", init_neg_zero(_x, 1));
    E1("edge", init_pos_zero(_x, 4096));
    E1("edge", init_neg_zero(_x, 4096));
    /* Subnormal-range doubles. */
    E1("edge", init_from_double(_x, DBL_MIN, 53));
    E1("edge", init_from_double(_x, -DBL_MIN, 53));
    E1("edge", init_from_double(_x, DBL_MAX, 53));
    E1("edge", init_from_double(_x, -DBL_MAX, 53));
    E1("edge", init_from_double(_x, DBL_EPSILON, 53));
    /* prec=1 */
    E1("edge", init_from_double(_x, 1.0, 1));
    E1("edge", init_from_double(_x, -1.0, 1));
    /* High prec. */
    E1("edge", init_from_double(_x, 1.0, 4096));
    E1("edge", init_from_double(_x, 1.5, 2));
    E1("edge", init_from_double(_x, 1.5, 24));
    E1("edge", init_from_double(_x, 1.5, 200));
    /* Near-integer. */
    E1("edge", init_from_double(_x, 1.0 + DBL_EPSILON, 53));
    E1("edge", init_from_double(_x, 1.0 - DBL_EPSILON, 53));
    E1("edge", init_from_double(_x, 100.5, 53));
    E1("edge", init_from_double(_x, 100.5, 32));

    /* adversarial: 12 -- overflow/underflow-on-conversion. */
    E1("adversarial", init_from_double(_x, 1e308, 53));
    E1("adversarial", init_from_double(_x, -1e308, 53));
    E1("adversarial", init_from_double(_x, DBL_MAX, 64));
    E1("adversarial", init_from_double(_x, -DBL_MAX, 64));
    E1("adversarial", init_from_double(_x, 0.5, 53));
    E1("adversarial", init_from_double(_x, -0.5, 53));
    E1("adversarial", init_from_double(_x, 1.0, 100));
    E1("adversarial", init_from_double(_x, 1.5, 100));
    E1("adversarial", init_from_double(_x, 0.1, 53));
    E1("adversarial", init_from_double(_x, -0.1, 53));
    E1("adversarial", init_from_double(_x, 6.022e23, 53));
    E1("adversarial", init_from_double(_x, 6.022e23, 100));

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5151515151515151ULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 256);
            const uint64_t r1 = xs64_next(&rng);
            double d = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            mpfr_t x;
            init_from_double(x, d, prec);
            emit_case(out, "fuzz", x);
            mpfr_clear(x);
        }
    }

    /* mined: 5 */
    E1("mined", init_from_double(_x, 1.0, 53));
    E1("mined", init_from_double(_x, 3.14, 53));
    E1("mined", init_pos_inf(_x, 53));
    E1("mined", init_pos_zero(_x, 53));
    E1("mined", init_nan(_x, 53));

    return 0;
}
