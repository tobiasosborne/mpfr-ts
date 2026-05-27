/*
 * golden_driver.c -- Golden master for MPFR's mpfr_mul_ui5.
 *
 * C: static void mpfr_mul_ui5(mpfr_t y, mpfr_srcptr x,
 *      unsigned long v1, v2, v3, v4, v5, mpfr_rnd_t mode).
 *    Ref: mpfr/src/gammaonethird.c L49-L62.
 *
 * Since the C symbol is static (no public linkage), this driver
 * REPLICATES the C algorithm verbatim per ADR 0002 -- the
 * golden-driver-substitute pattern. The faithful reimplementation
 * matches the rounding sequence the static helper would produce.
 *
 * Wire: {"inputs":{"x":<mpfr>,"v1":"<dec>",...,"v5":"<dec>","prec":"<dec>","rnd":"RND_"},
 *        "output":{"value":<mpfr>,"ternary":<int>}}.
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * The driver builds y at the requested prec, calls the substitute, and
 * captures (y, ternary). The ternary returned is from the FINAL
 * mpfr_mul_ui call in the chain -- matching the TS port contract.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <stdint.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_mul_ui5 golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))

/* MPFR_ACC_OR_MUL: literal mirror of gammaonethird.c L25-L35.
 * Updates (y, acc) -- if v fits in ULONG_MAX/acc, multiply into acc;
 * otherwise flush acc into y via mpfr_mul_ui and reset acc to v. */
#define ACC_OR_MUL(v) \
    do { \
        if ((v) <= ULONG_MAX / acc) { \
            acc *= (v); \
        } else { \
            mpfr_mul_ui(y, y, acc, mode); \
            acc = (v); \
        } \
    } while (0)

/* Portable substitute: literal mirror of mpfr/src/gammaonethird.c L49-L62.
 * Returns the ternary from the FINAL mpfr_mul_ui call (the C source returns
 * void; we surface ternary because every TS call has one). */
static int substitute_mul_ui5(mpfr_ptr y, mpfr_srcptr x,
                              unsigned long v1, unsigned long v2,
                              unsigned long v3, unsigned long v4,
                              unsigned long v5, mpfr_rnd_t mode) {
    unsigned long acc = v1;
    mpfr_set(y, x, mode);
    ACC_OR_MUL(v2);
    ACC_OR_MUL(v3);
    ACC_OR_MUL(v4);
    ACC_OR_MUL(v5);
    return mpfr_mul_ui(y, y, acc, mode);
}

