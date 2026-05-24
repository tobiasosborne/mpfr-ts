/*
 * golden_driver.c — Golden master for MPFR's mpfr_get_exp.
 *
 * C signature
 * -----------
 *
 *   mpfr_exp_t mpfr_get_exp(mpfr_srcptr x);
 *
 * Returns MPFR_EXP(x). Asserts MPFR_IS_PURE_FP(x) — i.e. aborts on
 * non-normal kinds (zero/inf/nan). Ref: mpfr/src/get_exp.c L24-L30.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS port throws MPFRError('EDOMAIN') for non-normal x; for normal x
 * returns x.exp as a bigint. Since the TS port throws on non-normal
 * inputs (and a throw is counted as a non-pass in the harness), the
 * golden emits ONLY normal-kind inputs — that's the only path with a
 * defined expected output to compare against.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-normal>},
 *    "output":"<decimal exp>",
 *    "time_ns":<n>}
 *
 *   - input x is a normal MPFR (the only kind the function defines).
 *   - output is a decimal-string bigint via jl_output_scalar_i64 (the
 *     exp is potentially negative — could not use scalar_u64).
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  22
 *   edge         :  30
 *   adversarial  :  10
 *   fuzz         :  55
 *   mined        :   5
 *
 * Ref: mpfr/src/get_exp.c — C reference.
 * Ref: src/ops/get_exp.ts — production port (to be written by sonnet).
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_get_exp golden_driver requires GMP_NUMB_BITS == 64"
#endif

static void emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    assert(mpfr_regular_p(x));  /* only normal inputs */
    const uint64_t t0 = now_ns();
    const mpfr_exp_t e = mpfr_get_exp(x);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    jl_output_scalar_i64(out, (int64_t)e);
    jl_finish(out, elapsed);
}

static void mk_norm_d(mpfr_ptr x, mpfr_prec_t prec, double d) {
    mpfr_init2(x, prec);
    mpfr_set_d(x, d, MPFR_RNDN);
}

static void mk_norm_si(mpfr_ptr x, mpfr_prec_t prec, long v) {
    mpfr_init2(x, prec);
    mpfr_set_si(x, v, MPFR_RNDN);
}

