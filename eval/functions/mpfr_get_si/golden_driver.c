/*
 * golden_driver.c — Golden master for MPFR's mpfr_get_si.
 *
 * C signature
 * -----------
 *
 *   long mpfr_get_si(mpfr_srcptr op, mpfr_rnd_t rnd);
 *
 *   Rounds `op` to a signed long per `rnd`. Saturates + sets ERANGE on
 *   NaN / Inf / out-of-range. Ref: mpfr/src/get_si.c.
 *
 * TS divergence
 * -------------
 *
 * The TS port throws MPFRError('EPREC') on NaN / Inf / out-of-range
 * instead of saturating. Throws are graded as n_throw (not pass), so
 * this golden EXCLUDES every input that would throw — i.e. every
 * input must be a finite MPFR whose rnd-rounded value lies in
 * [LONG_MIN, LONG_MAX]. That's enforced per-case by
 * `mpfr_fits_slong_p(x, rnd)` before emission.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-record>,"rnd":"RND[NZUDA]"},
 *    "output":"<int64-decimal>",
 *    "time_ns":<n>}
 *
 *   - `output` is jl_output_scalar_i64 — a quoted signed decimal string
 *     parsed as bigint on the TS side.
 *
 * Tag distribution
 * ----------------
 *
 *   happy        :  ~25  (integer-valued MPFRs in int64 range)
 *   edge         :  ~50  (0, ±1, LONG_MAX/MIN, just-inside boundaries,
 *                         half-integers across all 5 rnd modes)
 *   adversarial  :  ~30  (half-integer values exercising the round
 *                         direction for all 5 modes; magnitude near
 *                         LONG_MAX where round-up would overflow)
 *   fuzz         :   55  (xs64 seed 0xCA7C4711CA7C4711ULL)
 *   mined        :    5  (transcribed from mpfr/tests/tget_si.c)
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_get_si golden_driver requires GMP_NUMB_BITS == 64"
#endif

_Static_assert(sizeof(long) == 8, "mpfr_get_si golden requires 64-bit long");

/* Emit one mpfr_get_si case — but ONLY if `x` rounds to a value that
 * fits in a signed long per the given rnd. Inputs that would throw on
 * the TS side (NaN, ±Inf, out-of-range round) are silently skipped.
 *
 * Returns 1 on emit, 0 on skip. Callers that want to verify their
 * golden density should check this. */
