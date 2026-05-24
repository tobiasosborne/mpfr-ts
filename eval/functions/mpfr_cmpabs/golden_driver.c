/*
 * golden_driver.c -- Golden master for MPFR's mpfr_cmpabs.
 *
 * Returns sign of (|b| - |c|). NaN inputs set erange and return 0 in C;
 * the TS port throws MPFRError('EDOMAIN') on NaN. We therefore do NOT
 * emit cases with NaN inputs (those are graded as throws separately).
 *
 * Output normalised to {-1, 0, +1}.
 *
 * Tag distribution: happy 25, edge 30, adversarial 12, fuzz 60, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_cmpabs golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline int normalise(int r) {
    return (r > 0) ? 1 : ((r < 0) ? -1 : 0);
}

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr b, mpfr_srcptr c) {
    assert(!mpfr_nan_p(b) && !mpfr_nan_p(c));
    const uint64_t t0 = now_ns();
    const int raw = mpfr_cmpabs(b, c);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "b", b);
    jl_kv_mpfr(out, 0, "c", c);
    jl_end_inputs(out);
    jl_output_scalar_int(out, normalise(raw));
    jl_finish(out, elapsed);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}

static inline void init_from_si(mpfr_ptr x, uint64_t prec, long v) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_si(x, v, MPFR_RNDN);
}

static inline void init_pos_inf(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, 1); }
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, -1); }
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, 1); }
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, -1); }

#define PAIR_E(tag, ie1, ie2) do { mpfr_t _b, _c; ie1; ie2; emit_case(out, tag, _b, _c); mpfr_clear(_b); mpfr_clear(_c); } while (0)

int main(void) {
    FILE *out = stdout;

    /* happy: 25 -- mix of normal pairs of various signs and magnitudes. */
    PAIR_E("happy", init_from_double(_b, 1.0, 53), init_from_double(_c, 1.0, 53));
    PAIR_E("happy", init_from_double(_b, 1.0, 53), init_from_double(_c, 2.0, 53));
    PAIR_E("happy", init_from_double(_b, 2.0, 53), init_from_double(_c, 1.0, 53));
    PAIR_E("happy", init_from_double(_b, -1.0, 53), init_from_double(_c, 1.0, 53));   /* |b|=|c| */
    PAIR_E("happy", init_from_double(_b, 1.0, 53), init_from_double(_c, -1.0, 53));   /* |b|=|c| */
    PAIR_E("happy", init_from_double(_b, -2.0, 53), init_from_double(_c, 1.0, 53));   /* |b|>|c| */
    PAIR_E("happy", init_from_double(_b, 1.0, 53), init_from_double(_c, -2.0, 53));   /* |b|<|c| */
    PAIR_E("happy", init_from_double(_b, 3.14, 53), init_from_double(_c, 2.71, 53));
    PAIR_E("happy", init_from_double(_b, -3.14, 53), init_from_double(_c, 2.71, 53));
    PAIR_E("happy", init_from_double(_b, 1e100, 53), init_from_double(_c, 1e100, 53));
    PAIR_E("happy", init_from_double(_b, 1e100, 53), init_from_double(_c, 1e-100, 53));
    PAIR_E("happy", init_from_double(_b, 1e-100, 53), init_from_double(_c, 1e100, 53));
    PAIR_E("happy", init_from_si(_b, 53, 100), init_from_si(_c, 53, -100));
    PAIR_E("happy", init_from_si(_b, 53, -100), init_from_si(_c, 53, -200));
    PAIR_E("happy", init_from_si(_b, 53, -200), init_from_si(_c, 53, -100));
    PAIR_E("happy", init_from_double(_b, 0.5, 53), init_from_double(_c, 0.25, 53));
    PAIR_E("happy", init_from_double(_b, -0.5, 53), init_from_double(_c, -0.25, 53));
    PAIR_E("happy", init_from_double(_b, 1.0, 24), init_from_double(_c, 1.0, 53));     /* same value, different prec */
    PAIR_E("happy", init_from_double(_b, 1.5, 24), init_from_double(_c, 1.5, 53));     /* same value, different prec */
    PAIR_E("happy", init_from_double(_b, 1.5, 53), init_from_double(_c, 1.5, 200));    /* same value, different prec */
    PAIR_E("happy", init_from_double(_b, 1.5, 53), init_from_double(_c, 1.5 + 1e-10, 53));
    PAIR_E("happy", init_from_double(_b, 6.022e23, 53), init_from_double(_c, 6.022e23, 53));
    PAIR_E("happy", init_from_double(_b, -42.0, 53), init_from_double(_c, 42.0, 53));
    PAIR_E("happy", init_from_double(_b, 1.0, 53), init_from_double(_c, 1.5, 53));
    PAIR_E("happy", init_from_double(_b, 1.5, 53), init_from_double(_c, 1.0, 53));

    /* edge: 30 -- specials x normals, signed zero (which collapses), Inf. */
    PAIR_E("edge", init_pos_inf(_b, 53), init_pos_inf(_c, 53));         /* |Inf| = |Inf| -> 0 */
    PAIR_E("edge", init_pos_inf(_b, 53), init_neg_inf(_c, 53));         /* |Inf| = |-Inf| -> 0 */
    PAIR_E("edge", init_neg_inf(_b, 53), init_neg_inf(_c, 53));         /* equal */
    PAIR_E("edge", init_pos_inf(_b, 53), init_from_double(_c, 1e308, 53));  /* Inf > finite */
    PAIR_E("edge", init_neg_inf(_b, 53), init_from_double(_c, 1e308, 53));  /* |-Inf| > finite */
    PAIR_E("edge", init_from_double(_b, 1e308, 53), init_pos_inf(_c, 53));  /* finite < Inf */
    PAIR_E("edge", init_pos_inf(_b, 53), init_pos_zero(_c, 53));        /* Inf > 0 */
    PAIR_E("edge", init_pos_zero(_b, 53), init_pos_inf(_c, 53));
    PAIR_E("edge", init_pos_zero(_b, 53), init_pos_zero(_c, 53));       /* 0 = 0 */
    PAIR_E("edge", init_pos_zero(_b, 53), init_neg_zero(_c, 53));       /* |0| = |-0| */
    PAIR_E("edge", init_neg_zero(_b, 53), init_neg_zero(_c, 53));
    PAIR_E("edge", init_pos_zero(_b, 53), init_from_double(_c, 1.0, 53));  /* 0 < 1 */
    PAIR_E("edge", init_pos_zero(_b, 53), init_from_double(_c, -1.0, 53)); /* 0 < |-1| */
    PAIR_E("edge", init_from_double(_b, 1.0, 53), init_pos_zero(_c, 53));
    PAIR_E("edge", init_from_double(_b, -1.0, 53), init_pos_zero(_c, 53));
    /* prec=1 boundaries. */
    PAIR_E("edge", init_from_double(_b, 1.0, 1), init_from_double(_c, 1.0, 1));
    PAIR_E("edge", init_from_double(_b, 1.0, 1), init_from_double(_c, -1.0, 1));
    PAIR_E("edge", init_from_double(_b, 1.0, 1), init_from_double(_c, 1.0, 256));
    /* Same value at very different precs. */
    PAIR_E("edge", init_from_double(_b, 1.0, 1), init_from_double(_c, 1.0, 4096));
    PAIR_E("edge", init_from_double(_b, 3.0, 2), init_from_double(_c, 3.0, 4096));
    /* Adjacent exponents. */
    PAIR_E("edge", init_from_double(_b, 1.0, 53), init_from_double(_c, 2.0, 53));
    PAIR_E("edge", init_from_double(_b, 2.0, 53), init_from_double(_c, 4.0, 53));
    /* Same exp, mantissa diff. */
    PAIR_E("edge", init_from_double(_b, 1.5, 53), init_from_double(_c, 1.25, 53));
    PAIR_E("edge", init_from_double(_b, 1.25, 53), init_from_double(_c, 1.5, 53));
    /* Mixed signs but |equal|. */
    PAIR_E("edge", init_from_si(_b, 53, 5), init_from_si(_c, 53, -5));
    PAIR_E("edge", init_from_si(_b, 53, -7), init_from_si(_c, 53, 7));
    /* Cross-prec same value with sign. */
    PAIR_E("edge", init_from_double(_b, -1.0, 24), init_from_double(_c, 1.0, 53));
    PAIR_E("edge", init_from_double(_b, 1.0, 24), init_from_double(_c, -1.0, 53));
    /* Just-different magnitudes at high prec. */
    PAIR_E("edge", init_from_double(_b, 1.0, 100), init_from_double(_c, 1.0 + 1e-15, 100));
    PAIR_E("edge", init_from_double(_b, 1.0 + 1e-15, 100), init_from_double(_c, 1.0, 100));

    /* adversarial: 12 -- same-magnitude cross-sign and cross-prec. */
    PAIR_E("adversarial", init_from_double(_b, 1.5, 24), init_from_double(_c, 1.5, 53));
    PAIR_E("adversarial", init_from_double(_b, 1.5, 53), init_from_double(_c, 1.5, 24));
    PAIR_E("adversarial", init_from_double(_b, -1.5, 24), init_from_double(_c, 1.5, 53));
    PAIR_E("adversarial", init_from_double(_b, 1.5, 24), init_from_double(_c, -1.5, 53));
    PAIR_E("adversarial", init_from_double(_b, 3.0, 2), init_from_double(_c, 3.0, 4096));
    PAIR_E("adversarial", init_from_double(_b, -3.0, 2), init_from_double(_c, 3.0, 4096));
    PAIR_E("adversarial", init_from_double(_b, 1.0, 53), init_from_double(_c, 1.0 + 1e-308, 53));
    PAIR_E("adversarial", init_from_double(_b, 1e308, 53), init_from_double(_c, 1e308, 53));
    PAIR_E("adversarial", init_from_double(_b, 1e-300, 53), init_from_double(_c, 1e-300, 53));
    PAIR_E("adversarial", init_from_double(_b, -1e308, 53), init_from_double(_c, 1e308, 53));
    PAIR_E("adversarial", init_from_double(_b, 1.0, 53), init_from_double(_c, 1.0, 53));   /* exact equal */
    PAIR_E("adversarial", init_from_double(_b, -1.0, 53), init_from_double(_c, -1.0, 53)); /* exact equal */

    /* fuzz: 60 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEEC0FFEEC0FFULL);
        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t bprec = 1 + xs64_below(&rng, 256);
            const uint64_t cprec = 1 + xs64_below(&rng, 256);
            const uint64_t r1 = xs64_next(&rng);
            const uint64_t r2 = xs64_next(&rng);
            double bd = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            double cd = ((double)(r2 % 200000ULL) - 100000.0) / 100.0;
            mpfr_t b, c;
            init_from_double(b, bd, bprec);
            init_from_double(c, cd, cprec);
            emit_case(out, "fuzz", b, c);
            mpfr_clear(b);
            mpfr_clear(c);
        }
    }

    /* mined: 5 -- drawn from tcmpabs.c shapes. */
    PAIR_E("mined", init_from_double(_b, 1.0, 53), init_from_double(_c, 1.0, 53));
    PAIR_E("mined", init_from_double(_b, -1.0, 53), init_from_double(_c, 1.0, 53));
    PAIR_E("mined", init_from_double(_b, 2.0, 53), init_from_double(_c, 1.0, 53));
    PAIR_E("mined", init_pos_inf(_b, 53), init_from_double(_c, 1.0, 53));
    PAIR_E("mined", init_pos_zero(_b, 53), init_from_double(_c, 1.0, 53));

    return 0;
}
