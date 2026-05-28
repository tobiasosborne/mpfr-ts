/*
 * golden_driver.c -- Golden master for MPFR's mpfr_set_prec_raw.
 *
 * C: void mpfr_set_prec_raw (mpfr_ptr x, mpfr_prec_t p)
 *    Reset x's precision field to p WITHOUT reallocating and WITHOUT
 *    preserving the value. Only well-defined post-condition:
 *    mpfr_get_prec(x) == p. Ref: mpfr/src/set_prc_raw.c L25-L30.
 *
 * Wire: {"inputs":{"x":{<mpfr>},"prec":"<dec>"},"output":"<dec>"}
 *   output is the resulting precision (== p) as a decimal-string scalar
 *   via jl_output_scalar_u64 -> bigint on the TS side. The mantissa /
 *   exponent of x AFTER the call is indeterminate in C, so it is NOT
 *   part of the golden -- only the prec is asserted.
 *
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 *
 * Allocation-fit precondition: C asserts p <= alloc_limbs*64. We
 * construct x at orig_prec, compute the limb-rounded allocation bound
 * alloc_bits = ceil(orig_prec/64)*64, and choose every p in
 * [1, alloc_bits] so the C assertion always accepts.
 *
 * Ref: mpfr/src/set_prc_raw.c -- C reference.
 * Ref: mpfr/src/mpfr.h L483 -- public mpfr_set_prec_raw declaration.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_set_prec_raw golden_driver requires GMP_NUMB_BITS == 64"
#endif

extern void mpfr_set_prec_raw(mpfr_ptr, mpfr_prec_t);

/* Limb-rounded allocation bound, in bits, for an MPFR built at prec. */
static inline uint64_t alloc_bits_for(uint64_t prec) {
    return ((prec + 63ULL) / 64ULL) * 64ULL;
}

/* Build x at orig_prec, set it to value v, reset its prec to new_prec
 * (clamped into the allocation bound) and emit (x_before_reset, new_prec)
 * -> resulting prec.
 *
 * NOTE: x is serialised to the wire BEFORE the prec reset, so the input
 * MPFR is a well-formed value at orig_prec. After the reset the C value
 * is indeterminate; we read only mpfr_get_prec for the output. */
static inline void emit_case(FILE *out, const char *tag,
                             double v, uint64_t orig_prec, uint64_t new_prec) {
    const uint64_t bound = alloc_bits_for(orig_prec);
    if (new_prec < 1) new_prec = 1;
    if (new_prec > bound) new_prec = bound;

    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)orig_prec);
    mpfr_set_d(x, v, MPFR_RNDN);

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);   /* well-formed value at orig_prec */
    jl_kv_u64(out, 0, "prec", new_prec);
    jl_end_inputs(out);

    const uint64_t t0 = now_ns();
    mpfr_set_prec_raw(x, (mpfr_prec_t)new_prec);
    const uint64_t result_prec = (uint64_t)mpfr_get_prec(x);
    const uint64_t elapsed = now_ns() - t0;

    assert(result_prec == new_prec);  /* the sole C post-condition */
    jl_output_scalar_u64(out, result_prec);
    jl_finish(out, elapsed);
    mpfr_clear(x);
}