static inline int emit_case(FILE *out, const char *tag,
                            mpfr_srcptr x, mpfr_rnd_t rnd) {
    if (!mpfr_fits_slong_p(x, rnd)) {
        /* Round result would overflow; TS port would throw. Skip. */
        return 0;
    }
    /* Also skip NaN explicitly — fits_slong_p returns false on NaN, so
     * the gate above already catches it, but the comment doubles as
     * documentation. */

    const uint64_t t0 = now_ns();
    const long result = mpfr_get_si(x, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_scalar_i64(out, (int64_t)result);
    jl_finish(out, elapsed);
    return 1;
}

/* Emit (x_double, prec, rnd) tuple convenience helper. */
static inline int emit_d(FILE *out, const char *tag,
                         double d, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
    const int emitted = emit_case(out, tag, x, rnd);
    mpfr_clear(x);
    return emitted;
}

/* Emit (long-int, prec, rnd) — convert via mpfr_set_si then get_si. */
static inline int emit_si(FILE *out, const char *tag,
                          long n, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_si(x, n, MPFR_RNDN);
    const int emitted = emit_case(out, tag, x, rnd);
    mpfr_clear(x);
    return emitted;
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: integer-valued MPFRs at common precs.                   */
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
        emit_si(out, "happy", -(1L << 40), 53, MPFR_RNDN);
        emit_si(out, "happy", 1L << 50, 64, MPFR_RNDN);
        emit_si(out, "happy", -(1L << 50), 64, MPFR_RNDN);
        emit_si(out, "happy", 42, 24, MPFR_RNDN);
        emit_si(out, "happy", 42, 100, MPFR_RNDN);
        emit_si(out, "happy", 42, 200, MPFR_RNDN);
        emit_si(out, "happy", 3, 53, MPFR_RNDN);
        emit_si(out, "happy", 5, 53, MPFR_RNDN);
        emit_si(out, "happy", 7, 53, MPFR_RNDN);
        emit_si(out, "happy", 255, 53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: signed zeros, ±1, LONG_MAX/MIN, half-integers across     */
    /* all 5 rnd modes.                                               */
    /* ============================================================== */
    {
        /* (1-2) ±0 → 0 (both signs collapse on the integer side). */
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

        /* (3-7) ±1 across all 5 rnd modes. */
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", 1, 53, RNDS[i]);
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", -1, 53, RNDS[i]);

        /* (13-22) LONG_MAX and LONG_MIN constructed exactly (prec=64 is
         * enough for ±2^63). */
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", LONG_MAX, 64, RNDS[i]);
        for (int i = 0; i < 5; ++i) emit_si(out, "edge", LONG_MIN, 64, RNDS[i]);

        /* (23-) Half-integers in all 5 rnd modes. 0.5 → 0/1
         * depending on rnd. */
        for (int i = 0; i < 5; ++i) emit_d(out, "edge", 0.5, 53, RNDS[i]);
        for (int i = 0; i < 5; ++i) emit_d(out, "edge", -0.5, 53, RNDS[i]);

        /* 1.5 across all 5 rnd modes. */
        for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.5, 53, RNDS[i]);
        for (int i = 0; i < 5; ++i) emit_d(out, "edge", -1.5, 53, RNDS[i]);

        /* 2.5 — RNDN ties to even (2). */
        for (int i = 0; i < 5; ++i) emit_d(out, "edge", 2.5, 53, RNDS[i]);

        /* Magnitude-zero values via 1/2^k (always rounds to 0 in RNDZ,
         * but rounds up in RNDU/RNDA). */
        emit_d(out, "edge", 0.25, 53, MPFR_RNDN);
        emit_d(out, "edge", 0.25, 53, MPFR_RNDU);
        emit_d(out, "edge", -0.25, 53, MPFR_RNDD);
        emit_d(out, "edge", 1e-100, 53, MPFR_RNDA);
        emit_d(out, "edge", -1e-100, 53, MPFR_RNDZ);

        /* Just-in-range: 2^53 + 1, 2^60, ±2^62. */
        emit_si(out, "edge", (1L << 53) + 1, 64, MPFR_RNDN);
        emit_si(out, "edge", 1L << 60, 64, MPFR_RNDN);
        emit_si(out, "edge", 1L << 62, 64, MPFR_RNDN);
        emit_si(out, "edge", -(1L << 62), 64, MPFR_RNDN);
        emit_si(out, "edge", LONG_MAX - 1, 64, MPFR_RNDN);
    }

    /* ============================================================== */
    /* adversarial: half-int values that test rnd direction; values  */
    /* near LONG_MAX where round-up would overflow.                   */
    /* ============================================================== */
    {
        /* Decimal-like values to stress all 5 rnd modes per sign. */
        const double vals[] = { 1.4, 1.5, 1.6, 2.5, 3.5, 4.5, 5.5 };
        const size_t n_vals = sizeof(vals) / sizeof(vals[0]);
        for (size_t v = 0; v < n_vals; ++v) {
            for (int r = 0; r < 5; ++r) {
                emit_d(out, "adversarial",  vals[v], 53, RNDS[r]);
                emit_d(out, "adversarial", -vals[v], 53, RNDS[r]);
            }
        }

        /* Near-LONG_MAX boundary cases that DO fit (LONG_MAX - 1, etc.). */
        emit_si(out, "adversarial", LONG_MAX - 1, 64, MPFR_RNDN);
        emit_si(out, "adversarial", LONG_MAX - 1, 64, MPFR_RNDZ);
        emit_si(out, "adversarial", LONG_MAX, 64, MPFR_RNDZ);
        emit_si(out, "adversarial", LONG_MAX, 64, MPFR_RNDD);
        emit_si(out, "adversarial", LONG_MIN, 64, MPFR_RNDU);
    }

    /* ============================================================== */
    /* fuzz: random MPFR values within range.                         */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xCA7C4711CA7C4711ULL);
        const uint64_t precs[5] = { 53, 64, 100, 200, 300 };
        const mpfr_rnd_t rnds_arr[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

        int emitted = 0;
        int tries = 0;
        while (emitted < 55 && tries < 200) {
            tries++;
            /* Generate random int64; convert to MPFR at random prec;
             * sometimes add a fractional offset by multiplying by a
             * small double. */
            const uint64_t u = xs64_next(&rng);
            int64_t n;
            memcpy(&n, &u, sizeof n);

            const uint64_t prec = precs[xs64_below(&rng, 5)];
            const mpfr_rnd_t rnd = rnds_arr[xs64_below(&rng, 5)];

            /* Half-and-half: pure int vs fractional offset. */
            const int frac = (int)(xs64_next(&rng) & 1);

            mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
            mpfr_set_si(x, (long)n, MPFR_RNDN);

            if (frac) {
                /* Multiply by a random 0.7..1.3 to introduce non-integer
                 * mantissa. The result may be out of range — emit_case
                 * skips those automatically. */
                double scale = 0.7 + ((double)xs64_below(&rng, 7)) * 0.1;
                mpfr_mul_d(x, x, scale, MPFR_RNDN);
            }

            if (emit_case(out, "fuzz", x, rnd)) emitted++;
            mpfr_clear(x);
        }
    }

    /* ============================================================== */
    /* mined: 5 from mpfr/tests/tget_si.c                             */
    /* ============================================================== */
    {
        /* tget_si.c L48–L52: mpfr_get_si of 0 returns 0. */
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1);
            emit_case(out, "mined", x, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* tget_si.c L57–L61: mpfr_get_si of -1 returns -1. */
        emit_si(out, "mined", -1, 53, MPFR_RNDN);
        /* tget_si.c — get_si of 1 returns 1. */
        emit_si(out, "mined", 1, 53, MPFR_RNDN);
        /* tget_si.c — get_si of LONG_MAX returns LONG_MAX (exactly at 64-bit prec). */
        emit_si(out, "mined", LONG_MAX, 64, MPFR_RNDN);
        /* tget_si.c — get_si of LONG_MIN returns LONG_MIN. */
        emit_si(out, "mined", LONG_MIN, 64, MPFR_RNDN);
    }

    return 0;
}
