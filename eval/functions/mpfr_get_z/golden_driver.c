/*
 * golden_driver.c — Golden master for MPFR's mpfr_get_z.
 *
 * C signature
 * -----------
 *
 *   int mpfr_get_z(mpz_ptr z, mpfr_srcptr f, mpfr_rnd_t rnd);
 *
 *   Rounds f to an integer per rnd, stores in z, returns ternary.
 *   Ref: mpfr/src/get_z.c.
 *
 * TS divergence
 * -------------
 *
 * The TS port throws MPFRError('EPREC') on NaN / ±Inf instead of
 * returning 0 with a hidden ERANGE flag. The golden EXCLUDES NaN and
 * Inf inputs — the harness can't grade expected-throw cases, and the
 * domain-error behaviour is covered by inspection of src/ops/get_z.ts.
 *
 * The TS port returns ONLY the bigint value, not (value, ternary).
 * We emit only the bigint here (via jl_output_scalar_str so arbitrarily
 * large magnitudes round-trip through BigInt parsing).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-record>,"rnd":"RND[NZUDA]"},
 *    "output":"<decimal>",
 *    "time_ns":<n>}
 *
 *   `output` is the decimal repr of the rounded integer; the TS-side
 *   decodeExpectedOutput's `isDecimalIntegerString` branch parses it as
 *   bigint and compareOutput's scalar/bigint branch accepts the port's
 *   return.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_get_z golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Emit one mpfr_get_z case. Skips NaN / ±Inf inputs silently (the TS
 * port throws on those). Returns 1 on emit, 0 on skip. */
static inline int emit_case(FILE *out, const char *tag,
                            mpfr_srcptr x, mpfr_rnd_t rnd) {
    if (mpfr_nan_p(x) || mpfr_inf_p(x)) {
        return 0;
    }

    mpz_t z; mpz_init(z);
    const uint64_t t0 = now_ns();
    /* The C function returns the ternary; we don't emit it (TS port
     * surface returns only the bigint), but we do call get_z exactly
     * once so the produced z matches what a true round-and-extract
     * would yield. */
    (void)mpfr_get_z(z, x, rnd);
    const uint64_t elapsed = now_ns() - t0;

    char *s = mpz_get_str(NULL, 10, z);

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_scalar_str(out, s);
    jl_finish(out, elapsed);

    void (*gmp_free)(void *, size_t);
    mp_get_memory_functions(NULL, NULL, &gmp_free);
    gmp_free(s, strlen(s) + 1);
    mpz_clear(z);
    return 1;
}

/* Build x from a small int via mpfr_set_si, then emit. */
static inline int emit_si(FILE *out, const char *tag,
                          long n, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_si(x, n, MPFR_RNDN);
    const int e = emit_case(out, tag, x, rnd);
    mpfr_clear(x);
    return e;
}

/* Build x from a double. */
static inline int emit_d(FILE *out, const char *tag,
                         double d, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
    const int e = emit_case(out, tag, x, rnd);
    mpfr_clear(x);
    return e;
}

