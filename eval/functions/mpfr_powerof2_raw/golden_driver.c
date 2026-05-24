/*
 * golden_driver.c — Golden master for MPFR's mpfr_powerof2_raw.
 *
 * mpfr_powerof2_raw(x) returns 1 iff |x| is exactly a power of 2 — i.e.
 * its mantissa has only the MSB set. C-side rationale at
 * mpfr/src/powerof2.c L30-L38; here we drive the full kind matrix
 * (normal/zero/inf/nan) so the wrapper's behaviour is locked in.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR>},
 *    "output":<bool>,
 *    "time_ns":<n>}
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  20
 *   edge         :  30
 *   adversarial  :  10
 *   fuzz         :  55
 *   mined        :   5
 *
 * Ref: mpfr/src/powerof2.c L30-L38 — C reference.
 * Ref: src/internal/mpfr/powerof2_raw.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_powerof2_raw golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Forward decl: mpfr_powerof2_raw lives in the internal header, not in
 * the public mpfr.h. Linked from libmpfr. */
extern int mpfr_powerof2_raw(mpfr_srcptr);

static inline void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    /* For non-normal kinds, the C contract is "undefined" — mpfr_powerof2_raw
     * reads MPFR_MANT(x) which is uninitialised garbage for zero/inf/nan.
     * The TS port's well-defined behaviour (returns false for non-normal
     * kinds) is what we test against, so we synthesise false output for
     * non-normal cases rather than trust the C side's heap state. */
    const int is_normal = mpfr_regular_p(x);
    const uint64_t t0 = now_ns();
    const int raw = is_normal ? mpfr_powerof2_raw(x) : 0;
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    jl_output_scalar_bool(out, raw);
    jl_finish(out, elapsed);
}

/* Build a normal value equal to ±2^e at precision p (MSB-only mantissa). */
static void mk_pow2(mpfr_ptr x, mpfr_prec_t prec, mpfr_exp_t e, int sign) {
    mpfr_init2(x, prec);
    mpfr_set_si_2exp(x, sign, e, MPFR_RNDN);
    assert(mpfr_regular_p(x));
}

/* Build a normal value with an exact mantissa pattern. */
static void mk_norm_exact(mpfr_ptr b, mpfr_prec_t prec,
                          const char *mant_dec, mpfr_exp_t exp_ts, int sign) {
    mpz_t z;
    mpz_init(z);
    if (mpz_set_str(z, mant_dec, 10) != 0) {
        fprintf(stderr, "mk_norm_exact: bad decimal\n");
        exit(2);
    }
    if (sign < 0) mpz_neg(z, z);
    mpfr_init2(b, prec);
    mpfr_set_z_2exp(b, z, exp_ts - (mpfr_exp_t)prec, MPFR_RNDN);
    mpz_clear(z);
    assert(mpfr_regular_p(b));
}

