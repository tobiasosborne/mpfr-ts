/*
 * golden_driver.c -- Golden master for MPFR's mpfr_cmp3.
 *
 * Compares b against sign(s) * c. NaN throws on the TS side, so the
 * driver does NOT emit NaN cases.
 *
 * Output normalised to {-1, 0, +1}.
 *
 * Tag distribution: happy 22, edge 30, adversarial 12, fuzz 60, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_cmp3 golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline int normalise(int r) {
    return (r > 0) ? 1 : ((r < 0) ? -1 : 0);
}

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr b, mpfr_srcptr c, int s) {
    assert(!mpfr_nan_p(b) && !mpfr_nan_p(c));
    /* s typically +/-1; the C source uses MPFR_MULT_SIGN which treats 0 as 0.
     * We only emit +/-1. */
    assert(s == 1 || s == -1);
    const uint64_t t0 = now_ns();
    const int raw = mpfr_cmp3(b, c, s);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "b", b);
    jl_kv_mpfr(out, 0, "c", c);
    jl_kv_int(out, 0, "s", s);
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

#define E3(tag, ie1, ie2, s) do { mpfr_t _b, _c; ie1; ie2; emit_case(out, tag, _b, _c, s); mpfr_clear(_b); mpfr_clear(_c); } while (0)

