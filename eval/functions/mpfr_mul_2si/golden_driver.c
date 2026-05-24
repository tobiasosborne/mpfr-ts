/*
 * golden_driver.c — Golden master for MPFR's mpfr_mul_2si.
 *
 * C signature
 * -----------
 *
 *   int mpfr_mul_2si(mpfr_t rop, mpfr_srcptr op, long e, mpfr_rnd_t rnd);
 *
 *   Computes rop = op * 2^e, refit to MPFR_PREC(rop), rounded per rnd.
 *   Ref: mpfr/src/mul_2si.c.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The TS port `mpfr_mul_2si(x, e, prec, rnd) -> Result` takes prec
 * positionally, takes `e` as a bigint (decoded from a quoted int64
 * decimal on the wire), and omits the C emax/emin range check. The
 * golden therefore avoids inputs that would trigger overflow/underflow
 * on the C side (so the libmpfr-emitted value matches the unbounded
 * TS computation). At the default emax/emin the safe `e` range is
 * comfortably within [-1000000, +1000000] for the magnitudes we feed
 * in; the golden uses |e| <= 1000.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-record>,"e":"<int64-decimal>",
 *              "prec":"<prec-decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_mul_2si golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

_Static_assert(sizeof(long) == 8, "mpfr_mul_2si golden requires 64-bit long");

/* Emit one mpfr_mul_2si case using a pre-built `op` value at `prec`.
 * The caller is responsible for x's construction; we do the timing
 * and emission. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr op, long e,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_mul_2si(rop, op, e, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", op);
    jl_kv_i64(out, 0, "e", (int64_t)e);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

/* Convenience: build x from a double, then emit_case. */
static inline void emit_d(FILE *out, const char *tag,
                          double d, uint64_t xprec,
                          long e, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec);
    mpfr_set_d(x, d, MPFR_RNDN);
    emit_case(out, tag, x, e, prec, rnd);
    mpfr_clear(x);
}

