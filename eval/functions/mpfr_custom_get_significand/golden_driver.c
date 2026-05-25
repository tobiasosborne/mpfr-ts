/*
 * golden_driver.c -- Golden master for MPFR's mpfr_custom_get_significand.
 *
 * C: void *mpfr_custom_get_significand(mpfr_srcptr x);
 *   returns the limb-array pointer; TS port returns x.mant (bigint).
 * Ref: mpfr/src/stack_interface.c L39-L44.
 *
 * INPUT CARVE-OUT: only NORMAL values (C returns meaningless pointer
 * for singulars). The driver emits the mantissa as a single decimal
 * bigint by reading the MPFR value via mpfr_get_z_2exp (same trick
 * jl_kv_mpfr uses to produce the MSB-aligned mantissa).
 *
 * Wire: {"inputs":{"x":<mpfr>},"output":"<dec>"}.
 * Tag distribution (Rule 7): happy 20, edge 30, adv 12, fuzz 50, mined 5.
 */
#include "common.h"
#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_custom_get_significand golden_driver requires GMP_NUMB_BITS == 64"
#endif

extern void mpfr_setmin(mpfr_ptr, mpfr_exp_t);
extern void mpfr_setmax(mpfr_ptr, mpfr_exp_t);

/* Emit the MSB-aligned mantissa as a decimal string. Mirrors the trick
 * used in common.h's jl_kv_mpfr: mpfr_get_z_2exp depacks the mantissa
 * to exactly prec significant bits. */
static inline void emit_mantissa(FILE *out, mpfr_srcptr x) {
    mpz_t z;
    mpz_init(z);
    (void) mpfr_get_z_2exp(z, x);
    mpz_abs(z, z);
    char *s = mpz_get_str(NULL, 10, z);
    fputs(",\"output\":\"", out);
    fputs(s, out);
    fputc('"', out);
    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(s, strlen(s) + 1);
    mpz_clear(z);
}

static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    assert(mpfr_regular_p(x));
    const uint64_t t0 = now_ns();
    (void) mpfr_custom_get_significand(x);  /* call the function (output computed from x). */
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    emit_mantissa(out, x);
    jl_finish(out, elapsed);
}