int main(void) {
    FILE *out = stdout;

    /* happy: 20 */
    {
        /* Powers of 2 at common precisions, both signs. */
        { mpfr_t x; mk_pow2(x, 53, 0, 1); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 53, 0, -1); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 53, 1, 1); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 53, 10, 1); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 53, -10, 1); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 64, 0, 1); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 128, 0, 1); emit_case(out, "happy", x); mpfr_clear(x); }

        /* Non-powers-of-2. */
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 3.0, MPFR_RNDN); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, -3.0, MPFR_RNDN); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 5.0, MPFR_RNDN); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 3.14, MPFR_RNDN); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 6.0, MPFR_RNDN); emit_case(out, "happy", x); mpfr_clear(x); }

        /* Zeros and infinities (false). */
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, 1); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, 1); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, -1); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x); emit_case(out, "happy", x); mpfr_clear(x); }

        /* Larger powers of 2. */
        { mpfr_t x; mk_pow2(x, 113, 100, 1); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 200, -50, -1); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 24, 5, 1); emit_case(out, "happy", x); mpfr_clear(x); }
    }

    /* edge: 30 */
    {
        /* Boundary precs: 1, 2, 63, 64, 65, 127, 128, 129. */
        { mpfr_t x; mk_pow2(x, 1, 0, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 1, 0, -1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 2, 0, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 63, 0, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 64, 0, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 65, 0, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 127, 0, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 128, 0, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 129, 0, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 256, 0, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 512, 0, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 1024, 0, 1); emit_case(out, "edge", x); mpfr_clear(x); }

        /* prec=2: only true value is mant=10b (i.e. 2 = 2^1, MSB-only).
         * Other normal values like mant=11b (=3) → false. */
        { mpfr_t x; mk_norm_exact(x, 2, "3", 2, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_exact(x, 3, "5", 3, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_exact(x, 3, "6", 3, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_exact(x, 3, "7", 3, 1); emit_case(out, "edge", x); mpfr_clear(x); }

        /* Very large/small exponents at prec=53. */
        { mpfr_t x; mk_pow2(x, 53, 1000000, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 53, -1000000, 1); emit_case(out, "edge", x); mpfr_clear(x); }

        /* Zero/Inf/NaN at many precs. */
        { mpfr_t x; mpfr_init2(x, 1); mpfr_set_zero(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 1); mpfr_set_inf(x, -1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 1024); mpfr_set_zero(x, -1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 1024); mpfr_set_inf(x, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 1024); mpfr_set_nan(x); emit_case(out, "edge", x); mpfr_clear(x); }

        /* MSB plus lowest bit set (false). */
        { mpfr_t x; mk_norm_exact(x, 53, "4503599627370497", 53, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        /* MSB plus bit just below (false). */
        { mpfr_t x; mk_norm_exact(x, 53, "6755399441055744", 53, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        /* All-ones mantissa (false). */
        { mpfr_t x; mk_norm_exact(x, 53, "9007199254740991", 53, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        /* MSB-only at limb boundary, negative. */
        { mpfr_t x; mk_pow2(x, 64, -100, -1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 128, -100, -1); emit_case(out, "edge", x); mpfr_clear(x); }
        /* Non-power at multi-limb prec. */
        { mpfr_t x; mk_norm_exact(x, 128, "170141183460469231731687303715884105729", 128, 1);
          emit_case(out, "edge", x); mpfr_clear(x); }
        /* prec=192 MSB-only (3 limbs). */
        { mpfr_t x; mk_pow2(x, 192, 0, 1); emit_case(out, "edge", x); mpfr_clear(x); }
    }

    /* adversarial: 10 */
    {
        /* Power of 2 negative at the largest representable exponents. */
        { mpfr_t x; mk_pow2(x, 53, 1073741822, 1); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 53, -1073741822, -1); emit_case(out, "adversarial", x); mpfr_clear(x); }
        /* MSB-only at limb-boundary precs. */
        { mpfr_t x; mk_pow2(x, 64, 0, 1); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 128, 0, -1); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 192, 0, 1); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; mk_pow2(x, 256, 0, 1); emit_case(out, "adversarial", x); mpfr_clear(x); }
        /* MSB + bit at position 0 (most-distant lower-bit set). */
        { mpfr_t x; mk_norm_exact(x, 128, "170141183460469231731687303715884105729", 128, 1);
          emit_case(out, "adversarial", x); mpfr_clear(x); }
        /* MSB + bit at position prec-2. */
        { mpfr_t x; mk_norm_exact(x, 128, "255211775190703847597530955573826158592", 128, 1);
          emit_case(out, "adversarial", x); mpfr_clear(x); }
        /* All-ones mantissa at large prec. */
        { mpfr_t x; mk_norm_exact(x, 128, "340282366920938463463374607431768211455", 128, 1);
          emit_case(out, "adversarial", x); mpfr_clear(x); }
        /* prec=1, mant=1 (always true). */
        { mpfr_t x; mpfr_init2(x, 1); mpfr_set_si(x, 1, MPFR_RNDN); emit_case(out, "adversarial", x); mpfr_clear(x); }
    }

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5B1F5002F0AAFFADULL);
        int emitted = 0;
        while (emitted < 55) {
            const uint64_t kind_sel = xs64_below(&rng, 10);
            const uint64_t prec_raw = 1 + xs64_below(&rng, 256);
            const int sign = (xs64_below(&rng, 2) == 0) ? 1 : -1;
            mpfr_t x;
            mpfr_init2(x, (mpfr_prec_t)prec_raw);
            if (kind_sel == 8) {
                mpfr_set_zero(x, sign);
            } else if (kind_sel == 9) {
                mpfr_set_inf(x, sign);
            } else if (kind_sel < 3) {
                /* MSB-only (true): set_si_2exp with magnitude 1. */
                const int64_t e = (int64_t)xs64_below(&rng, 1000) - 500;
                mpfr_set_si_2exp(x, sign, (mpfr_exp_t)e, MPFR_RNDN);
            } else {
                /* Random normal mag — almost certainly not a power of 2. */
                const uint64_t mag = 2 + xs64_below(&rng, 1000000);
                mpfr_set_si(x, sign * (long)mag, MPFR_RNDN);
                if (!mpfr_regular_p(x)) {
                    mpfr_clear(x);
                    continue;
                }
            }
            emit_case(out, "fuzz", x);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* mined: 5 */
    {
        { mpfr_t x; mk_pow2(x, 53, 0, 1); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 1.5, MPFR_RNDN); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, 1); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, 1); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x); emit_case(out, "mined", x); mpfr_clear(x); }
    }

    return 0;
}
