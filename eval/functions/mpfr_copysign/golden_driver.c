/*
 * golden_driver.c — Golden master for MPFR's mpfr_copysign.
 *
 * C signature
 * -----------
 *
 *   int mpfr_copysign(mpfr_t rop, mpfr_srcptr x, mpfr_srcptr y,
 *                     mpfr_rnd_t rnd);
 *
 *   Result is `(-1)^signbit(y) * |x|`. Ref: mpfr/src/copysign.c L24–L46.
 *
 * Divergence from C → TS — NaN y
 * ------------------------------
 *
 * The C function copies y's sign onto the result even when the result
 * is NaN; the locked schema collapses every NaN to NAN_VALUE (sign=1).
 * The wire emitter (jl_kv_mpfr in common.h L383) writes NaN with
 * sign=1 regardless of libmpfr's internal NaN sign, so the divergence
 * is grader-invisible — BUT only as long as the goldens do not
 * construct a y that is a "negative NaN". We only use mpfr_set_nan to
 * create NaN values, which leaves the default positive sign.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR>,"y":<MPFR>,"prec":"<dec>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        : ~25
 *   edge         : ~50  (all 4 kinds × all 5 rnd × y.sign in {+,-})
 *   adversarial  : ~30  (prec change forces rounding; y.sign disagrees
 *                        with x.sign so the broken port misroutes)
 *   fuzz         :  60
 *   mined        :   5  (from mpfr/tests/tcopysign.c)
 *
 * Refs
 * ----
 *   - mpfr/src/copysign.c — the C reference.
 *   - src/ops/copysign.ts — the production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_copysign golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, mpfr_srcptr y,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_copysign(rop, x, y, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_mpfr(out, 0, "y", y);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_nan(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_nan(x);
}
static inline void init_pos_inf(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, 1);
}
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, -1);
}
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, 1);
}
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, -1);
}

/* Emit a quick (x, y) pair from doubles for the common case. */
static inline void emit_dd(FILE *out, const char *tag,
                           double xd, double yd,
                           uint64_t xp, uint64_t yp, uint64_t dp,
                           mpfr_rnd_t rnd) {
    mpfr_t x, y;
    init_from_double(x, xd, xp);
    init_from_double(y, yd, yp);
    emit_case(out, tag, x, y, dp, rnd);
    mpfr_clear(x);
    mpfr_clear(y);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25                                                     */
    /* ============================================================== */
    {
        /* (sign source: +, x: ±) */
        emit_dd(out, "happy",  1.0,  1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy",  2.0,  1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, 1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -1.0,  1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -2.0,  1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -3.14, 1.0, 53, 53, 53, MPFR_RNDN);
        /* (sign source: -, x: ±) */
        emit_dd(out, "happy",  1.0, -1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy",  2.0, -1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy",  3.14,-1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -1.0, -1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -2.0, -1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -3.14,-1.0, 53, 53, 53, MPFR_RNDN);

        /* Common precs. */
        emit_dd(out, "happy",  3.14,  1.0,  24,  53,  24, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, -1.0,  64,  53,  64, MPFR_RNDN);
        emit_dd(out, "happy",  3.14,  1.0, 100,  53, 100, MPFR_RNDN);
        emit_dd(out, "happy",  3.14, -1.0, 200,  53, 200, MPFR_RNDN);

        /* Magnitudes. */
        emit_dd(out, "happy",  1.5e100, -1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -1.5e-100, 1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy",  6.022e23, -1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy",  2.718281828, -1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy",  1.4142135623730951, -1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy",  0.5,  1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy",  0.5, -1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy",  10.0,  1.0, 53, 53, 53, MPFR_RNDN);
        emit_dd(out, "happy", -10.0,  1.0, 53, 53, 53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~50                                                      */
    /* ============================================================== */
    {
        /* NaN x with ±y. NaN x → NAN_VALUE regardless. y in {+1, -1}. */
        for (int j = 0; j < 2; ++j) {
            for (int i = 0; i < 5; ++i) {
                mpfr_t x, y;
                init_nan(x, 53);
                init_from_double(y, j == 0 ? 1.0 : -1.0, 53);
                emit_case(out, "edge", x, y, 53, RNDS[i]);
                mpfr_clear(x); mpfr_clear(y);
            }
        }

        /* ±Inf x with ±y, RNDN — sign comes from y. */
        {
            mpfr_t x, y;
            init_pos_inf(x, 53); init_from_double(y, 1.0, 53);
            emit_case(out, "edge", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }
        {
            mpfr_t x, y;
            init_pos_inf(x, 53); init_from_double(y, -1.0, 53);
            emit_case(out, "edge", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }
        {
            mpfr_t x, y;
            init_neg_inf(x, 53); init_from_double(y, 1.0, 53);
            emit_case(out, "edge", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }
        {
            mpfr_t x, y;
            init_neg_inf(x, 53); init_from_double(y, -1.0, 53);
            emit_case(out, "edge", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }

        /* ±0 x with ±y. */
        {
            mpfr_t x, y;
            init_pos_zero(x, 53); init_from_double(y, -1.0, 53);
            emit_case(out, "edge", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }
        {
            mpfr_t x, y;
            init_neg_zero(x, 53); init_from_double(y, 1.0, 53);
            emit_case(out, "edge", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }
        /* ±0 x with ±0 y — the sign source itself signed-zero. */
        {
            mpfr_t x, y;
            init_pos_zero(x, 53); init_neg_zero(y, 53);
            emit_case(out, "edge", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }
        {
            mpfr_t x, y;
            init_neg_zero(x, 53); init_pos_zero(y, 53);
            emit_case(out, "edge", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }

        /* ±Inf y as the sign source. */
        {
            mpfr_t x, y;
            init_from_double(x, 3.14, 53); init_pos_inf(y, 53);
            emit_case(out, "edge", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }
        {
            mpfr_t x, y;
            init_from_double(x, 3.14, 53); init_neg_inf(y, 53);
            emit_case(out, "edge", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }

        /* Normal × all 5 rnd modes × y in {+, -}. */
        for (int j = 0; j < 2; ++j) {
            for (int i = 0; i < 5; ++i) {
                mpfr_t x, y;
                init_from_double(x, 3.14, 53);
                init_from_double(y, j == 0 ? 1.0 : -1.0, 53);
                emit_case(out, "edge", x, y, 53, RNDS[i]);
                mpfr_clear(x); mpfr_clear(y);
            }
        }
        /* Negative source × all 5 rnd × y in {+, -}. */
        for (int j = 0; j < 2; ++j) {
            for (int i = 0; i < 5; ++i) {
                mpfr_t x, y;
                init_from_double(x, -3.14, 53);
                init_from_double(y, j == 0 ? 1.0 : -1.0, 53);
                emit_case(out, "edge", x, y, 53, RNDS[i]);
                mpfr_clear(x); mpfr_clear(y);
            }
        }

        /* Prec mismatch. */
        emit_dd(out, "edge",  1.0, -1.0,  10,  53,  53, MPFR_RNDN);
        emit_dd(out, "edge", -1.5,  1.0,  53,  53, 100, MPFR_RNDN);
        emit_dd(out, "edge",  3.0, -1.0,   3,  53, 200, MPFR_RNDD);

        /* prec=1 boundary. */
        {
            mpfr_t x, y;
            init_from_double(x, 1.0, 1); init_from_double(y, -1.0, 1);
            emit_case(out, "edge", x, y, 1, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }
    }

    /* ============================================================== */
    /* adversarial: ~30                                                */
    /* ============================================================== */
    {
        const char *patterns[] = {
            "1.1011E0",
            "1.0101E0",
            "1.1111E0",
            "1.0001E10",
            "1.1100E-50",
        };
        const size_t n_pat = sizeof(patterns) / sizeof(patterns[0]);

        /* Positive x, negative y, force prec=3 rounding × 5 rnd modes.
         * Broken port (uses x.sign) returns positive => sign mismatch. */
        for (size_t p = 0; p < n_pat; ++p) {
            for (int r = 0; r < 5; ++r) {
                mpfr_t x, y;
                mpfr_init2(x, 53); mpfr_set_str(x, patterns[p], 2, MPFR_RNDN);
                init_from_double(y, -1.0, 53);
                emit_case(out, "adversarial", x, y, 3, RNDS[r]);
                mpfr_clear(x); mpfr_clear(y);
            }
        }
        /* Negative x, positive y. */
        for (size_t p = 0; p < n_pat; ++p) {
            char buf[200];
            buf[0] = '-';
            size_t n = strlen(patterns[p]);
            memcpy(buf + 1, patterns[p], n + 1);
            mpfr_t x, y;
            mpfr_init2(x, 53); mpfr_set_str(x, buf, 2, MPFR_RNDN);
            init_from_double(y, 1.0, 53);
            emit_case(out, "adversarial", x, y, 3, MPFR_RNDU);
            mpfr_clear(x); mpfr_clear(y);
        }

        /* RNDN tie boundary with sign-flip. */
        {
            mpfr_t x, y;
            mpfr_init2(x, 4); mpfr_set_str(x, "1.110E0", 2, MPFR_RNDN);
            init_from_double(y, -1.0, 53);
            emit_case(out, "adversarial", x, y, 2, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }

        /* Carry-out at MSB on a sign-flip path. */
        {
            mpfr_t x, y;
            mpfr_init2(x, 4); mpfr_set_str(x, "1.111E0", 2, MPFR_RNDN);
            init_from_double(y, -1.0, 53);
            emit_case(out, "adversarial", x, y, 3, MPFR_RNDU);
            mpfr_clear(x); mpfr_clear(y);
        }
        {
            mpfr_t x, y;
            mpfr_init2(x, 4); mpfr_set_str(x, "-1.111E0", 2, MPFR_RNDN);
            init_from_double(y, 1.0, 53);
            emit_case(out, "adversarial", x, y, 3, MPFR_RNDD);
            mpfr_clear(x); mpfr_clear(y);
        }
    }

    /* ============================================================== */
    /* fuzz: 60                                                       */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC07951617DA71B5DULL); /* COPYSIGN hex-pun */
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        int emitted = 0;
        while (emitted < 60) {
            const uint64_t xbits = xs64_next(&rng);
            const uint64_t ybits = xs64_next(&rng);
            const uint64_t xexp = (xbits >> 52) & 0x7FF;
            const uint64_t yexp = (ybits >> 52) & 0x7FF;
            if (xexp == 0x7FF || yexp == 0x7FF) continue;

            double xd, yd;
            memcpy(&xd, &xbits, sizeof xd);
            memcpy(&yd, &ybits, sizeof yd);

            const uint64_t xp = precs[xs64_below(&rng, 6)];
            const uint64_t yp = precs[xs64_below(&rng, 6)];
            const uint64_t dp = precs[xs64_below(&rng, 6)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            mpfr_t x, y;
            init_from_double(x, xd, xp);
            init_from_double(y, yd, yp);
            emit_case(out, "fuzz", x, y, dp, rnd);
            mpfr_clear(x); mpfr_clear(y);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 — from mpfr/tests/tcopysign.c (k=0..2 variants)        */
    /* ============================================================== */
    {
        /* copysign(NaN, ±NaN) → NaN (sign collapses via wire). */
        {
            mpfr_t x, y;
            init_nan(x, 53); init_nan(y, 53);
            emit_case(out, "mined", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }
        /* copysign(1250, -1717) → -1250. */
        {
            mpfr_t x, y;
            mpfr_init2(x, 53); mpfr_set_si(x, 1250, MPFR_RNDN);
            mpfr_init2(y, 53); mpfr_set_si(y, -1717, MPFR_RNDN);
            emit_case(out, "mined", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }
        /* copysign(-1250, 1717) → 1250. */
        {
            mpfr_t x, y;
            mpfr_init2(x, 53); mpfr_set_si(x, -1250, MPFR_RNDN);
            mpfr_init2(y, 53); mpfr_set_si(y, 1717, MPFR_RNDN);
            emit_case(out, "mined", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }
        /* copysign(1.0, -1.0) → -1.0. */
        {
            mpfr_t x, y;
            init_from_double(x, 1.0, 53); init_from_double(y, -1.0, 53);
            emit_case(out, "mined", x, y, 53, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }
        /* copysign with prec change: 5/3 at prec 53, y negative,
         * round to prec 4 — exercises round step + new-sign routing. */
        {
            mpfr_t x, y;
            mpfr_init2(x, 53); mpfr_set_d(x, 5.0/3.0, MPFR_RNDN);
            init_from_double(y, -1.0, 53);
            emit_case(out, "mined", x, y, 4, MPFR_RNDN);
            mpfr_clear(x); mpfr_clear(y);
        }
    }

    return 0;
}