/* Like emit_case but x is a special value (zero / inf / nan). */
static inline void emit_special(FILE *out, const char *tag,
                                int which, uint64_t orig_prec, uint64_t new_prec) {
    const uint64_t bound = alloc_bits_for(orig_prec);
    if (new_prec < 1) new_prec = 1;
    if (new_prec > bound) new_prec = bound;

    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)orig_prec);
    switch (which) {
        case 0: mpfr_set_zero(x, 1); break;
        case 1: mpfr_set_zero(x, -1); break;
        case 2: mpfr_set_inf(x, 1); break;
        case 3: mpfr_set_inf(x, -1); break;
        case 4: mpfr_set_nan(x); break;   /* +NaN only */
        default: mpfr_set_zero(x, 1); break;
    }

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_u64(out, 0, "prec", new_prec);
    jl_end_inputs(out);

    const uint64_t t0 = now_ns();
    mpfr_set_prec_raw(x, (mpfr_prec_t)new_prec);
    const uint64_t result_prec = (uint64_t)mpfr_get_prec(x);
    const uint64_t elapsed = now_ns() - t0;

    assert(result_prec == new_prec);
    jl_output_scalar_u64(out, result_prec);
    jl_finish(out, elapsed);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 -- typical resets to a smaller-or-equal prec. */
    emit_case(out, "happy", 3.14, 53, 53);
    emit_case(out, "happy", 3.14, 53, 24);
    emit_case(out, "happy", 3.14, 53, 10);
    emit_case(out, "happy", 3.14, 100, 50);
    emit_case(out, "happy", 3.14, 100, 100);
    emit_case(out, "happy", 2.71, 64, 32);
    emit_case(out, "happy", 2.71, 64, 64);
    emit_case(out, "happy", 1.5, 200, 100);
    emit_case(out, "happy", 1.5, 200, 53);
    emit_case(out, "happy", 1.5, 200, 200);
    emit_case(out, "happy", -1.0, 128, 64);
    emit_case(out, "happy", -1.0, 128, 1);
    emit_case(out, "happy", 1e100, 256, 128);
    emit_case(out, "happy", 1e100, 256, 256);
    emit_case(out, "happy", 1e-100, 256, 53);
    emit_case(out, "happy", 42.0, 53, 2);
    emit_case(out, "happy", 0.5, 53, 53);
    emit_case(out, "happy", 100.0, 80, 40);
    emit_case(out, "happy", 7.0, 90, 30);
    emit_case(out, "happy", 6.022e23, 150, 75);

    /* edge: 30 -- prec=1 floor, raise to the allocation bound, specials. */
    emit_case(out, "edge", 3.14, 53, 1);          /* min prec */
    emit_case(out, "edge", 3.14, 64, 1);
    emit_case(out, "edge", 3.14, 100, 1);
    emit_case(out, "edge", 3.14, 53, 64);         /* raise to 1-limb bound (53->64) */
    emit_case(out, "edge", 3.14, 100, 128);       /* raise to 2-limb bound */
    emit_case(out, "edge", 3.14, 200, 256);       /* raise to 4-limb bound */
    emit_case(out, "edge", 3.14, 1, 1);           /* orig prec=1 */
    emit_case(out, "edge", 3.14, 1, 64);          /* 1->64 (1-limb alloc) */
    emit_case(out, "edge", 1.0, 65, 1);
    emit_case(out, "edge", 1.0, 65, 65);
    emit_case(out, "edge", 1.0, 65, 128);         /* 65 needs 2 limbs -> 128 bound */
    emit_case(out, "edge", 1.0, 127, 128);
    emit_case(out, "edge", 1.0, 128, 128);
    emit_case(out, "edge", 1.0, 129, 192);        /* 129 needs 3 limbs -> 192 bound */
    /* specials: zero / inf / nan -- prec reset is value-agnostic. */
    emit_special(out, "edge", 0, 53, 24);   /* +0 */
    emit_special(out, "edge", 1, 53, 24);   /* -0 */
    emit_special(out, "edge", 0, 100, 50);
    emit_special(out, "edge", 1, 100, 1);
    emit_special(out, "edge", 2, 53, 24);   /* +Inf */
    emit_special(out, "edge", 3, 53, 24);   /* -Inf */
    emit_special(out, "edge", 2, 128, 64);
    emit_special(out, "edge", 3, 200, 100);
    emit_special(out, "edge", 4, 53, 24);   /* +NaN */
    emit_special(out, "edge", 4, 100, 50);
    emit_special(out, "edge", 4, 64, 64);
    emit_special(out, "edge", 4, 200, 1);
    emit_special(out, "edge", 0, 53, 64);   /* +0 raise within alloc */
    emit_special(out, "edge", 2, 65, 128);  /* +Inf raise within alloc */
    emit_special(out, "edge", 4, 65, 128);  /* +NaN raise within alloc */
    emit_special(out, "edge", 1, 256, 256); /* -0 identity */

    /* adversarial: 12 -- prec equal to orig, off-by-one around limb
     * boundaries, and the largest p the allocation admits. */
    emit_case(out, "adversarial", 3.14, 64, 63);
    emit_case(out, "adversarial", 3.14, 64, 64);   /* exactly 1 limb */
    emit_case(out, "adversarial", 3.14, 65, 64);
    emit_case(out, "adversarial", 3.14, 65, 65);
    emit_case(out, "adversarial", 3.14, 63, 64);   /* 63 alloc-rounds to 64 */
    emit_case(out, "adversarial", 1.0, 1, 64);     /* prec 1 -> 64 (1-limb alloc) */
    emit_case(out, "adversarial", 1.0, 128, 127);
    emit_case(out, "adversarial", 1.0, 128, 128);
    emit_case(out, "adversarial", 1.0, 129, 192);  /* 3-limb bound */
    emit_case(out, "adversarial", 1.0, 192, 191);
    emit_special(out, "adversarial", 4, 64, 64);   /* +NaN exact-limb */
    emit_special(out, "adversarial", 0, 1, 64);    /* +0 raise to alloc bound */

    /* fuzz: 50 random (orig_prec, new_prec, value). new_prec clamped
     * into the allocation bound by emit_case. Unique 64-bit hex seed. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5EC0FFEEDEADBEEFULL);
        for (int rep = 0; rep < 50; ++rep) {
            const uint64_t orig_prec = 1 + (xs64_below(&rng, 300));   /* [1,300] */
            const uint64_t bound = alloc_bits_for(orig_prec);
            const uint64_t new_prec = 1 + (xs64_below(&rng, bound));  /* [1,bound] */
            /* value: random finite double from random bits. */
            uint64_t bits = xs64_next(&rng);
            const uint64_t e = (bits >> 52) & 0x7FF;
            if (e == 0x7FF) bits &= ~(0x7FFULL << 52);  /* avoid inf/nan double */
            double v;
            memcpy(&v, &bits, sizeof v);
            emit_case(out, "fuzz", v, orig_prec, new_prec);
        }
    }

    /* mined: 5 -- the documented use-pattern from MPFR internals, where
     * set_prec_raw temporarily shrinks then restores a scratch var's
     * prec without touching its allocation (e.g. the MPFR_TMP idiom). */
    emit_case(out, "mined", 1.0, 53, 53);    /* identity reset */
    emit_case(out, "mined", 1.0, 53, 1);     /* shrink to minimum */
    emit_case(out, "mined", 1.0, 64, 64);    /* exact-limb identity */
    emit_case(out, "mined", 1.0, 128, 64);   /* shrink across limb boundary */
    emit_case(out, "mined", 1.0, 64, 1);     /* shrink scratch to 1 bit */

    return 0;
}
