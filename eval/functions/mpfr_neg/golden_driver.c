/*
 * golden_driver.c — Golden master for MPFR's mpfr_neg.
 *
 * C signature
 * -----------
 *
 *   int mpfr_neg(mpfr_t rop, mpfr_srcptr op, mpfr_rnd_t rnd);
 *
 *   Sets rop to -op, rounded per rnd to rop's precision; returns the
 *   ternary flag (sign of rounded - exact). Ref: mpfr/src/neg.c L24–L37.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS port `mpfr_neg(x, prec, rnd) -> Result` takes prec positionally and
 * returns the canonical {value, ternary} pair (src/core.ts L173–L176). To
 * grade we drive the C side at the SAME prec and emit Result-shaped
 * output via jl_output_result.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-record>,"prec":"<decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR-record>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25
 *   edge         :  ~50  (all kinds × all 5 rnd modes + signed zero +
 *                         ±Inf + same-prec round-trip)
 *   adversarial  :  ~30  (prec change forcing rounding; RNDN tie + sign
 *                         flip combination; prec<x.prec stresses the
 *                         new-sign-aware roundMantissa branch)
 *   fuzz         :   60  (PRNG seed 0xABEDA71BEDA71BEDULL — chosen for
 *                         hex-pun visibility; xs64_seed normalises if 0)
 *   mined        :   5   (transcribed from mpfr/tests/tabs.c — covers
 *                         neg in passing since mpfr_abs after neg is the
 *                         standard double-flip test)
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/neg.c — C reference (delegates to mpfr_set4 with -sign).
 * Ref: mpfr/src/set.c L25–L64 — mpfr_set4, the underlying primitive.
 * Ref: src/ops/neg.ts — the production port.
 * Ref: src/internal/mpfr/round_raw.ts — shared rounding substrate.
 */
#include "common.h"