static inline void emit_from_double(FILE *out, const char *tag, double d, mpfr_prec_t prec) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_d(x, d, MPFR_RNDN);
    if (mpfr_regular_p(x)) emit_case(out, tag, x);
    mpfr_clear(x);
}
static inline void emit_pow2(FILE *out, const char *tag, mpfr_prec_t prec, mpfr_exp_t e) {
    mpfr_t x; mpfr_init2(x, prec); mpfr_set_ui(x, 1, MPFR_RNDN); mpfr_set_exp(x, e);
    emit_case(out, tag, x); mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    /* happy: 20 */
    emit_from_double(out, "happy", 3.14, 53);
    emit_from_double(out, "happy", 2.71828, 53);
    emit_from_double(out, "happy", 1.0, 53);
    emit_from_double(out, "happy", -3.14, 53);
    emit_from_double(out, "happy", 100.5, 53);
    emit_from_double(out, "happy", -100.5, 53);
    emit_from_double(out, "happy", 0.5, 53);
    emit_from_double(out, "happy", 0.25, 53);
    emit_from_double(out, "happy", 0.125, 53);
    emit_from_double(out, "happy", 1.5, 24);
    emit_from_double(out, "happy", 1.5, 64);
    emit_from_double(out, "happy", 1.5, 128);
    emit_from_double(out, "happy", 1.5, 200);
    emit_pow2(out, "happy", 53, 0);
    emit_pow2(out, "happy", 53, 5);
    emit_pow2(out, "happy", 53, -5);
    emit_pow2(out, "happy", 64, 10);
    emit_pow2(out, "happy", 128, 100);
    emit_from_double(out, "happy", 1234567.89, 53);
    emit_from_double(out, "happy", -1234567.89, 53);
    /* edge: 30 -- precs spanning limb boundaries, sign variations. */
    emit_from_double(out, "edge", 1.5, 1);
    emit_from_double(out, "edge", -1.5, 1);
    emit_pow2(out, "edge", 1, 0);
    emit_pow2(out, "edge", 1, 1);
    emit_pow2(out, "edge", 1, -1);
    emit_pow2(out, "edge", 1, 100);
    emit_from_double(out, "edge", 3.14, 2);
    emit_from_double(out, "edge", 3.14, 63);
    emit_from_double(out, "edge", 3.14, 64);
    emit_from_double(out, "edge", 3.14, 65);
    emit_from_double(out, "edge", 3.14, 127);
    emit_from_double(out, "edge", 3.14, 128);
    emit_from_double(out, "edge", 3.14, 129);
    emit_from_double(out, "edge", 3.14, 256);
    emit_from_double(out, "edge", 3.14, 512);
    emit_pow2(out, "edge", 24, 0);
    emit_pow2(out, "edge", 24, 50);
    emit_pow2(out, "edge", 24, -50);
    emit_pow2(out, "edge", 200, 0);
    emit_pow2(out, "edge", 200, 100);
    emit_pow2(out, "edge", 256, 0);
    emit_pow2(out, "edge", 512, 100);
    emit_pow2(out, "edge", 64, mpfr_get_emax() - 1);
    emit_pow2(out, "edge", 64, mpfr_get_emin() + 1);
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_setmax(x, mpfr_get_emax()); emit_case(out, "edge", x); mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_setmin(x, mpfr_get_emin()); emit_case(out, "edge", x); mpfr_clear(x);
    }
    emit_from_double(out, "edge", 1e10, 53);
    emit_from_double(out, "edge", 1e-10, 53);
    emit_from_double(out, "edge", 7.0, 3);
    emit_from_double(out, "edge", 0.7, 4);
    /* adversarial: 12 -- all-ones mantissa, various corner shapes. */
    {
        mpfr_t x; mpfr_init2(x, 53); mpfr_setmax(x, 10); emit_case(out, "adversarial", x); mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 24); mpfr_setmax(x, 5); emit_case(out, "adversarial", x); mpfr_clear(x);
    }
    {
        mpfr_t x; mpfr_init2(x, 128); mpfr_setmax(x, 100); emit_case(out, "adversarial", x); mpfr_clear(x);
    }
    emit_pow2(out, "adversarial", 53, mpfr_get_emax());
    emit_pow2(out, "adversarial", 53, mpfr_get_emin());
    emit_from_double(out, "adversarial", 1.0/3.0, 53);
    emit_from_double(out, "adversarial", 2.0/3.0, 53);
    emit_from_double(out, "adversarial", -1.0/3.0, 64);
    emit_from_double(out, "adversarial", 0.1, 200);
    emit_from_double(out, "adversarial", 0.2, 200);
    emit_pow2(out, "adversarial", 1000, 0);
    emit_pow2(out, "adversarial", 1000, 100);
    /* fuzz: 50 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x110011EE22EEBB00ULL);
        const mpfr_prec_t precs[6] = {1, 24, 53, 64, 128, 256};
        int emitted = 0;
        while (emitted < 50) {
            const mpfr_prec_t p = precs[xs64_below(&rng, 6)];
            const uint64_t hi = xs64_next(&rng);
            const uint64_t lo = xs64_next(&rng);
            const double d = (double)hi / 4294967296.0 + 0.5;  /* in [0.5, 1.5) */
            const int neg = (lo & 1) ? +1 : -1;
            mpfr_t x;
            mpfr_init2(x, p);
            mpfr_set_d(x, neg * d, MPFR_RNDN);
            if (!mpfr_regular_p(x)) { mpfr_clear(x); continue; }
            emit_case(out, "fuzz", x);
            mpfr_clear(x);
            emitted++;
        }
    }
    /* mined: 5 */
    emit_from_double(out, "mined", 1.0, 53);
    emit_from_double(out, "mined", 17.0, 53);
    emit_from_double(out, "mined", -1.0, 100);
    emit_pow2(out, "mined", 64, 50);
    emit_pow2(out, "mined", 53, 0);
    return 0;
}