/* Build x from an mpz_t (large bigint). */
static inline int emit_z(FILE *out, const char *tag,
                         mpz_srcptr z, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_z(x, z, MPFR_RNDN);
    const int e = emit_case(out, tag, x, rnd);
    mpfr_clear(x);
    return e;
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: integer-valued MPFRs.                                   */
    /* ============================================================== */
    {
        emit_si(out, "happy", 0, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, 53, MPFR_RNDN);
        emit_si(out, "happy", -1, 53, MPFR_RNDN);
        emit_si(out, "happy", 2, 53, MPFR_RNDN);
        emit_si(out, "happy", -2, 53, MPFR_RNDN);
        emit_si(out, "happy", 42, 53, MPFR_RNDN);
        emit_si(out, "happy", -42, 53, MPFR_RNDN);
        emit_si(out, "happy", 100, 53, MPFR_RNDN);
        emit_si(out, "happy", -100, 53, MPFR_RNDN);
        emit_si(out, "happy", 1000, 53, MPFR_RNDN);
        emit_si(out, "happy", 1000000L, 53, MPFR_RNDN);
        emit_si(out, "happy", -1000000L, 53, MPFR_RNDN);
        emit_si(out, "happy", 1000000000L, 53, MPFR_RNDN);
        emit_si(out, "happy", -1000000000L, 53, MPFR_RNDN);
        emit_si(out, "happy", 1L << 40, 53, MPFR_RNDN);
        emit_si(out, "happy", 1L << 50, 64, MPFR_RNDN);
        emit_si(out, "happy", 42, 24, MPFR_RNDN);
        emit_si(out, "happy", 42, 100, MPFR_RNDN);
        emit_si(out, "happy", 42, 200, MPFR_RNDN);
        emit_si(out, "happy", 3, 53, MPFR_RNDN);
        emit_si(out, "happy", 5, 53, MPFR_RNDN);
        emit_si(out, "happy", 7, 53, MPFR_RNDN);
        emit_si(out, "happy", 255, 53, MPFR_RNDN);
        emit_si(out, "happy", 256, 53, MPFR_RNDN);
        emit_si(out, "happy", 65535, 53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: specials (±0), ±1, large-magnitude integers, half-       */
    /* integers across all 5 rnd modes, sub-1 values.                 */
    /* ============================================================== */
    {
        /* (1-2) ±0 → 0 (sign collapses). */
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1);
            emit_case(out, "edge", x, MPFR_RNDN);
            mpfr_clear(x);
        }
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1);
            emit_case(out, "edge", x, MPFR_RNDN);
            mpfr_clear(x);
        }

        /* (3-12) ±1 across all 5 rnds. */
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", 1, 53, RNDS[i]);
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", -1, 53, RNDS[i]);

        /* (13-22) Large integers exactly representable. */
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", LONG_MAX, 64, RNDS[i]);
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", LONG_MIN, 64, RNDS[i]);

        /* (23-32) Half-integers across all 5 rnds. */
        for (int i = 0; i < 5; ++i) emit_d(out, "edge",  0.5, 53, RNDS[i]);
        for (int i = 0; i < 5; ++i) emit_d(out, "edge", -0.5, 53, RNDS[i]);

        /* (33-42) 1.5 across rnds, both signs. */
        for (int i = 0; i < 5; ++i) emit_d(out, "edge",  1.5, 53, RNDS[i]);
        for (int i = 0; i < 5; ++i) emit_d(out, "edge", -1.5, 53, RNDS[i]);

        /* (43-47) 2.5 — RNDN ties to even (2). */
        for (int i = 0; i < 5; ++i) emit_d(out, "edge", 2.5, 53, RNDS[i]);

        /* (48-50) Sub-1 magnitudes — |value|<1 branch. */
        emit_d(out, "edge",  0.25, 53, MPFR_RNDU);
        emit_d(out, "edge", -0.25, 53, MPFR_RNDD);
        emit_d(out, "edge",  1e-100, 53, MPFR_RNDA);

        /* (51+) Very large integers — beyond int64. Exercises the
         * arbitrary-precision output path. */
        {
            mpz_t z; mpz_init(z);
            mpz_ui_pow_ui(z, 2UL, 200UL);
            emit_z(out, "edge", z, 201, MPFR_RNDN);
            mpz_sub_ui(z, z, 1UL);
            emit_z(out, "edge", z, 201, MPFR_RNDN);
            mpz_ui_pow_ui(z, 2UL, 1000UL);
            emit_z(out, "edge", z, 1001, MPFR_RNDN);
            mpz_neg(z, z);
            emit_z(out, "edge", z, 1001, MPFR_RNDN);
            mpz_clear(z);
        }
    }

    /* ============================================================== */
    /* adversarial: half-int / rounding-direction stress.             */
    /* ============================================================== */
    {
        const double vals[] = { 1.4, 1.5, 1.6, 2.5, 3.5, 4.5, 5.5 };
        const size_t n_vals = sizeof(vals) / sizeof(vals[0]);
        for (size_t v = 0; v < n_vals; ++v) {
            for (int r = 0; r < 5; ++r) {
                emit_d(out, "adversarial",  vals[v], 53, RNDS[r]);
                emit_d(out, "adversarial", -vals[v], 53, RNDS[r]);
            }
        }
    }

    /* ============================================================== */
    /* fuzz: random MPFR values.                                      */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xCA7E972CA7E972CAULL);
        const uint64_t precs[5] = { 53, 64, 100, 200, 300 };

        int emitted = 0;
        int tries = 0;
        while (emitted < 55 && tries < 200) {
            tries++;
            const uint64_t u = xs64_next(&rng);
            int64_t n;
            memcpy(&n, &u, sizeof n);

            const uint64_t prec = precs[xs64_below(&rng, 5)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            const int frac = (int)(xs64_next(&rng) & 1);
            mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
            mpfr_set_si(x, (long)n, MPFR_RNDN);
            if (frac) {
                double scale = 0.7 + ((double)xs64_below(&rng, 7)) * 0.1;
                mpfr_mul_d(x, x, scale, MPFR_RNDN);
            }
            if (emit_case(out, "fuzz", x, rnd)) emitted++;
            mpfr_clear(x);
        }
    }

    /* ============================================================== */
    /* mined: 5 from mpfr/tests/tget_z.c.                             */
    /* ============================================================== */
    {
        /* tget_z.c L43–L50: mpfr_set_str(x, "17.5", 10, RNDN);
         *                   mpfr_get_z RNDN → 18 (inex > 0). */
        {
            mpfr_t x; mpfr_init2(x, 6);
            mpfr_set_str(x, "17.5", 10, MPFR_RNDN);
            emit_case(out, "mined", x, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* tget_z.c L35: mpfr_set_ui(x, 2047, RNDU) at prec 2.
         * RNDU forces 2047 → 2048 (the only repr above at prec 2). */
        {
            mpfr_t x; mpfr_init2(x, 2);
            mpfr_set_ui(x, 2047, MPFR_RNDU);
            emit_case(out, "mined", x, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* tget_z.c L56: mpfr_set_ui(x, 0, RNDN) → 0. */
        {
            mpfr_t x; mpfr_init2(x, 53);
            mpfr_set_ui(x, 0, MPFR_RNDN);
            emit_case(out, "mined", x, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* tget_z.c structural: small positive int. */
        emit_si(out, "mined", 17, 53, MPFR_RNDN);
        /* tget_z.c structural: small negative int. */
        emit_si(out, "mined", -17, 53, MPFR_RNDN);
    }

    return 0;
}