static void mk_norm_exact(mpfr_ptr x, mpfr_prec_t prec,
                          const char *mant_dec, mpfr_exp_t exp_ts, int sign) {
    mpz_t z;
    mpz_init(z);
    if (mpz_set_str(z, mant_dec, 10) != 0) {
        fprintf(stderr, "mk_norm_exact: bad decimal\n");
        exit(2);
    }
    if (sign < 0) mpz_neg(z, z);
    mpfr_init2(x, prec);
    mpfr_set_z_2exp(x, z, exp_ts - (mpfr_exp_t)prec, MPFR_RNDN);
    mpz_clear(z);
    assert(mpfr_regular_p(x));
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 — common values, varied precs/exps. */
    {
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "happy", x); mpfr_clear(x); }       /* exp=1 */
        { mpfr_t x; mk_norm_d(x, 53, 2.0); emit_case(out, "happy", x); mpfr_clear(x); }       /* exp=2 */
        { mpfr_t x; mk_norm_d(x, 53, 0.5); emit_case(out, "happy", x); mpfr_clear(x); }       /* exp=0 */
        { mpfr_t x; mk_norm_d(x, 53, 4.0); emit_case(out, "happy", x); mpfr_clear(x); }       /* exp=3 */
        { mpfr_t x; mk_norm_d(x, 53, 8.0); emit_case(out, "happy", x); mpfr_clear(x); }       /* exp=4 */
        { mpfr_t x; mk_norm_d(x, 53, -1.0); emit_case(out, "happy", x); mpfr_clear(x); }      /* exp=1 (sign doesn't affect exp) */
        { mpfr_t x; mk_norm_d(x, 53, -2.0); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 3.14); emit_case(out, "happy", x); mpfr_clear(x); }      /* exp=2 */
        { mpfr_t x; mk_norm_d(x, 53, 2.71); emit_case(out, "happy", x); mpfr_clear(x); }      /* exp=2 */
        { mpfr_t x; mk_norm_d(x, 53, 100.0); emit_case(out, "happy", x); mpfr_clear(x); }     /* exp=7 */
        { mpfr_t x; mk_norm_d(x, 53, 1000.0); emit_case(out, "happy", x); mpfr_clear(x); }    /* exp=10 */
        { mpfr_t x; mk_norm_d(x, 24, 1.5); emit_case(out, "happy", x); mpfr_clear(x); }       /* exp=1 */
        { mpfr_t x; mk_norm_d(x, 113, 1.5); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_si(x, 53, 7); emit_case(out, "happy", x); mpfr_clear(x); }        /* exp=3 */
        { mpfr_t x; mk_norm_si(x, 53, -7); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_si(x, 53, 100); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_si(x, 53, 1000); emit_case(out, "happy", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 80, 1.0/3.0); emit_case(out, "happy", x); mpfr_clear(x); }   /* exp=0 (~0.333, [.25,.5)) */
        { mpfr_t x; mk_norm_d(x, 100, 0.25); emit_case(out, "happy", x); mpfr_clear(x); }     /* exp=-1 */
        { mpfr_t x; mk_norm_d(x, 100, 0.125); emit_case(out, "happy", x); mpfr_clear(x); }    /* exp=-2 */
        { mpfr_t x; mk_norm_d(x, 64, 1e10); emit_case(out, "happy", x); mpfr_clear(x); }      /* exp~34 */
        { mpfr_t x; mk_norm_d(x, 64, 1e-10); emit_case(out, "happy", x); mpfr_clear(x); }     /* exp~-33 */
    }

    /* edge: 30 — extreme exps, PREC_MIN, exact powers of 2. */
    {
        { mpfr_t x; mk_norm_d(x, 53, 1e100); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1e-100); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1e300); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1e-300); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, -1e300); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, -1e-300); emit_case(out, "edge", x); mpfr_clear(x); }
        /* PREC_MIN. */
        { mpfr_t x; mk_norm_si(x, 1, 1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_si(x, 1, -1); emit_case(out, "edge", x); mpfr_clear(x); }
        /* prec=64 — limb boundary. */
        { mpfr_t x; mk_norm_d(x, 64, 1.0); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 64, 0.5); emit_case(out, "edge", x); mpfr_clear(x); }
        /* prec=65 — just past limb boundary. */
        { mpfr_t x; mk_norm_d(x, 65, 1.0); emit_case(out, "edge", x); mpfr_clear(x); }
        /* Exact powers of 2, both signs. */
        { mpfr_t x; mk_norm_exact(x, 53, "4503599627370496", 1, +1); emit_case(out, "edge", x); mpfr_clear(x); }   /* 2^52 mantissa, exp=1 → value 0.5 */
        { mpfr_t x; mk_norm_exact(x, 53, "4503599627370496", 1, -1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_exact(x, 53, "4503599627370496", 50, +1); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_exact(x, 53, "4503599627370496", -50, +1); emit_case(out, "edge", x); mpfr_clear(x); }
        /* Large prec. */
        { mpfr_t x; mk_norm_d(x, 1024, 1.5); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 2048, 1.5); emit_case(out, "edge", x); mpfr_clear(x); }
        /* Very large positive exp. */
        { mpfr_t x; mk_norm_exact(x, 53, "4503599627370496", 1000000, +1); emit_case(out, "edge", x); mpfr_clear(x); }
        /* Very large negative exp. */
        { mpfr_t x; mk_norm_exact(x, 53, "4503599627370496", -1000000, -1); emit_case(out, "edge", x); mpfr_clear(x); }
        /* Limb-boundary precs. */
        { mpfr_t x; mk_norm_d(x, 63, 1.5); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 127, 1.5); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 128, 1.5); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 129, 1.5); emit_case(out, "edge", x); mpfr_clear(x); }
        /* exp = 0 boundary. */
        { mpfr_t x; mk_norm_exact(x, 53, "4503599627370496", 0, +1); emit_case(out, "edge", x); mpfr_clear(x); }   /* value ~ 0.25, exp=0 */
        /* exp = 1 boundary (smallest positive exp). */
        { mpfr_t x; mk_norm_exact(x, 4, "8", 1, +1); emit_case(out, "edge", x); mpfr_clear(x); }   /* mant=8, prec=4, value=0.5 */
        /* exp = -1. */
        { mpfr_t x; mk_norm_exact(x, 4, "8", -1, +1); emit_case(out, "edge", x); mpfr_clear(x); }
        /* Tiny value. */
        { mpfr_t x; mk_norm_d(x, 53, 1e-200); emit_case(out, "edge", x); mpfr_clear(x); }
        /* Large value. */
        { mpfr_t x; mk_norm_d(x, 53, 1e200); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 200, 1.5); emit_case(out, "edge", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 4096, 1.5); emit_case(out, "edge", x); mpfr_clear(x); }
    }

    /* adversarial: 10 — exact-mantissa values at exp boundaries. */
    {
        { mpfr_t x; mk_norm_exact(x, 53, "4503599627370496", 100, +1); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_exact(x, 53, "4503599627370496", -100, -1); emit_case(out, "adversarial", x); mpfr_clear(x); }
        /* Largest representable mantissa at small prec. */
        { mpfr_t x; mk_norm_exact(x, 4, "15", 0, +1); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_exact(x, 4, "15", 1000, +1); emit_case(out, "adversarial", x); mpfr_clear(x); }
        /* Carry-on-rounding territory. */
        { mpfr_t x; mk_norm_d(x, 53, 0.9999999999999999); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 1.0000000000000002); emit_case(out, "adversarial", x); mpfr_clear(x); }
        /* Subnormal-ish IEEE doubles (still normal in MPFR). */
        { mpfr_t x; mk_norm_d(x, 53, 5e-324); emit_case(out, "adversarial", x); mpfr_clear(x); }
        /* Largest IEEE double. */
        { mpfr_t x; mk_norm_d(x, 53, 1.7976931348623157e308); emit_case(out, "adversarial", x); mpfr_clear(x); }
        /* Sign-only flip — exp unaffected. */
        { mpfr_t x; mk_norm_d(x, 53, -1.5); emit_case(out, "adversarial", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, -2.5); emit_case(out, "adversarial", x); mpfr_clear(x); }
    }

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x6E7E73AD471FACE5ULL);
        int emitted = 0;
        while (emitted < 55) {
            const uint64_t prec = 1 + xs64_below(&rng, 200);
            const double mag = (double)(1 + xs64_below(&rng, 1000000));
            const int sign = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            mpfr_t x;
            mpfr_init2(x, (mpfr_prec_t)prec);
            mpfr_set_d(x, sign * mag, MPFR_RNDN);
            if (mpfr_regular_p(x)) {
                emit_case(out, "fuzz", x);
                emitted++;
            }
            mpfr_clear(x);
        }
    }

    /* mined: 5 — patterns from mpfr/tests/texp.c, tdiv.c using get_exp. */
    {
        { mpfr_t x; mk_norm_d(x, 53, 1.0); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 2.0); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 0.5); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 53, 3.14); emit_case(out, "mined", x); mpfr_clear(x); }
        { mpfr_t x; mk_norm_d(x, 113, 1.0/3.0); emit_case(out, "mined", x); mpfr_clear(x); }
    }

    return 0;
}
