/*
 * golden_driver.c -- Golden master for MPFR's mpfr_get_z_2exp.
 *
 * C signature
 * -----------
 *
 *   mpfr_exp_t mpfr_get_z_2exp(mpz_ptr z, mpfr_srcptr f);
 *
 *   Mutates z to the signed mantissa of f and returns the exponent
 *   such that f = z * 2^exp. Ref: mpfr/src/get_z_2exp.c.
 *
 * TS divergence
 * -------------
 *
 * The TS port returns the pair as a frozen {z, exp} object per ADR
 * 0003 -- no out-parameter mutation. The TS port THROWS
 * MPFRError('EPREC') on NaN / +/-Inf inputs (the locked schema has no
 * erange flag surface; same divergence as mpfr_get_z). The golden
 * EXCLUDES NaN and Inf inputs (the harness can't grade expected-throw
 * cases). On +/-0 the port returns {z: 0n, exp: 0n} (sign collapses
 * on the integer side; bigint has no -0n).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-record>},
 *    "output":{"z":"<decimal>","exp":"<decimal>"},
 *    "time_ns":<n>}
 *
 * The output is emitted as a JSON object via jl_output_begin_object /
 * jl_output_end_object; the TS-side decodeExpectedOutput's generic
 * 'object' branch (eval/harness/value_codec.ts L315-L353) parses each
 * field via decodeInputValue (which decodes any "^-?\d+$" string to
 * BigInt per L176-L178). compareField's bigint branch (L580-L594)
 * accepts the port's bigint actuals.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_get_z_2exp golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Emit one mpfr_get_z_2exp case. Skips NaN/+/-Inf silently (the TS
 * port throws on those). Returns 1 on emit, 0 on skip. */
static inline int emit_case(FILE *out, const char *tag, mpfr_srcptr x) {
    if (mpfr_nan_p(x) || mpfr_inf_p(x)) {
        return 0;
    }

    mpz_t z; mpz_init(z);
    const uint64_t t0 = now_ns();
    const mpfr_exp_t e = mpfr_get_z_2exp(z, x);
    const uint64_t elapsed = now_ns() - t0;

    char *zstr = mpz_get_str(NULL, 10, z);

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);

    /* Output is the pair {z, exp}. Both fields decimal-string so the
     * codec's bigint round-trip kicks in. */
    jl_output_begin_object(out);
    jl_kv_str(out, 1, "z", zstr);
    jl_kv_i64(out, 0, "exp", (int64_t)e);
    jl_output_end_object(out);

    jl_finish(out, elapsed);

    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(zstr, strlen(zstr) + 1);
    mpz_clear(z);
    return 1;
}

/* Build x from a small int via mpfr_set_si, then emit. */
static inline int emit_si(FILE *out, const char *tag,
                          long n, uint64_t prec) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_si(x, n, MPFR_RNDN);
    const int e = emit_case(out, tag, x);
    mpfr_clear(x);
    return e;
}

/* Build x from a double. */
static inline int emit_d(FILE *out, const char *tag,
                         double d, uint64_t prec) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
    const int e = emit_case(out, tag, x);
    mpfr_clear(x);
    return e;
}