#include <assert.h>
#include <float.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_neg golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Mirror src/core.ts PREC_MAX/PREC_MIN. */
#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Emit one mpfr_neg golden case.
 *
 *   1. mpfr_init2(rop, prec)             — target prec on a fresh probe.
 *   2. ternary = mpfr_neg(rop, x, rnd)   — the operation we mirror.
 *   3. emit {tag, inputs:{x, prec, rnd}, output:{value: rop, ternary}}.
 *
 * Timing brackets only the mpfr_neg call. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_neg(rop, x, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

/* Construct helpers for the four MPFR kinds. */
static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_d(x, d, MPFR_RNDN);
}
static inline void init_from_str_binary(mpfr_ptr x, const char *s,
                                        uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_str(x, s, 2, MPFR_RNDN);
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

/* Convenience: emit from a (double, source-prec, target-prec, rnd) tuple. */
static inline void emit_d(FILE *out, const char *tag,
                          double d, uint64_t srcp, uint64_t dstp,
                          mpfr_rnd_t rnd) {
    mpfr_t x; init_from_double(x, d, srcp);
    emit_case(out, tag, x, dstp, rnd);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25 cases — typical inputs, RNDN, same prec.            */
    /* ============================================================== */
    {
        /* Simple positive normals. Same prec → exact, ternary 0. */
        emit_d(out, "happy",  1.0,   53,  53, MPFR_RNDN);
        emit_d(out, "happy",  2.0,   53,  53, MPFR_RNDN);
        emit_d(out, "happy",  3.14,  53,  53, MPFR_RNDN);
        emit_d(out, "happy",  10.0,  53,  53, MPFR_RNDN);
        emit_d(out, "happy",  100.0, 53,  53, MPFR_RNDN);

        /* Negatives — flip back to positive. */
        emit_d(out, "happy", -1.0,   53,  53, MPFR_RNDN);
        emit_d(out, "happy", -2.0,   53,  53, MPFR_RNDN);
        emit_d(out, "happy", -3.14,  53,  53, MPFR_RNDN);
        emit_d(out, "happy", -10.0,  53,  53, MPFR_RNDN);
        emit_d(out, "happy", -100.0, 53,  53, MPFR_RNDN);

        /* Common precs. */
        emit_d(out, "happy",  3.14,  24,  24, MPFR_RNDN);
        emit_d(out, "happy",  3.14,  64,  64, MPFR_RNDN);
        emit_d(out, "happy",  3.14, 100, 100, MPFR_RNDN);
        emit_d(out, "happy",  3.14, 200, 200, MPFR_RNDN);

        /* Misc magnitudes. */
        emit_d(out, "happy",  1.5e100, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -1.5e100, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  1.5e-100, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -1.5e-100, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  6.022e23, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -6.022e23, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  1.0, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -1.0, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  2.718281828459045, 53, 53, MPFR_RNDN);
        emit_d(out, "happy", -2.718281828459045, 53, 53, MPFR_RNDN);
        emit_d(out, "happy",  1.4142135623730951, 53, 53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~50 cases — all 4 kinds × all 5 rnd modes, signed zero,  */
    /* ±Inf, NaN, same-prec round-trip across rounding modes.         */
    /* ============================================================== */
    {
        /* (1-5) NaN at all 5 rnd modes — result is NaN regardless. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_nan(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (6-10) +Inf at all 5 rnd modes — flip to -Inf. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_pos_inf(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (11-15) -Inf at all 5 rnd modes — flip to +Inf. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_neg_inf(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (16-20) +0 at all 5 rnd modes — flip to -0. The signed-zero
         * observability rule means a broken port that drops sign on
         * zero produces +0 here and fails. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_pos_zero(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (21-25) -0 at all 5 rnd modes — flip to +0. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_neg_zero(x, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (26-30) positive normal at all 5 rnd modes, same prec — flip
         * is exact regardless of mode. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_from_double(x, 3.14, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }
        /* (31-35) negative normal at all 5 rnd modes, same prec. */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_from_double(x, -3.14, 53);
            emit_case(out, "edge", x, 53, RNDS[i]);
            mpfr_clear(x);
        }

        /* (36) Inf with prec mismatch (prec change is irrelevant for
         * Inf — should still flip cleanly). */
        {
            mpfr_t x; init_pos_inf(x, 64);
            emit_case(out, "edge", x, 200, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* (37) +0 with prec mismatch. */
        {
            mpfr_t x; init_pos_zero(x, 64);
            emit_case(out, "edge", x, 200, MPFR_RNDD);
            mpfr_clear(x);
        }
        /* (38) -0 with prec mismatch. */
        {
            mpfr_t x; init_neg_zero(x, 64);
            emit_case(out, "edge", x, 200, MPFR_RNDU);
            mpfr_clear(x);
        }

        /* (39-43) Same-prec round-trip at prec=1 (boundary). */
        for (int i = 0; i < 5; ++i) {
            mpfr_t x; init_from_double(x, 1.0, 1);
            emit_case(out, "edge", x, 1, RNDS[i]);
            mpfr_clear(x);
        }

        /* (44) Same-prec at very large prec. */
        {
            mpfr_t x; init_from_double(x, 1.5, 200);
            emit_case(out, "edge", x, 200, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* (45) Same-prec at small prec=2. */
        {
            mpfr_t x; init_from_double(x, 1.5, 2);
            emit_case(out, "edge", x, 2, MPFR_RNDN);
            mpfr_clear(x);
        }

        /* (46-50) Mixed prec — small-to-large lossless pad for both
         * positive and negative; differs from the same-prec branch in
         * the production port (the padShift step). */
        emit_d(out, "edge",  1.0,  10,  53, MPFR_RNDN);
        emit_d(out, "edge", -1.0,  10,  53, MPFR_RNDN);
        emit_d(out, "edge",  1.5,  53, 100, MPFR_RNDN);
        emit_d(out, "edge", -1.5,  53, 100, MPFR_RNDN);
        emit_d(out, "edge",  3.0,   3, 200, MPFR_RNDD);
    }

    /* ============================================================== */
    /* adversarial: ~30 cases — prec < x.prec forces rounding; ALL 5  */
    /* rnd modes; cross-magnitude mantissas that distinguish RNDU/RNDD */
    /* on flipped sign (the subtle new-sign-direction case).          */
    /* ============================================================== */
    {
        /* Inexact-rounding pattern from set_d source mantissas. A 5-bit
         * pattern 11011 truncates differently per rnd mode and per
         * resulting (new) sign. Construct from a string for total
         * control over the source bits. */
        const char *patterns[] = {
            "1.1011E0",     /* 1.6875 — 5-bit MSB-aligned */
            "1.0101E0",     /* 1.3125 */
            "1.1111E0",     /* 1.9375 — RNDA carry-out candidate */
            "1.0001E10",    /* large exponent + low-bit pattern */
            "1.1100E-50",   /* small exponent + truncation tail */
            "1.0000000000000000000000000000000000000000000000000001E0", /* 1+ulp */
        };
        const size_t n_pat = sizeof(patterns) / sizeof(patterns[0]);

        /* For each pattern: src prec = 53, target prec = 3 (drops many
         * bits); both positive and negative source; all 5 rounding modes.
         * That's 6 patterns × 2 signs × 5 modes = 60 cases — we keep half
         * of them (positives only) to stay near ~30. */
        for (size_t p = 0; p < n_pat; ++p) {
            for (int r = 0; r < 5; ++r) {
                mpfr_t x;
                mpfr_init2(x, 53);
                mpfr_set_str(x, patterns[p], 2, MPFR_RNDN);
                emit_case(out, "adversarial", x, 3, RNDS[r]);
                mpfr_clear(x);
            }
        }

        /* RNDN tie boundary: source mantissa with the dropped half-bit
         * exactly at the rounding boundary. At prec=2 truncation 1.10
         * = 1.5; dropped half-bit boundary at the tie. */
        {
            /* 1.1100E0 = 1.75: tie between 1.10 (=1.5) and 10.0 (=2.0)
             * — RNDN ties-to-even chooses 10.0 (LSB 0). */
            mpfr_t x; mpfr_init2(x, 4); mpfr_set_str(x, "1.110E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x, 2, MPFR_RNDN);
            mpfr_clear(x);
        }
        {
            /* Same value negative — RNDN ties-to-even still chooses the
             * even mantissa, but ternary direction inverts. This is the
             * exact case where the new-sign-aware rounding shows up. */
            mpfr_t x; mpfr_init2(x, 4); mpfr_set_str(x, "-1.110E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x, 2, MPFR_RNDN);
            mpfr_clear(x);
        }

        /* Carry-out at MSB: 1.1111 at prec=4 rounds under RNDU up to
         * 10.000 (= 2^1), causing the mantissa to renormalise. */
        {
            mpfr_t x; mpfr_init2(x, 4); mpfr_set_str(x, "1.111E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x, 3, MPFR_RNDU);
            mpfr_clear(x);
        }
        {
            /* Negative source: source -1.111 (= -1.875); neg(-1.875) = +1.875;
             * round to prec=3 under RNDU → +10.00 (carry-out). */
            mpfr_t x; mpfr_init2(x, 4); mpfr_set_str(x, "-1.111E0", 2, MPFR_RNDN);
            emit_case(out, "adversarial", x, 3, MPFR_RNDU);
            mpfr_clear(x);
        }
    }

    /* ============================================================== */
    /* fuzz: 60 cases — PRNG-driven                                   */
    /* ============================================================== */
    {
        /* Seed picked for hex-pun visibility (ABED A71BED A71BED).
         * xs64_seed substitutes a multiplier on zero, so the constant
         * being non-zero is the only correctness requirement. */
        xs64_t rng;
        xs64_seed(&rng, 0xABEDA71BEDA71BEDULL);
        const uint64_t precs[6] = { 1, 2, 53, 64, 100, 200 };

        int emitted = 0;
        while (emitted < 60) {
            const uint64_t bits = xs64_next(&rng);

            /* Reject NaN/Inf bit patterns (exp == 0x7FF). NaN is covered
             * in edge; Inf is too. We want fuzz dense on normals and
             * subnormals + ±0. */
            const uint64_t exp = (bits >> 52) & 0x7FF;
            if (exp == 0x7FF) continue;

            double d;
            memcpy(&d, &bits, sizeof d);

            const uint64_t srcp = precs[xs64_below(&rng, 6)];
            const uint64_t dstp = precs[xs64_below(&rng, 6)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];

            mpfr_t x;
            init_from_double(x, d, srcp);
            emit_case(out, "fuzz", x, dstp, rnd);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — from mpfr/tests/tabs.c (neg in passing)        */
    /* ============================================================== */
    {
        /* mpfr/tests/tabs.c L75–L77: neg(NaN) = NaN, ternary 0.
         * Same shape as edge (1), kept for tag-class coverage. */
        {
            mpfr_t x; init_nan(x, 53);
            emit_case(out, "mined", x, 53, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* mpfr/tests/tneg.c L52–L55: neg(0) = 0 with flipped sign. */
        {
            mpfr_t x; init_pos_zero(x, 53);
            emit_case(out, "mined", x, 53, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* tneg.c — neg(+Inf) = -Inf. */
        {
            mpfr_t x; init_pos_inf(x, 53);
            emit_case(out, "mined", x, 53, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* tneg.c — neg(1.0) = -1.0. */
        {
            mpfr_t x; init_from_double(x, 1.0, 53);
            emit_case(out, "mined", x, 53, MPFR_RNDN);
            mpfr_clear(x);
        }
        /* tneg.c — neg with prec change rounding: 5/3 at prec 53 →
         * neg at prec 4, RNDN exercises the round step. */
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 5.0/3.0, MPFR_RNDN);
            emit_case(out, "mined", x, 4, MPFR_RNDN);
            mpfr_clear(x);
        }
    }

    return 0;
}
