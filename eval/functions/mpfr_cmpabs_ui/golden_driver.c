/*
 * golden_driver.c -- Golden master for MPFR's mpfr_cmpabs_ui.
 *
 * Returns sign of (|b| - c). NaN inputs throw on TS, so omitted from
 * the golden.
 *
 * Output normalised to {-1, 0, +1}.
 *
 * Tag distribution: happy 22, edge 30, adversarial 10, fuzz 55, mined 5.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_cmpabs_ui golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline int normalise(int r) {
    return (r > 0) ? 1 : ((r < 0) ? -1 : 0);
}

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr b, unsigned long c) {
    assert(!mpfr_nan_p(b));
    const uint64_t t0 = now_ns();
    const int raw = mpfr_cmpabs_ui(b, c);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "b", b);
    jl_kv_u64(out, 0, "c", (uint64_t)c);
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

#define E2(tag, ie, c) do { mpfr_t _b; ie; emit_case(out, tag, _b, c); mpfr_clear(_b); } while (0)

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    E2("happy", init_from_si(_b, 53, 5), 5);
    E2("happy", init_from_si(_b, 53, -5), 5);   /* |-5| = 5 -> 0 */
    E2("happy", init_from_si(_b, 53, 5), 10);   /* < */
    E2("happy", init_from_si(_b, 53, 10), 5);   /* > */
    E2("happy", init_from_si(_b, 53, 0), 0);    /* 0 vs 0 */
    E2("happy", init_from_si(_b, 53, 0), 1);    /* 0 < 1 */
    E2("happy", init_from_si(_b, 53, 1), 0);    /* 1 > 0 */
    E2("happy", init_from_si(_b, 53, -1), 0);   /* |-1|>0 */
    E2("happy", init_from_si(_b, 53, 100), 100);
    E2("happy", init_from_si(_b, 53, -100), 100);
    E2("happy", init_from_double(_b, 1.5, 53), 1);   /* 1.5 > 1 */
    E2("happy", init_from_double(_b, 1.5, 53), 2);   /* 1.5 < 2 */
    E2("happy", init_from_double(_b, 0.5, 53), 0);   /* 0.5 > 0 */
    E2("happy", init_from_double(_b, 0.5, 53), 1);   /* 0.5 < 1 */
    E2("happy", init_from_double(_b, -3.14, 53), 3); /* 3.14 > 3 */
    E2("happy", init_from_double(_b, -3.14, 53), 4); /* 3.14 < 4 */
    E2("happy", init_from_si(_b, 64, 1000000), 1000000);
    E2("happy", init_from_si(_b, 64, 1000000), 999999);
    E2("happy", init_from_si(_b, 64, -1000000), 1000001);
    E2("happy", init_from_double(_b, 1e10, 53), 10000000000UL);
    E2("happy", init_from_double(_b, 1.0, 24), 1);
    E2("happy", init_from_double(_b, 1.0, 1), 1);

    /* edge: 30 -- Inf/zero/cross-prec. */
    E2("edge", init_pos_inf(_b, 53), 0);
    E2("edge", init_pos_inf(_b, 53), 1);
    E2("edge", init_pos_inf(_b, 53), ULONG_MAX);
    E2("edge", init_neg_inf(_b, 53), 1);
    E2("edge", init_pos_zero(_b, 53), 0);
    E2("edge", init_pos_zero(_b, 53), 1);
    E2("edge", init_neg_zero(_b, 53), 0);
    E2("edge", init_neg_zero(_b, 53), 1);
    /* ULONG_MAX boundary. */
    E2("edge", init_from_double(_b, 1e20, 64), ULONG_MAX);
    E2("edge", init_from_double(_b, 1e19, 64), ULONG_MAX);
    E2("edge", init_from_double(_b, 1e30, 64), ULONG_MAX);
    /* prec=1, value +/-1. */
    E2("edge", init_from_si(_b, 1, 1), 0);
    E2("edge", init_from_si(_b, 1, 1), 1);
    E2("edge", init_from_si(_b, 1, 1), 2);
    E2("edge", init_from_si(_b, 1, -1), 1);
    /* High prec exact c. */
    E2("edge", init_from_si(_b, 256, 42), 42);
    E2("edge", init_from_si(_b, 256, 43), 42);
    E2("edge", init_from_si(_b, 256, 41), 42);
    /* Non-integer b vs integer c. */
    E2("edge", init_from_double(_b, 2.5, 53), 2);
    E2("edge", init_from_double(_b, 2.5, 53), 3);
    E2("edge", init_from_double(_b, -2.5, 53), 2);
    E2("edge", init_from_double(_b, -2.5, 53), 3);
    /* c=0 special. */
    E2("edge", init_from_si(_b, 53, 1), 0);
    E2("edge", init_from_si(_b, 53, -1), 0);
    E2("edge", init_from_si(_b, 53, 1000000000), 0);
    /* High-magnitude vs moderate c. */
    E2("edge", init_from_double(_b, 1e100, 53), 1);
    E2("edge", init_from_double(_b, 1e100, 53), 1000000);
    E2("edge", init_from_double(_b, 1e-100, 53), 0);
    E2("edge", init_from_double(_b, 1e-100, 53), 1);

    /* adversarial: 10 -- near-equal cases. */
    E2("adversarial", init_from_double(_b, 1.0 - 1e-15, 53), 1);   /* < */
    E2("adversarial", init_from_double(_b, 1.0 + 1e-15, 53), 1);   /* > */
    E2("adversarial", init_from_double(_b, 100.5, 53), 100);
    E2("adversarial", init_from_double(_b, 100.5, 53), 101);
    E2("adversarial", init_from_double(_b, -100.5, 53), 101);
    E2("adversarial", init_from_si(_b, 64, 1L << 62), (unsigned long)(1ULL << 62));
    E2("adversarial", init_from_si(_b, 64, (1L << 62) - 1), (unsigned long)(1ULL << 62));
    E2("adversarial", init_from_si(_b, 64, (1L << 62) + 1), (unsigned long)(1ULL << 62));
    E2("adversarial", init_pos_zero(_b, 53), 0);
    E2("adversarial", init_neg_zero(_b, 53), 0);

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x7777777777777777ULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t bprec = 1 + xs64_below(&rng, 256);
            const uint64_t r1 = xs64_next(&rng);
            double bd = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            const unsigned long c = (unsigned long)xs64_below(&rng, 1000000);
            mpfr_t b;
            init_from_double(b, bd, bprec);
            if (mpfr_nan_p(b)) { mpfr_clear(b); continue; }
            emit_case(out, "fuzz", b, c);
            mpfr_clear(b);
        }
    }

    /* mined: 5 */
    E2("mined", init_from_si(_b, 53, 5), 5);
    E2("mined", init_from_si(_b, 53, -5), 5);
    E2("mined", init_from_si(_b, 53, 5), 10);
    E2("mined", init_pos_inf(_b, 53), 1);
    E2("mined", init_pos_zero(_b, 53), 0);

    return 0;
}