int main(void) {
    FILE *out = stdout;

    /* happy: 22 -- pairs across s=+1 and s=-1. */
    E3("happy", init_from_double(_b, 1.0, 53), init_from_double(_c, 1.0, 53), +1);
    E3("happy", init_from_double(_b, 1.0, 53), init_from_double(_c, 1.0, 53), -1);
    E3("happy", init_from_double(_b, 1.0, 53), init_from_double(_c, -1.0, 53), -1);  /* b vs -1*-1 = 1; equal */
    E3("happy", init_from_double(_b, 2.0, 53), init_from_double(_c, 1.0, 53), +1);
    E3("happy", init_from_double(_b, 2.0, 53), init_from_double(_c, 1.0, 53), -1);   /* b vs -1; b > */
    E3("happy", init_from_double(_b, -2.0, 53), init_from_double(_c, 1.0, 53), -1);  /* b vs -1; b < */
    E3("happy", init_from_double(_b, 3.14, 53), init_from_double(_c, 2.71, 53), +1);
    E3("happy", init_from_double(_b, 3.14, 53), init_from_double(_c, 2.71, 53), -1);
    E3("happy", init_from_double(_b, -3.14, 53), init_from_double(_c, 2.71, 53), +1);
    E3("happy", init_from_double(_b, -3.14, 53), init_from_double(_c, 2.71, 53), -1);
    E3("happy", init_from_si(_b, 53, 10), init_from_si(_c, 53, 10), +1);
    E3("happy", init_from_si(_b, 53, 10), init_from_si(_c, 53, -10), -1);   /* 10 vs -1*-10 = 10 */
    E3("happy", init_from_si(_b, 53, -10), init_from_si(_c, 53, 10), -1);   /* -10 vs -10 */
    E3("happy", init_from_double(_b, 1e100, 53), init_from_double(_c, 1e-100, 53), +1);
    E3("happy", init_from_double(_b, 1e100, 53), init_from_double(_c, 1e-100, 53), -1);
    E3("happy", init_from_double(_b, 1.0, 24), init_from_double(_c, 1.0, 53), +1);
    E3("happy", init_from_double(_b, 1.0, 24), init_from_double(_c, 1.0, 53), -1);
    E3("happy", init_from_double(_b, 1.5, 53), init_from_double(_c, 1.5, 200), +1);
    E3("happy", init_from_double(_b, 1.5, 53), init_from_double(_c, 1.5, 200), -1);
    E3("happy", init_from_si(_b, 53, 0), init_from_si(_c, 53, 1), +1);   /* 0 < 1 */
    E3("happy", init_from_si(_b, 53, 0), init_from_si(_c, 53, 1), -1);   /* 0 > -1 */
    E3("happy", init_from_si(_b, 53, 0), init_from_si(_c, 53, -1), -1);  /* 0 < 1 */

    /* edge: 30 -- Inf/zero/sign edges. */
    E3("edge", init_pos_inf(_b, 53), init_pos_inf(_c, 53), +1);   /* +Inf vs +Inf -> 0 */
    E3("edge", init_pos_inf(_b, 53), init_pos_inf(_c, 53), -1);   /* +Inf vs -Inf -> +1 */
    E3("edge", init_neg_inf(_b, 53), init_pos_inf(_c, 53), +1);   /* -Inf vs +Inf -> -1 */
    E3("edge", init_neg_inf(_b, 53), init_pos_inf(_c, 53), -1);   /* -Inf vs -Inf -> 0 */
    E3("edge", init_pos_inf(_b, 53), init_from_double(_c, 1.0, 53), +1);
    E3("edge", init_neg_inf(_b, 53), init_from_double(_c, 1.0, 53), -1);
    E3("edge", init_from_double(_b, 1.0, 53), init_pos_inf(_c, 53), +1);
    E3("edge", init_from_double(_b, 1.0, 53), init_pos_inf(_c, 53), -1);
    E3("edge", init_pos_zero(_b, 53), init_pos_zero(_c, 53), +1);
    E3("edge", init_pos_zero(_b, 53), init_neg_zero(_c, 53), +1);  /* +0 vs +0 (-1*-0=+0) -- wait check */
    E3("edge", init_pos_zero(_b, 53), init_neg_zero(_c, 53), -1);
    E3("edge", init_pos_zero(_b, 53), init_from_si(_c, 53, 1), +1);   /* 0 < 1 */
    E3("edge", init_pos_zero(_b, 53), init_from_si(_c, 53, 1), -1);   /* 0 > -1 */
    E3("edge", init_pos_zero(_b, 53), init_from_si(_c, 53, -1), +1);
    E3("edge", init_pos_zero(_b, 53), init_from_si(_c, 53, -1), -1);
    E3("edge", init_from_si(_b, 53, 1), init_pos_zero(_c, 53), +1);
    E3("edge", init_from_si(_b, 53, 1), init_pos_zero(_c, 53), -1);
    E3("edge", init_from_si(_b, 53, -1), init_pos_zero(_c, 53), +1);
    E3("edge", init_from_si(_b, 53, -1), init_pos_zero(_c, 53), -1);
    /* prec=1 */
    E3("edge", init_from_double(_b, 1.0, 1), init_from_double(_c, 1.0, 1), +1);
    E3("edge", init_from_double(_b, 1.0, 1), init_from_double(_c, 1.0, 1), -1);
    E3("edge", init_from_double(_b, 1.0, 1), init_from_double(_c, -1.0, 1), +1);
    E3("edge", init_from_double(_b, 1.0, 1), init_from_double(_c, -1.0, 1), -1);
    /* Same value at very different prec. */
    E3("edge", init_from_double(_b, 1.0, 1), init_from_double(_c, 1.0, 4096), +1);
    E3("edge", init_from_double(_b, 3.0, 2), init_from_double(_c, 3.0, 4096), -1);
    /* exp adjacency. */
    E3("edge", init_from_double(_b, 2.0, 53), init_from_double(_c, 1.0, 53), -1);  /* 2 vs -1 -> +1 */
    E3("edge", init_from_double(_b, 1.0, 53), init_from_double(_c, 2.0, 53), -1);  /* 1 vs -2 -> +1 */
    /* sign flips at high prec */
    E3("edge", init_from_double(_b, 1e308, 53), init_from_double(_c, 1e308, 53), -1);  /* 1e308 vs -1e308 -> +1 */
    E3("edge", init_from_double(_b, -1e308, 53), init_from_double(_c, 1e308, 53), -1); /* equal */
    E3("edge", init_from_double(_b, 1.0/3.0, 200), init_from_double(_c, 1.0/3.0, 53), +1);

    /* adversarial: 12 -- sign-multiplier traps. */
    E3("adversarial", init_from_double(_b, 1.0, 53), init_from_double(_c, 1.0, 53), -1);   /* 1 vs -1 -> +1 */
    E3("adversarial", init_from_double(_b, -1.0, 53), init_from_double(_c, -1.0, 53), -1); /* -1 vs +1 -> -1 */
    E3("adversarial", init_from_double(_b, 1.0, 53), init_from_double(_c, -1.0, 53), -1);  /* 1 vs +1 -> 0 */
    E3("adversarial", init_from_double(_b, -1.0, 53), init_from_double(_c, 1.0, 53), -1);  /* -1 vs -1 -> 0 */
    E3("adversarial", init_from_double(_b, 1.0, 53), init_from_double(_c, 1.0 + 1e-15, 53), +1);
    E3("adversarial", init_from_double(_b, 1.0, 53), init_from_double(_c, 1.0 + 1e-15, 53), -1);
    E3("adversarial", init_from_double(_b, 1.5, 24), init_from_double(_c, 1.5, 53), -1);
    E3("adversarial", init_from_double(_b, -1.5, 24), init_from_double(_c, 1.5, 53), -1);
    E3("adversarial", init_pos_inf(_b, 53), init_neg_inf(_c, 53), -1);   /* +Inf vs +Inf -> 0 */
    E3("adversarial", init_pos_inf(_b, 53), init_neg_inf(_c, 53), +1);   /* +Inf vs -Inf -> +1 */
    E3("adversarial", init_pos_zero(_b, 53), init_neg_zero(_c, 53), +1); /* +0 vs -0 -> 0 */
    E3("adversarial", init_neg_zero(_b, 53), init_pos_zero(_c, 53), -1);

    /* fuzz: 60 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x3333333333333333ULL);
        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t bprec = 1 + xs64_below(&rng, 256);
            const uint64_t cprec = 1 + xs64_below(&rng, 256);
            const uint64_t r1 = xs64_next(&rng);
            const uint64_t r2 = xs64_next(&rng);
            double bd = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            double cd = ((double)(r2 % 200000ULL) - 100000.0) / 100.0;
            const int s = (xs64_below(&rng, 2)) ? +1 : -1;
            mpfr_t b, c;
            init_from_double(b, bd, bprec);
            init_from_double(c, cd, cprec);
            emit_case(out, "fuzz", b, c, s);
            mpfr_clear(b);
            mpfr_clear(c);
        }
    }

    /* mined: 5 */
    E3("mined", init_from_double(_b, 1.0, 53), init_from_double(_c, 1.0, 53), +1);
    E3("mined", init_from_double(_b, 1.0, 53), init_from_double(_c, -1.0, 53), -1);
    E3("mined", init_from_si(_b, 53, 2), init_from_si(_c, 53, 1), +1);
    E3("mined", init_pos_inf(_b, 53), init_from_double(_c, 1.0, 53), +1);
    E3("mined", init_pos_zero(_b, 53), init_pos_zero(_c, 53), +1);

    return 0;
}
