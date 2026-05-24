/*
 * golden_driver.c — Golden master for MPFR's mpfr_div_2si.
 *
 * C signature
 * -----------
 *
 *   int mpfr_div_2si(mpfr_t rop, mpfr_srcptr op, long e, mpfr_rnd_t rnd);
 *
 *   Computes rop = op / 2^e = op * 2^(-e), refit to MPFR_PREC(rop),
 *   rounded per rnd. Ref: mpfr/src/div_2si.c.
 *
 * The driver shape mirrors mul_2si exactly — same wire format, same
 * tag distribution, same range conservatism — with the C call site
 * swapped. The `e` semantics ARE different on the C side (div by 2^e
 * vs mul by 2^e), so an agent porting the wrong direction would see
 * every nonzero-e case fail.
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
#  error "mpfr_div_2si golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

_Static_assert(sizeof(long) == 8, "mpfr_div_2si golden requires 64-bit long");

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr op, long e,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_div_2si(rop, op, e, rnd);
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

static inline void emit_d(FILE *out, const char *tag,
                          double d, uint64_t xprec,
                          long e, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec);
    mpfr_set_d(x, d, MPFR_RNDN);
    emit_case(out, tag, x, e, prec, rnd);
    mpfr_clear(x);
}

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

    /* happy: ~25 */
    {
        emit_si(out, "happy", 1, 53, 0, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, 53, 1, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, 53, -1, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, 53, 10, 53, MPFR_RNDN);
        emit_si(out, "happy", 1, 53, -10, 53, MPFR_RNDN);
        emit_si(out, "happy", 12, 53, 2, 53, MPFR_RNDN);  /* 12/4 = 3 */
        emit_si(out, "happy", 12, 53, -2, 53, MPFR_RNDN); /* 12*4 = 48 */
        emit_si(out, "happy", -12, 53, 2, 53, MPFR_RNDN);
        emit_si(out, "happy", -12, 53, -2, 53, MPFR_RNDN);
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

    /* edge: ~50 */
    {
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1);
            for (int i = 0; i < 5; ++i) emit_case(out, "edge", x, 5, 53, RNDS[i]);
            mpfr_clear(x);
        }
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1);
            for (int i = 0; i < 5; ++i) emit_case(out, "edge", x, -5, 53, RNDS[i]);
            mpfr_clear(x);
        }
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, +1);
            for (int i = 0; i < 5; ++i) emit_case(out, "edge", x, 10, 53, RNDS[i]);
            mpfr_clear(x);
        }
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, -1);
            for (int i = 0; i < 5; ++i) emit_case(out, "edge", x, -10, 53, RNDS[i]);
            mpfr_clear(x);
        }
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x);
            for (int i = 0; i < 5; ++i) emit_case(out, "edge", x, 3, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* e=0, prec narrowing — refit alone. */
        emit_si(out, "edge", 0b10101, 5, 0, 3, MPFR_RNDN);
        emit_si(out, "edge", 0b10101, 5, 0, 3, MPFR_RNDZ);
        emit_si(out, "edge", 0b10101, 5, 0, 3, MPFR_RNDU);
        emit_si(out, "edge", 0b10101, 5, 0, 3, MPFR_RNDD);
        emit_si(out, "edge", 0b10101, 5, 0, 3, MPFR_RNDA);

        /* e=0, prec widening — exact pad. */
        emit_si(out, "edge", 5, 4, 0, 10, MPFR_RNDN);
        emit_si(out, "edge", 5, 4, 0, 53, MPFR_RNDN);
        emit_si(out, "edge", 5, 4, 0, 100, MPFR_RNDN);
        emit_si(out, "edge", -5, 4, 0, 100, MPFR_RNDD);
        emit_si(out, "edge", -5, 4, 0, 100, MPFR_RNDU);

        /* Large positive e (means dividing — value gets smaller). */
        emit_si(out, "edge", 1, 53, 1000, 53, MPFR_RNDN);
        emit_si(out, "edge", -1, 53, 1000, 53, MPFR_RNDN);
        emit_si(out, "edge", 3, 53, 999, 53, MPFR_RNDN);
        emit_si(out, "edge", 1, 53, 500, 100, MPFR_RNDN);
        emit_si(out, "edge", 1, 53, 100, 53, MPFR_RNDN);

        /* Large negative e (means multiplying). */
        emit_si(out, "edge", 1, 53, -1000, 53, MPFR_RNDN);
        emit_si(out, "edge", -1, 53, -1000, 53, MPFR_RNDN);
        emit_si(out, "edge", 3, 53, -999, 53, MPFR_RNDN);
        emit_si(out, "edge", 1, 53, -500, 100, MPFR_RNDN);
        emit_si(out, "edge", 1, 53, -100, 53, MPFR_RNDN);

        /* prec=1 corner. */
        emit_si(out, "edge", 5, 53, 0, 1, MPFR_RNDN);
        emit_si(out, "edge", 5, 53, 3, 1, MPFR_RNDU);
        emit_si(out, "edge", 5, 53, -3, 1, MPFR_RNDD);
        emit_si(out, "edge", -7, 53, 1, 1, MPFR_RNDN);
        emit_si(out, "edge", -7, 53, -1, 1, MPFR_RNDA);
    }

    /* adversarial: ~30 */
    {
        const long patterns[] = { 0b11011L, 0b10101L, 0b11111L, 0b11100L };
        const size_t n_pat = sizeof(patterns) / sizeof(patterns[0]);
        const long es[] = { 1, -1, 7, -7 };
        const size_t n_es = sizeof(es) / sizeof(es[0]);
        for (size_t p = 0; p < n_pat; ++p) {
            for (size_t k = 0; k < n_es; ++k) {
                emit_si(out, "adversarial",  patterns[p], 5, es[k], 3, MPFR_RNDN);
                if ((p + k) & 1) {
                    emit_si(out, "adversarial", -patterns[p], 5, es[k], 3, MPFR_RNDA);
                }
            }
        }

        emit_si(out, "adversarial",  0b1010, 4, 5, 2, MPFR_RNDN);
        emit_si(out, "adversarial", -0b1010, 4, -5, 2, MPFR_RNDN);
        emit_si(out, "adversarial",  0b1110, 4, 5, 2, MPFR_RNDN);
        emit_si(out, "adversarial", -0b1110, 4, -5, 2, MPFR_RNDN);

        emit_si(out, "adversarial", (1L << 53) - 1, 53, 1, 52, MPFR_RNDA);
        emit_si(out, "adversarial", (1L << 53) - 1, 53, -1, 52, MPFR_RNDA);
    }

    /* fuzz: 55 — distinct seed from mul_2si so streams don't accidentally
     * coincide. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xD2D2D2D2D2D2D2D2ULL);
        const uint64_t precs[6] = { 1, 2, 24, 53, 64, 100 };

        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t u = xs64_next(&rng);
            int64_t n;
            memcpy(&n, &u, sizeof n);
            n = (int32_t)(n & 0xFFFFFFFFLL);
            if (n == 0) n = 1;

            const uint64_t xprec = precs[xs64_below(&rng, 6)];
            const uint64_t prec  = precs[xs64_below(&rng, 6)];
            const long e = (long)((int64_t)(xs64_next(&rng) % 201) - 100);
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec);
            mpfr_set_si(x, (long)n, MPFR_RNDN);
            emit_case(out, "fuzz", x, e, prec, rnd);
            mpfr_clear(x);
        }
    }

    /* mined: 5 — see tmul_2exp.c L180. */
    {
        /* tmul_2exp.c L180: mpfr_div_2si(z, x, 1, RNDU). Structural analog. */
        emit_d(out, "mined", 1.0, 8, 1, 8, MPFR_RNDU);
        emit_si(out, "mined", 1, 53, 0, 53, MPFR_RNDN);
        emit_si(out, "mined", 1, 53, 1, 53, MPFR_RNDN);  /* div by 2 */
        emit_d(out, "mined", 3.0, 53, 1, 53, MPFR_RNDN); /* exact */
        emit_si(out, "mined", -7, 53, 3, 53, MPFR_RNDZ);
    }

    return 0;
}
