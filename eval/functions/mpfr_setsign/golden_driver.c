/*
 * golden_driver.c — Golden master for MPFR's mpfr_setsign.
 *
 * C signature
 * -----------
 *
 *   int mpfr_setsign(mpfr_t rop, mpfr_srcptr op, int s, mpfr_rnd_t rnd);
 *
 *   - s != 0 → result sign is MPFR_SIGN_NEG;
 *   - s == 0 → result sign is MPFR_SIGN_POS.
 *
 *   Ref: mpfr/src/setsign.c L25–L38.
 *
 * TS port
 * -------
 *
 *   mpfr_setsign(x, sign, prec, rnd) → {value, ternary}
 *
 *   where `sign: boolean` (true = negative). The wire emits `sign` as a
 *   JSON boolean via jl_kv_bool; the runner's decodeInputValue passes
 *   booleans through unchanged so the port receives a real TS boolean.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-record>,"sign":<bool>,"prec":"<dec>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        : ~25
 *   edge         : ~50  (all 4 kinds × {sign=0, sign=1} × subset of rnd
 *                        modes; signed-zero observability; ±Inf flips)
 *   adversarial  : ~30  (prec change forcing rounding; new-sign disagrees
 *                        with x.sign → forces the broken port to misroute)
 *   fuzz         :  60
 *   mined        :   5  (from mpfr/tests/tcopysign.c, which exercises
 *                        setsign via the variant-k=3..6 paths)
 *
 * Refs
 * ----
 *   - mpfr/src/setsign.c — the C reference.
 *   - src/ops/setsign.ts — the production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_setsign golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Emit one mpfr_setsign golden case. `s` is the int that mpfr_setsign
 * takes; we also emit it on the wire as a JSON boolean (truthy → true). */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, int s, uint64_t prec,
                             mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_setsign(rop, x, s, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_bool(out, 0, "sign", s);
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

static inline void emit_d(FILE *out, const char *tag,
                          double d, int s,
                          uint64_t srcp, uint64_t dstp, mpfr_rnd_t rnd) {
    mpfr_t x; init_from_double(x, d, srcp);
    emit_case(out, tag, x, s, dstp, rnd);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25                                                     */
    /* ============================================================== */
    {
        /* Positive sources, sign=0 (no-op) and sign=1 (negate). */
        emit_d(out, "happy",  1.0,   0, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  1.0,   1, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  2.0,   0, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  2.0,   1, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  3.14,  0, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  3.14,  1, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  10.0,  0, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  10.0,  1, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  100.0, 0, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  100.0, 1, 53, 53, MPFR_RNDN);

        /* Negative sources — sign=0 returns positive, sign=1 stays negative. */
        emit_d(out, "happy", -1.0,   0, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -1.0,   1, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -3.14,  0, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -3.14,  1, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -10.0,  0, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -10.0,  1, 53, 53, MPFR_RNDN);

        /* Common precs. */
        emit_d(out, "happy",  3.14,  0,  24,  24, MPFR_RNDN);
        emit_d(out, "happy",  3.14,  1,  64,  64, MPFR_RNDN);
        emit_d(out, "happy",  3.14,  0, 100, 100, MPFR_RNDN);
        emit_d(out, "happy",  3.14,  1, 200, 200, MPFR_RNDN);

        /* Magnitudes. */
        emit_d(out, "happy",  1.5e100, 1, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -1.5e-100, 0, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  6.022e23, 1, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  2.718281828, 0, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  1.4142135623730951, 1, 53, 53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~50                                                      */
    /* ============================================================== */
    {
        /* NaN × 5 rnd × 2 signs — always NAN_VALUE, ternary 0. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_nan(x, 53);
            emit_case(out, "edge", x, 0, 53, RNDS[i]); /* sign=0 */
            emit_case(out, "edge", x, 1, 53, RNDS[i]); /* sign=1 */
            mpfr_clear(x);
        }

        /* +Inf × 5 rnd × 2 signs. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_pos_inf(x, 53);
            emit_case(out, "edge", x, 0, 53, RNDS[i]);
            emit_case(out, "edge", x, 1, 53, RNDS[i]);
            mpfr_clear(x);
        }
        /* -Inf × 5 rnd × 2 signs. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_neg_inf(x, 53);
            emit_case(out, "edge", x, 0, 53, RNDS[i]);
            emit_case(out, "edge", x, 1, 53, RNDS[i]);
            mpfr_clear(x);
        }
        /* +0, -0 × {sign=0, sign=1} × RNDN — signed-zero observability. */
        {
            mpfr_t x; init_pos_zero(x, 53);
            emit_case(out, "edge", x, 0, 53, MPFR_RNDN);
            emit_case(out, "edge", x, 1, 53, MPFR_RNDN);
            mpfr_clear(x);
        }
        {
            mpfr_t x; init_neg_zero(x, 53);
            emit_case(out, "edge", x, 0, 53, MPFR_RNDN);
            emit_case(out, "edge", x, 1, 53, MPFR_RNDN);
            mpfr_clear(x);
        }

        /* Same-prec normal × 5 rnd × {sign=0, sign=1}, positive source. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_from_double(x, 3.14, 53);
            emit_case(out, "edge", x, 0, 53, RNDS[i]);
            emit_case(out, "edge", x, 1, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* Prec mismatch on specials. */
        {
            mpfr_t x; init_neg_inf(x, 64);
            emit_case(out, "edge", x, 0, 200, MPFR_RNDN);
            mpfr_clear(x);
        }
        {
            mpfr_t x; init_pos_zero(x, 64);
            emit_case(out, "edge", x, 1, 200, MPFR_RNDD);
            mpfr_clear(x);
        }

        /* prec=1 boundary. */
        {
            mpfr_t x; init_from_double(x, 1.0, 1);
            emit_case(out, "edge", x, 0, 1, MPFR_RNDN);
            emit_case(out, "edge", x, 1, 1, MPFR_RNDN);
            mpfr_clear(x);
        }
    }

    /* ============================================================== */
    /* adversarial: ~30 — prec change forces rounding; sign override   */
    /* disagrees with x.sign so the broken port (preserving x.sign)    */
    /* produces a wrong sign on every case.                           */
    /* ============================================================== */
    {
        const char *patterns[] = {
            "1.1011E0",   /* 1.6875 */
            "1.0101E0",   /* 1.3125 */
            "1.1111E0",   /* 1.9375 — RNDA/RNDU carry-out candidate */
            "1.0001E10",
            "1.1100E-50",
        };
        const size_t n_pat = sizeof(patterns) / sizeof(patterns[0]);

        /* Positive source, sign=1 (force negative), all 5 rnd modes.
         * Broken port (preserves x.sign=+1) emits a positive result =>
         * sign mismatch. */
        for (size_t p = 0; p < n_pat; ++p) {
            for (int r = 0; r < 5; ++r) {
                mpfr_t x; mpfr_init2(x, 53);
                mpfr_set_str(x, patterns[p], 2, MPFR_RNDN);
                emit_case(out, "adversarial", x, 1, 3, RNDS[r]);
                mpfr_clear(x);
            }
        }
        /* Same patterns negative source, sign=0 (force positive). */
        for (size_t p = 0; p < n_pat; ++p) {
            char buf[200];
            buf[0] = '-';
            size_t n = strlen(patterns[p]);
            memcpy(buf + 1, patterns[p], n + 1);
            mpfr_t x; mpfr_init2(x, 53);
            mpfr_set_str(x, buf, 2, MPFR_RNDN);
            emit_case(out, "adversarial", x, 0, 3, MPFR_RNDU);
            mpfr_clear(x);
        }

        /* RNDN tie + sign override flips the rounding direction routing. */
        {
            mpfr_t x; mpfr_init2(x, 4); mpfr_set_str(x, "1.110E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x, 1, 2, MPFR_RNDN);
            mpfr_clear(x);
        }
        {
            mpfr_t x; mpfr_init2(x, 4); mpfr_set_str(x, "-1.110E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x, 0, 2, MPFR_RNDN);
            mpfr_clear(x);
        }

        /* Carry-out at MSB on a sign-flip path. */
        {
            mpfr_t x; mpfr_init2(x, 4); mpfr_set_str(x, "1.111E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x, 1, 3, MPFR_RNDU);
            mpfr_clear(x);
        }
        {
            mpfr_t x; mpfr_init2(x, 4); mpfr_set_str(x, "-1.111E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x, 0, 3, MPFR_RNDU);
            mpfr_clear(x);
        }
    }

    /* ============================================================== */
    /* fuzz: 60                                                       */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5E751601D1234567ULL); /* SETSIGN hex-pun */
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        int emitted = 0;
        while (emitted < 60) {
            const uint64_t bits = xs64_next(&rng);
            const uint64_t exp = (bits >> 52) & 0x7FF;
            if (exp == 0x7FF) continue;

            double d;
            memcpy(&d, &bits, sizeof d);

            const uint64_t srcp = precs[xs64_below(&rng, 6)];
            const uint64_t dstp = precs[xs64_below(&rng, 6)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            const int s = (int)(xs64_next(&rng) & 1);

            mpfr_t x; init_from_double(x, d, srcp);
            emit_case(out, "fuzz", x, s, dstp, rnd);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 — from mpfr/tests/tcopysign.c (exercises setsign in     */
    /* variants k=3..6).                                              */
    /* ============================================================== */
    {
        /* tcopysign.c L114–L124: setsign(NaN, signbit(NaN-neg)) — both
         * carry sign bit but the schema canonicalises NaN to NAN_VALUE. */
        {
            mpfr_t x; init_nan(x, 53);
            emit_case(out, "mined", x, 1, 53, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* tcopysign.c L126–L137: setsign(±1250, signbit(±NaN)) — but our
         * wire flattens NaN sign to 1, so emulate with a positive normal
         * x and sign=1 to mirror the C-side observation that the result
         * carries the requested sign. */
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_si(x, 1250, MPFR_RNDN);
            emit_case(out, "mined", x, 1, 53, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* setsign(-1250, 0) → +1250. */
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_si(x, -1250, MPFR_RNDN);
            emit_case(out, "mined", x, 0, 53, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* setsign(1.0, 1) → -1.0. */
        {
            mpfr_t x; init_from_double(x, 1.0, 53);
            emit_case(out, "mined", x, 1, 53, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* setsign with prec change rounding: 5/3 at prec 53 → setsign at
         * prec 4, RNDN, sign=1 — exercises the round step + new-sign
         * routing. */
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 5.0/3.0, MPFR_RNDN);
            emit_case(out, "mined", x, 1, 4, MPFR_RNDN);
            mpfr_clear(x);
        }
    }

    return 0;
}