static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x,
                             uint64_t v1, uint64_t v2, uint64_t v3,
                             uint64_t v4, uint64_t v5,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= 1 && prec <= TS_PREC_MAX);
    mpfr_t y;
    mpfr_init2(y, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = substitute_mul_ui5(y, x,
                                           (unsigned long)v1,
                                           (unsigned long)v2,
                                           (unsigned long)v3,
                                           (unsigned long)v4,
                                           (unsigned long)v5,
                                           rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_u64(out, 0, "v1", v1);
    jl_kv_u64(out, 0, "v2", v2);
    jl_kv_u64(out, 0, "v3", v3);
    jl_kv_u64(out, 0, "v4", v4);
    jl_kv_u64(out, 0, "v5", v5);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, y, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(y);
}

static inline void emit_d(FILE *out, const char *tag, double xd, uint64_t prec_x,
                          uint64_t v1, uint64_t v2, uint64_t v3,
                          uint64_t v4, uint64_t v5,
                          uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec_x); mpfr_set_d(x, xd, MPFR_RNDN);
    emit_case(out, tag, x, v1, v2, v3, v4, v5, prec, rnd);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = { MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA };

    /* happy: 20 -- typical inputs as used by Browns_const (small v_i's
     * derived from 6k-5, 6k-4, ..., 6k-1 for small k). */
    /* Browns_const k=1: v = (1, 2, 3, 4, 5). */
    emit_d(out, "happy", 1.0, 53, 1, 2, 3, 4, 5, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 53, 1, 2, 3, 4, 5, 64, MPFR_RNDN);
    emit_d(out, "happy", 2.0, 53, 1, 2, 3, 4, 5, 53, MPFR_RNDZ);
    emit_d(out, "happy", -1.5, 53, 1, 2, 3, 4, 5, 53, MPFR_RNDU);
    /* Browns_const k=2: v = (7, 8, 9, 10, 11). */
    emit_d(out, "happy", 1.0, 53, 7, 8, 9, 10, 11, 53, MPFR_RNDN);
    emit_d(out, "happy", 3.14, 53, 7, 8, 9, 10, 11, 53, MPFR_RNDD);
    /* Browns_const k=3: v = (13, 14, 15, 16, 17). */
    emit_d(out, "happy", 1.0, 53, 13, 14, 15, 16, 17, 53, MPFR_RNDN);
    emit_d(out, "happy", 0.5, 53, 13, 14, 15, 16, 17, 53, MPFR_RNDA);
    /* All ones: product = 1, so y = x exactly. */
    emit_d(out, "happy", 3.14, 53, 1, 1, 1, 1, 1, 53, MPFR_RNDN);
    emit_d(out, "happy", -2.71828, 53, 1, 1, 1, 1, 1, 53, MPFR_RNDN);
    /* Mixed small values. */
    emit_d(out, "happy", 1.0, 53, 2, 3, 5, 7, 11, 53, MPFR_RNDN);  /* small primes */
    emit_d(out, "happy", 1.0, 53, 4, 4, 4, 4, 4, 53, MPFR_RNDN);   /* powers of 2 */
    emit_d(out, "happy", 100.0, 100, 2, 3, 5, 7, 11, 100, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 53, 6, 6, 6, 6, 6, 53, MPFR_RNDN);
    emit_d(out, "happy", 0.1, 53, 10, 10, 10, 10, 10, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 64, 99, 98, 97, 96, 95, 64, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 53, 1000, 1000, 1000, 1000, 1000, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 53, 2, 2, 2, 2, 2, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 24, 3, 5, 7, 11, 13, 24, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 200, 100, 200, 300, 400, 500, 200, MPFR_RNDN);

    /* edge: 30 -- specials (NaN, +/-Inf, +/-0), prec=1, v_i=0, v_i=1,
     * v_i values that trigger ACC_OR_MUL overflow flush. */
    /* Specials */
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x);
        emit_case(out, "edge", x, 2, 3, 5, 7, 11, 53, MPFR_RNDN);
        mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, 1);
        emit_case(out, "edge", x, 2, 3, 5, 7, 11, 53, MPFR_RNDN);
        mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, -1);
        emit_case(out, "edge", x, 2, 3, 5, 7, 11, 53, MPFR_RNDN);
        mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, 1);
        emit_case(out, "edge", x, 2, 3, 5, 7, 11, 53, MPFR_RNDN);
        mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1);
        emit_case(out, "edge", x, 2, 3, 5, 7, 11, 53, MPFR_RNDN);
        mpfr_clear(x);
    }
    /* NOTE: v_i = 0 is UB in the C source because the macro
     * (v <= ULONG_MAX / acc) divides by acc which becomes 0. We avoid
     * v_i = 0 in goldens to keep the contract reproducible. The TS port
     * documents and rejects v_i = 0 as well (matching C's effective
     * non-coverage of the value). */
    /* x = +Inf with all positive v_i. */
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, 1);
        emit_case(out, "edge", x, 2, 3, 5, 7, 11, 53, MPFR_RNDN);
        mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, -1);
        emit_case(out, "edge", x, 2, 3, 5, 7, 11, 53, MPFR_RNDN);
        mpfr_clear(x);
    }
    /* Normal x with v_i = 1 (identity multiplications). */
    emit_d(out, "edge", 3.14, 53, 1, 1, 1, 1, 1, 53, MPFR_RNDN);
    emit_d(out, "edge", -3.14, 53, 1, 1, 1, 1, 1, 53, MPFR_RNDN);
    emit_d(out, "edge", 3.14, 53, 2, 1, 1, 1, 1, 53, MPFR_RNDN);
    emit_d(out, "edge", 3.14, 53, 1, 1, 1, 1, 2, 53, MPFR_RNDN);
    /* Identity (all 1s) */
    emit_d(out, "edge", 1.0, 1, 1, 1, 1, 1, 1, 1, MPFR_RNDN);
    emit_d(out, "edge", 3.14, 1, 1, 1, 1, 1, 1, 1, MPFR_RNDN);
    /* prec=1, normal x */
    emit_d(out, "edge", 3.14, 53, 2, 3, 5, 7, 11, 1, MPFR_RNDN);
    emit_d(out, "edge", 3.14, 53, 2, 3, 5, 7, 11, 1, MPFR_RNDZ);
    emit_d(out, "edge", 3.14, 53, 2, 3, 5, 7, 11, 1, MPFR_RNDU);
    emit_d(out, "edge", 3.14, 53, 2, 3, 5, 7, 11, 1, MPFR_RNDD);
    emit_d(out, "edge", 3.14, 53, 2, 3, 5, 7, 11, 1, MPFR_RNDA);
    /* Very large v_i: forces ACC_OR_MUL to flush early. */
    emit_d(out, "edge", 1.0, 53, 1000000000ULL, 1000000000ULL, 2, 3, 5, 53, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 53, 0xFFFFFFFFULL, 2, 3, 5, 7, 53, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 64, 0xFFFFFFFFULL, 0xFFFFFFFFULL, 2, 3, 5, 64, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 64, 0xFFFFFFFFFFFFULL, 2, 3, 5, 7, 64, MPFR_RNDN);
    /* ULONG_MAX edge: each v_i forces immediate flush on next mul. */
    emit_d(out, "edge", 1.0, 64, ULONG_MAX, 2, 3, 5, 7, 64, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 64, ULONG_MAX, ULONG_MAX, 2, 3, 5, 64, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 64, 2, ULONG_MAX, 3, 5, 7, 64, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 64, 2, 3, ULONG_MAX, 5, 7, 64, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 64, 2, 3, 5, ULONG_MAX, 7, 64, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 64, 2, 3, 5, 7, ULONG_MAX, 64, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 24, 2, 3, 5, 7, 11, 24, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 128, 2, 3, 5, 7, 11, 128, MPFR_RNDN);
    emit_d(out, "edge", 1.0, 53, ULONG_MAX, 1, 1, 1, 1, 53, MPFR_RNDN);

    /* adversarial: 12 -- rounding-sensitive flushes, tie cases. */
    emit_d(out, "adversarial", 1.0, 53, 0x100000000ULL, 0x100000000ULL, 2, 3, 5, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 3.0, 53, 0xFFFFFFFFFFFFFFFFULL, 1, 1, 1, 1, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 1.0, 53, 0x100000001ULL, 0x100000001ULL, 2, 3, 5, 53, MPFR_RNDU);
    emit_d(out, "adversarial", 1.0, 53, 0x100000001ULL, 0x100000001ULL, 2, 3, 5, 53, MPFR_RNDD);
    emit_d(out, "adversarial", 1.0, 24, 0x10000ULL, 0x10000ULL, 0x10000ULL, 0x10000ULL, 2, 24, MPFR_RNDN);
    emit_d(out, "adversarial", 1.0, 53, 0xFFFFFFFFULL, 0x100000000ULL, 1, 1, 1, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 1.0, 53, 7, 7, 7, 7, 7, 53, MPFR_RNDN);  /* 7^5 = 16807 */
    emit_d(out, "adversarial", 1.0, 2, 3, 5, 7, 11, 13, 2, MPFR_RNDN);
    emit_d(out, "adversarial", 1.0, 2, 3, 5, 7, 11, 13, 2, MPFR_RNDU);
    emit_d(out, "adversarial", 1.0, 2, 3, 5, 7, 11, 13, 2, MPFR_RNDD);
    emit_d(out, "adversarial", -1.0, 53, ULONG_MAX/2, 3, 1, 1, 1, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 1.0, 53, 0x8000000000000000ULL, 2, 1, 1, 1, 53, MPFR_RNDN);

    /* fuzz: 50 random (x, v_i, prec, rnd). */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xFEEDBEEF0BADF00DULL);
        const uint64_t precs[5] = { 24, 53, 64, 100, 200 };
        for (int rep = 0; rep < 50; ++rep) {
            mpfr_t x;
            const uint64_t prec = precs[xs64_below(&rng, 5)];
            mpfr_init2(x, (mpfr_prec_t)prec);
            /* Random x via xorshift -> double. */
            const uint64_t r = xs64_next(&rng);
            const double base = (double)r / 18446744073709551616.0;
            const int sgn = (xs64_below(&rng, 2)) ? +1 : -1;
            mpfr_set_d(x, sgn * (base + 0.5), MPFR_RNDN);
            /* Random v_i: bias toward small to exercise no-flush + flush paths.
             * All v_i must be > 0 (C UB on v_i = 0 in the ACC_OR_MUL macro). */
            uint64_t vs[5];
            for (int i = 0; i < 5; ++i) {
                const uint64_t kind = xs64_below(&rng, 4);
                if (kind == 0) vs[i] = 1 + xs64_below(&rng, 20);                /* small */
                else if (kind == 1) vs[i] = 1 + xs64_below(&rng, 1000000);      /* medium */
                else if (kind == 2) vs[i] = 1 + xs64_below(&rng, 0xFFFFFFFFULL); /* large */
                else vs[i] = xs64_next(&rng) | 1;                                /* huge (odd, >=1) */
                if (vs[i] == 0) vs[i] = 1;
            }
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            emit_case(out, "fuzz", x, vs[0], vs[1], vs[2], vs[3], vs[4], prec, rnd);
            mpfr_clear(x);
        }
    }

    /* mined: 5 -- patterns directly from Browns_const usage. */
    emit_d(out, "mined", 1.0, 53, 1, 2, 3, 4, 5, 53, MPFR_RNDN);  /* k=1 */
    emit_d(out, "mined", 1.0, 53, 7, 8, 9, 10, 11, 53, MPFR_RNDN);  /* k=2 */
    emit_d(out, "mined", 1.0, 53, 13, 14, 15, 16, 17, 53, MPFR_RNDN);  /* k=3 */
    emit_d(out, "mined", 1.0, 100, 1, 2, 3, 4, 5, 100, MPFR_RNDN);
    emit_d(out, "mined", 1.0, 200, 1, 2, 3, 4, 5, 200, MPFR_RNDN);

    return 0;
}