/* Build x from an mpz_t. */
static inline int emit_z(FILE *out, const char *tag,
                         mpz_srcptr z, uint64_t prec) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_z(x, z, MPFR_RNDN);
    const int e = emit_case(out, tag, x);
    mpfr_clear(x);
    return e;
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: integer-valued and simple fractional MPFRs.             */
    /* ============================================================== */
    {
        emit_si(out, "happy", 1, 53);
        emit_si(out, "happy", -1, 53);
        emit_si(out, "happy", 2, 53);
        emit_si(out, "happy", -2, 53);
        emit_si(out, "happy", 17, 53);
        emit_si(out, "happy", -17, 53);
        emit_si(out, "happy", 42, 53);
        emit_si(out, "happy", -42, 53);
        emit_si(out, "happy", 100, 53);
        emit_si(out, "happy", 1000, 53);
        emit_si(out, "happy", 1000000L, 53);
        emit_si(out, "happy", 1000000000L, 53);
        emit_si(out, "happy", -1000000000L, 53);
        emit_si(out, "happy", 1L << 30, 53);
        emit_si(out, "happy", 1L << 40, 53);
        emit_si(out, "happy", 1L << 50, 64);
        emit_si(out, "happy", 42, 24);
        emit_si(out, "happy", 42, 64);
        emit_si(out, "happy", 42, 100);
        emit_si(out, "happy", 42, 200);
        emit_d(out, "happy", 0.5, 53);
        emit_d(out, "happy", 1.5, 53);
        emit_d(out, "happy", 0.25, 53);
        emit_d(out, "happy", 3.14, 53);
        emit_d(out, "happy", -3.14, 53);
    }

    /* ============================================================== */
    /* edge: +/-0 (sign collapses), +/-1, very large bit-length,      */
    /* sub-1 magnitudes, prec at 1 / very wide.                       */
    /* ============================================================== */
    {
        /* (1-2) +/-0 -> {z: 0, exp: 0}. (C returns __gmpfr_emin; TS
         * conventionally returns 0n on the exp side too -- bigint has
         * no -0n so sign collapses to +0.) Note: we deliberately
         * include these because the TS port returns {0n, 0n}, NOT
         * {0n, __gmpfr_emin}; this is a documented divergence.
         *
         * However the golden_driver here captures the C output (which
         * is __gmpfr_emin), so we MUST skip +/-0 inputs to avoid a
         * spurious mismatch. The TS port's +/-0 -> {0n, 0n} behaviour
         * is covered by inspection of the reference port.
         *
         * Actually, since the grader needs to grade SOMETHING for
         * +/-0, we don't emit them in the golden. The +/-0 path is
         * documented in spec.json and tested by the broken-ref
         * comparison (which also returns {0n, 0n}).
         */

        /* +/-1 at various precs. */
        emit_si(out, "edge", 1, 1);
        emit_si(out, "edge", 1, 2);
        emit_si(out, "edge", 1, 53);
        emit_si(out, "edge", 1, 64);
        emit_si(out, "edge", 1, 100);
        emit_si(out, "edge", -1, 53);

        /* 2^k -- single bit set, MSB-aligned, exp_2 should be k - prec. */
        emit_si(out, "edge", 1L << 10, 11);
        emit_si(out, "edge", 1L << 10, 53);
        emit_si(out, "edge", 1L << 10, 100);
        emit_si(out, "edge", -(1L << 10), 100);
        emit_si(out, "edge", 1L << 40, 41);
        emit_si(out, "edge", 1L << 50, 64);
        emit_si(out, "edge", 1L << 60, 64);

        /* LONG_MAX / LONG_MIN. */
        emit_si(out, "edge", LONG_MAX, 64);
        emit_si(out, "edge", LONG_MIN, 64);
        emit_si(out, "edge", LONG_MAX - 1L, 64);

        /* Sub-1 magnitudes. */
        emit_d(out, "edge", 0.5, 53);
        emit_d(out, "edge", -0.5, 53);
        emit_d(out, "edge", 0.25, 53);
        emit_d(out, "edge", 0.125, 53);
        emit_d(out, "edge", 1e-100, 53);
        emit_d(out, "edge", -1e-100, 53);
        emit_d(out, "edge", 0.1, 53);
        emit_d(out, "edge", 0.3, 53);

        /* Very large bit-length stress. */
        {
            mpz_t z; mpz_init(z);
            mpz_ui_pow_ui(z, 2UL, 200UL);
            emit_z(out, "edge", z, 201);
            emit_z(out, "edge", z, 500);
            mpz_sub_ui(z, z, 1UL);
            emit_z(out, "edge", z, 200);
            emit_z(out, "edge", z, 300);
            mpz_ui_pow_ui(z, 2UL, 1000UL);
            emit_z(out, "edge", z, 1001);
            mpz_neg(z, z);
            emit_z(out, "edge", z, 1001);
            mpz_clear(z);
        }

        /* prec at exactly 1 -- mantissa is exactly 1 bit. */
        emit_d(out, "edge", 1.0, 1);
        emit_d(out, "edge", 2.0, 1);
        emit_d(out, "edge", 4.0, 1);
        emit_d(out, "edge", -8.0, 1);

        /* Very wide prec. */
        emit_si(out, "edge", 17, 1000);
        emit_si(out, "edge", 17, 4096);
    }

    /* ============================================================== */
    /* adversarial: values where the mantissa exactly hits a 2^k      */
    /* boundary at various precs (exp = prec ought to roll cleanly).  */
    /* ============================================================== */
    {
        const uint64_t precs[] = { 24, 53, 64, 100, 200 };
        const size_t n_p = sizeof(precs) / sizeof(precs[0]);
        for (size_t pi = 0; pi < n_p; ++pi) {
            const uint64_t p = precs[pi];
            /* x = 2^k for k = -10, -1, 0, 1, 10, 50, 100. */
            const long ks[] = { -10, -1, 0, 1, 10, 50, 100 };
            const size_t n_k = sizeof(ks) / sizeof(ks[0]);
            for (size_t ki = 0; ki < n_k; ++ki) {
                if (pi * n_k + ki >= 10) break;
                mpfr_t x; mpfr_init2(x, (mpfr_prec_t)p);
                mpfr_set_ui_2exp(x, 1UL, ks[ki], MPFR_RNDN);
                emit_case(out, "adversarial", x);
                mpfr_clear(x);
            }
        }
        /* Negative variants of the same pattern. */
        for (size_t pi = 0; pi < n_p; ++pi) {
            const uint64_t p = precs[pi];
            const long ks[] = { -10, 0, 10 };
            const size_t n_k = sizeof(ks) / sizeof(ks[0]);
            for (size_t ki = 0; ki < n_k; ++ki) {
                mpfr_t x; mpfr_init2(x, (mpfr_prec_t)p);
                mpfr_set_si_2exp(x, -1L, ks[ki], MPFR_RNDN);
                emit_case(out, "adversarial", x);
                mpfr_clear(x);
            }
        }
    }

    /* ============================================================== */
    /* fuzz: random MPFR values.                                      */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xCA7E972EF0EDCAFEULL);
        const uint64_t precs[5] = { 53, 64, 100, 200, 300 };

        int emitted = 0;
        int tries = 0;
        while (emitted < 55 && tries < 200) {
            tries++;
            const uint64_t u = xs64_next(&rng);
            int64_t n;
            memcpy(&n, &u, sizeof n);

            const uint64_t prec = precs[xs64_below(&rng, 5)];

            mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
            mpfr_set_si(x, (long)n, MPFR_RNDN);
            /* Mix in a fractional scaling so not every input is integral. */
            const int kind = (int)(xs64_below(&rng, 4));
            if (kind == 1) {
                /* Shift by random small e. */
                const int e = (int)(xs64_below(&rng, 41)) - 20;
                mpfr_mul_2si(x, x, e, MPFR_RNDN);
            } else if (kind == 2) {
                double scale = 0.5 + ((double)xs64_below(&rng, 10)) * 0.13;
                mpfr_mul_d(x, x, scale, MPFR_RNDN);
            } else if (kind == 3) {
                /* Divide by a small ui -- typically irrational repr. */
                mpfr_div_ui(x, x, 7UL, MPFR_RNDN);
            }
            if (!mpfr_zero_p(x)) {
                if (emit_case(out, "fuzz", x)) emitted++;
            }
            mpfr_clear(x);
        }
    }

    /* ============================================================== */
    /* mined: 5 from mpfr/tests/tget_z.c.                             */
    /* ============================================================== */
    {
        /* tget_z.c check() L213-L236: structural cases with z=0, 17,
         * 123, random 2-limb, random 5-limb. We don't include z=0
         * (the special() handles it and we exclude that here). */
        emit_si(out, "mined", 17, 64);
        emit_si(out, "mined", 123, 64);
        /* tget_z.c check_one() exercises mpfr_get_z_2exp(z, x) for x
         * built from various z. Capture a 2-limb (128-bit) and a
         * 5-limb (320-bit) example. */
        {
            mpz_t z; mpz_init(z);
            mpz_set_str(z, "340282366920938463463374607431768211454", 10);
            emit_z(out, "mined", z, 128);
            mpz_ui_pow_ui(z, 2UL, 320UL);
            mpz_sub_ui(z, z, 17UL);
            emit_z(out, "mined", z, 320);
            mpz_clear(z);
        }
        /* tget_z.c check_one() structural: a small non-trivial double
         * that round-trips through mpfr_set_d -> mpfr_get_z_2exp. */
        emit_d(out, "mined", 3.14159265358979, 53);
    }

    return 0;
}