/* Convenience: build x from a signed integer. */
static inline void emit_si(FILE *out, const char *tag,
                           long n, uint64_t xprec,
                           long e, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec);
    mpfr_set_si(x, n, MPFR_RNDN);
    emit_case(out, tag, x, e, prec, rnd);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25 cases — common values at common precs, e in [-10, 10]. */
    /* ============================================================== */
    {
        emit_si(out, "happy", 1, 53, 0, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, 53, 1, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, 53, -1, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, 53, 10, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, 53, -10, 53, MPFR_RNDN);
        emit_si(out, "happy", 3, 53, 5, 53, MPFR_RNDN);
        emit_si(out, "happy", 3, 53, -5, 53, MPFR_RNDN);
        emit_si(out, "happy", -3, 53, 5, 53, MPFR_RNDN);
        emit_si(out, "happy", -3, 53, -5, 53, MPFR_RNDN);
        emit_si(out, "happy", 42, 53, 0, 53, MPFR_RNDN);
        emit_si(out, "happy", 42, 53, 1, 53, MPFR_RNDN);
        emit_si(out, "happy", 100, 53, 7, 53, MPFR_RNDN);
        emit_si(out, "happy", -100, 53, 7, 53, MPFR_RNDN);
        emit_d(out, "happy", 1.5, 53, 1, 53, MPFR_RNDN);
        emit_d(out, "happy", 1.5, 53, -1, 53, MPFR_RNDN);
        emit_d(out, "happy", 3.14, 53, 3, 53, MPFR_RNDN);
        emit_d(out, "happy", 3.14, 53, -3, 53, MPFR_RNDN);
        emit_d(out, "happy", 0.1, 53, 4, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, 24, 0, 24, MPFR_RNDN);
        emit_si(out, "happy", 1, 24, 5, 24, MPFR_RNDN);
        emit_si(out, "happy", 1, 100, 0, 100, MPFR_RNDN);
        emit_si(out, "happy", 1, 100, 50, 100, MPFR_RNDN);
        emit_si(out, "happy", 1, 200, -50, 200, MPFR_RNDN);
        emit_si(out, "happy", 7, 53, 8, 53, MPFR_RNDN);
        emit_si(out, "happy", -7, 53, -8, 53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~50 cases — specials, e=0, prec mismatches, large |e|.   */
    /* ============================================================== */
    {
        /* (1-5) ±0 across all 5 rnds — sign preserved, prec adopted. */
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1);
            for (int i = 0; i < 5; ++i) emit_case(out, "edge", x, 5, 53, RNDS[i]);
            mpfr_clear(x);
        }
        /* (6-10) -0 same. */
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1);
            for (int i = 0; i < 5; ++i) emit_case(out, "edge", x, -5, 53, RNDS[i]);
            mpfr_clear(x);
        }
        /* (11-15) +Inf across all 5 rnds — sign preserved. */
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, +1);
            for (int i = 0; i < 5; ++i) emit_case(out, "edge", x, 10, 53, RNDS[i]);
            mpfr_clear(x);
        }
        /* (16-20) -Inf. */
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, -1);
            for (int i = 0; i < 5; ++i) emit_case(out, "edge", x, -10, 53, RNDS[i]);
            mpfr_clear(x);
        }
        /* (21-25) NaN. */
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x);
            for (int i = 0; i < 5; ++i) emit_case(out, "edge", x, 3, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (26-30) e=0 with prec narrowing — exercises refit-without-shift.
         * 0b10101 at prec=3 forces rounding. */
        emit_si(out, "edge", 0b10101, 5, 0, 3, MPFR_RNDN);
        emit_si(out, "edge", 0b10101, 5, 0, 3, MPFR_RNDZ);
        emit_si(out, "edge", 0b10101, 5, 0, 3, MPFR_RNDU);
        emit_si(out, "edge", 0b10101, 5, 0, 3, MPFR_RNDD);
        emit_si(out, "edge", 0b10101, 5, 0, 3, MPFR_RNDA);

        /* (31-35) e=0 with prec widening — exact pad. */
        emit_si(out, "edge", 5, 4, 0, 10, MPFR_RNDN);
        emit_si(out, "edge", 5, 4, 0, 53, MPFR_RNDN);
        emit_si(out, "edge", 5, 4, 0, 100, MPFR_RNDN);
        emit_si(out, "edge", -5, 4, 0, 100, MPFR_RNDD);
        emit_si(out, "edge", -5, 4, 0, 100, MPFR_RNDU);

        /* (36-40) Large positive e. */
        emit_si(out, "edge", 1, 53, 1000, 53, MPFR_RNDN);
        emit_si(out, "edge", -1, 53, 1000, 53, MPFR_RNDN);
        emit_si(out, "edge", 3, 53, 999, 53, MPFR_RNDN);
        emit_si(out, "edge", 1, 53, 500, 100, MPFR_RNDN);
        emit_si(out, "edge", 1, 53, 100, 53, MPFR_RNDN);

        /* (41-45) Large negative e. */
        emit_si(out, "edge", 1, 53, -1000, 53, MPFR_RNDN);
        emit_si(out, "edge", -1, 53, -1000, 53, MPFR_RNDN);
        emit_si(out, "edge", 3, 53, -999, 53, MPFR_RNDN);
        emit_si(out, "edge", 1, 53, -500, 100, MPFR_RNDN);
        emit_si(out, "edge", 1, 53, -100, 53, MPFR_RNDN);

        /* (46-50) prec=1 — minimum precision corner. */
        emit_si(out, "edge", 5, 53, 0, 1, MPFR_RNDN);
        emit_si(out, "edge", 5, 53, 3, 1, MPFR_RNDU);
        emit_si(out, "edge", 5, 53, -3, 1, MPFR_RNDD);
        emit_si(out, "edge", -7, 53, 1, 1, MPFR_RNDN);
        emit_si(out, "edge", -7, 53, -1, 1, MPFR_RNDA);
    }

    /* ============================================================== */
    /* adversarial: ~30 cases — refit-rounding combined with non-zero */
    /* e across all 5 rnd modes; carry-out cases.                     */
    /* ============================================================== */
    {
        /* Patterns at prec=3 (drops 2 bits), with non-zero e to force
         * both rounding AND exponent shift. */
        const long patterns[] = {
            0b11011L,   /* 27 — RNDN rounds up; RNDA/RNDU positive inc */
            0b10101L,   /* 21 — RNDN rounds down; RNDA positive inc */
            0b11111L,   /* 31 — carry-out candidate under RNDA */
            0b11100L,   /* 28 — dropped "00" exact at prec=3 */
        };
        const size_t n_pat = sizeof(patterns) / sizeof(patterns[0]);
        const long es[] = { 1, -1, 7, -7 };
        const size_t n_es = sizeof(es) / sizeof(es[0]);
        for (size_t p = 0; p < n_pat; ++p) {
            for (size_t k = 0; k < n_es; ++k) {
                /* Pair positive/negative input with mixed signs of e. */
                emit_si(out, "adversarial",  patterns[p], 5, es[k], 3, MPFR_RNDN);
                if ((p + k) & 1) {
                    emit_si(out, "adversarial", -patterns[p], 5, es[k], 3, MPFR_RNDA);
                }
            }
        }

        /* RNDN tie cases with non-zero e. */
        emit_si(out, "adversarial",  0b1010, 4, 5, 2, MPFR_RNDN);
        emit_si(out, "adversarial", -0b1010, 4, -5, 2, MPFR_RNDN);
        emit_si(out, "adversarial",  0b1110, 4, 5, 2, MPFR_RNDN);  /* odd-LSB inc */
        emit_si(out, "adversarial", -0b1110, 4, -5, 2, MPFR_RNDN);

        /* Carry-out at refit: (2^53 - 1) rounds up to 2^53 at prec=52 RNDA. */
        emit_si(out, "adversarial", (1L << 53) - 1, 53, 1, 52, MPFR_RNDA);
        emit_si(out, "adversarial", (1L << 53) - 1, 53, -1, 52, MPFR_RNDA);
    }

    /* ============================================================== */
    /* fuzz: 55 cases — PRNG-driven (xs64 seed 0x2D2D2D2D2D2D2D2DULL) */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x2D2D2D2D2D2D2D2DULL);
        const uint64_t precs[6] = { 1, 2, 24, 53, 64, 100 };

        for (int rep = 0; rep < 55; ++rep) {
            /* Random int input avoiding overflow boundaries. */
            const uint64_t u = xs64_next(&rng);
            int64_t n;
            memcpy(&n, &u, sizeof n);
            /* Clamp magnitude to fit comfortably in 32 bits so that, even
             * combined with |e| <= 100, the result stays clear of the
             * default emax/emin. */
            n = (int32_t)(n & 0xFFFFFFFFLL);
            if (n == 0) n = 1;

            const uint64_t xprec = precs[xs64_below(&rng, 6)];
            const uint64_t prec  = precs[xs64_below(&rng, 6)];
            /* e in [-100, 100] — random sign. */
            const long e = (long)((int64_t)(xs64_next(&rng) % 201) - 100);
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec);
            mpfr_set_si(x, (long)n, MPFR_RNDN);
            emit_case(out, "fuzz", x, e, prec, rnd);
            mpfr_clear(x);
        }
    }

    /* ============================================================== */
    /* mined: 5 cases from mpfr/tests/tmul_2exp.c.                    */
    /* ============================================================== */
    {
        /* tmul_2exp.c L153: mpfr_mul_2si(y, x, -1, MPFR_RNDU) with x = max - 1ulp.
         * We emit a structural analog at default emax: just a value × 2^-1. */
        emit_d(out, "mined", 1.0, 8, -1, 8, MPFR_RNDU);
        /* Identity: e=0 RNDN. */
        emit_si(out, "mined", 1, 53, 0, 53, MPFR_RNDN);
        /* Simple doubling: mpfr_mul_2si(x, 1) RNDN. */
        emit_si(out, "mined", 1, 53, 1, 53, MPFR_RNDN);
        /* Halving: e=-1 RNDN, exact. */
        emit_d(out, "mined", 1.5, 53, -1, 53, MPFR_RNDN);
        /* Negative scaling. */
        emit_si(out, "mined", -7, 53, 3, 53, MPFR_RNDZ);
    }

    return 0;
}
