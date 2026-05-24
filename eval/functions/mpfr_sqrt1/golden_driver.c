/*
 * golden_driver.c -- Golden master for MPFR's mpfr_sqrt1.
 *
 * Static helper in mpfr/src/sqrt.c L74-L217; exercised via public mpfr_sqrt
 * with prec(r) == prec(u) AND 1 <= prec < GMP_NUMB_BITS (i.e. 1 <= prec <= 63
 * on a 64-bit GMP). The dispatcher routes here at mpfr/src/sqrt.c L563-L564.
 * To force the route, every emitted case has matching u-prec and r-prec, both
 * in [1, 63]. Singular inputs (NaN/Inf/zero/negative) are dispatched away by
 * mpfr_sqrt before reaching sqrt1, so this driver emits only regular positive
 * inputs.
 *
 * Tag distribution: happy 22, edge 32 (20 loop + 12 direct), adversarial 12, fuzz 60, mined 6.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"u":<MPFR>,"prec":"<dec>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Ref: mpfr/src/sqrt.c L74-L217 -- C reference.
 * Ref: mpfr/src/sqrt.c L561-L564 -- dispatcher entry conditions.
 * Ref: src/ops/sqrt.ts -- unified TS sqrt that the port delegates to.
 * Ref: mpfr/tests/tsqrt.c -- mined source.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sqrt1 golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

/* Emit a single case at (u, prec, rnd). Asserts prec in [1, 63] and prec(u) == prec. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr u, uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= 1 && prec < 64);
    assert(mpfr_get_prec(u) == (mpfr_prec_t)prec);
    assert(mpfr_regular_p(u));
    assert(mpfr_sgn(u) > 0);  /* positive only -- dispatcher filters negatives */
    mpfr_t r; mpfr_init2(r, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_sqrt(r, u, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "u", u);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, r, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(r);
}

/* Build u from a double at the given prec, then emit. Skips if u rounds to a
 * singular/non-positive value. */
static inline void emit_d(FILE *out, const char *tag,
                          double ud, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t u;
    mpfr_init2(u, (mpfr_prec_t)prec); mpfr_set_d(u, ud, MPFR_RNDN);
    if (!mpfr_regular_p(u) || mpfr_sgn(u) <= 0) { mpfr_clear(u); return; }
    emit_case(out, tag, u, prec, rnd);
    mpfr_clear(u);
}

/* Build u from a base-10 string at the given prec. Useful for fine-grained
 * inputs that exceed double precision. */
static inline void emit_s(FILE *out, const char *tag,
                          const char *us, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t u;
    mpfr_init2(u, (mpfr_prec_t)prec); mpfr_set_str(u, us, 10, MPFR_RNDN);
    if (!mpfr_regular_p(u) || mpfr_sgn(u) <= 0) { mpfr_clear(u); return; }
    emit_case(out, tag, u, prec, rnd);
    mpfr_clear(u);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 22 -- perfect squares + common values across the prec window. */
    /* ============================================================== */
    emit_d(out, "happy", 4.0, 24, MPFR_RNDN);    /* perfect square 4 -> 2 */
    emit_d(out, "happy", 9.0, 24, MPFR_RNDN);    /* 9 -> 3 */
    emit_d(out, "happy", 16.0, 24, MPFR_RNDN);   /* 16 -> 4 */
    emit_d(out, "happy", 25.0, 53, MPFR_RNDN);   /* 25 -> 5 at IEEE 754 binary64 prec */
    emit_d(out, "happy", 100.0, 53, MPFR_RNDN);  /* 100 -> 10 */
    emit_d(out, "happy", 2.0, 53, MPFR_RNDN);    /* sqrt(2) at binary64 prec */
    emit_d(out, "happy", 3.0, 53, MPFR_RNDN);    /* sqrt(3) at binary64 prec */
    emit_d(out, "happy", 0.25, 24, MPFR_RNDN);   /* 0.25 -> 0.5 (perfect) */
    emit_d(out, "happy", 0.5, 53, MPFR_RNDN);    /* sqrt(0.5) inexact */
    emit_d(out, "happy", 1.0, 1, MPFR_RNDN);     /* 1 -> 1 at lowest prec */
    emit_d(out, "happy", 1.0, 32, MPFR_RNDN);    /* 1 -> 1 mid-prec */
    emit_d(out, "happy", 1.0, 63, MPFR_RNDN);    /* 1 -> 1 max-prec */
    emit_d(out, "happy", 49.0, 24, MPFR_RNDN);   /* 49 -> 7 */
    emit_d(out, "happy", 144.0, 53, MPFR_RNDN);  /* 144 -> 12 */
    emit_d(out, "happy", 10.0, 53, MPFR_RNDN);   /* sqrt(10) */
    emit_d(out, "happy", 7.0, 53, MPFR_RNDU);    /* sqrt(7) RNDU */
    emit_d(out, "happy", 7.0, 53, MPFR_RNDD);    /* sqrt(7) RNDD */
    emit_d(out, "happy", 2.0, 24, MPFR_RNDN);    /* sqrt(2) low prec */
    emit_d(out, "happy", 2.0, 63, MPFR_RNDN);    /* sqrt(2) high prec */
    emit_d(out, "happy", 1e10, 53, MPFR_RNDN);   /* large magnitude */
    emit_d(out, "happy", 1e-10, 53, MPFR_RNDN);  /* small magnitude */
    emit_d(out, "happy", 1234.5678, 53, MPFR_RNDN);

    /* ============================================================== */
    /* edge: 35 -- prec extremes (1, 2, 53, 62, 63), all 5 rnd modes,
     * exponent parity coverage (even/odd EXP(u)), boundary precs. */
    /* ============================================================== */
    /* prec=1: minimum dispatch-valid value, 1 bit of mantissa. */
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.0, 1, RNDS[i]);
    /* prec=2: tiny mantissa where rounding matters. */
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 2.0, 2, RNDS[i]);
    /* prec=63: maximum dispatch-valid value, just below GMP_NUMB_BITS. */
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 2.0, 63, RNDS[i]);
    /* prec=53 (IEEE 754 binary64) all rnd modes on a non-perfect square. */
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 3.0, 53, RNDS[i]);
    /* Even vs odd EXP(u): EXP(u=4.0)=3 (odd), EXP(u=8.0)=4 (even). */
    emit_d(out, "edge", 4.0, 24, MPFR_RNDN);
    emit_d(out, "edge", 8.0, 24, MPFR_RNDN);
    emit_d(out, "edge", 4.0, 24, MPFR_RNDZ);
    emit_d(out, "edge", 8.0, 24, MPFR_RNDZ);
    /* Subnormal-ish very small numbers at high prec. */
    emit_d(out, "edge", 1e-100, 53, MPFR_RNDN);
    emit_d(out, "edge", 1e-200, 53, MPFR_RNDN);
    /* Very large at high prec. */
    emit_d(out, "edge", 1e100, 53, MPFR_RNDN);
    emit_d(out, "edge", 1e200, 53, MPFR_RNDN);
    /* Near-1 (small ratio to 1.0). */
    emit_d(out, "edge", 1.0000000001, 53, MPFR_RNDN);
    /* String input at high prec for fine-grained boundaries. */
    emit_s(out, "edge", "1.4142135623730950488016887242096980785696718753769",
                        53, MPFR_RNDN);
    /* Just above and below a perfect-square boundary. */
    emit_d(out, "edge", 0.9999, 53, MPFR_RNDN);
    emit_d(out, "edge", 1.0001, 53, MPFR_RNDN);

    /* ============================================================== */
    /* adversarial: 12 -- rounding-bit boundaries, ULP traps. */
    /* ============================================================== */
    /* sqrt(2) at low prec across all rounding directions (inexact). */
    emit_d(out, "adversarial", 2.0, 8, MPFR_RNDN);
    emit_d(out, "adversarial", 2.0, 8, MPFR_RNDZ);
    emit_d(out, "adversarial", 2.0, 8, MPFR_RNDU);
    emit_d(out, "adversarial", 2.0, 8, MPFR_RNDD);
    emit_d(out, "adversarial", 2.0, 8, MPFR_RNDA);
    /* sqrt(3) at boundary precs. */
    emit_d(out, "adversarial", 3.0, 16, MPFR_RNDN);
    emit_d(out, "adversarial", 3.0, 16, MPFR_RNDA);
    /* sqrt(0.1) -- 0.1 is itself inexact in binary. */
    emit_d(out, "adversarial", 0.1, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 0.1, 53, MPFR_RNDU);
    /* Values close to a halfway-case for round-to-nearest. */
    emit_s(out, "adversarial", "1.5625", 53, MPFR_RNDN);   /* sqrt(1.5625) = 1.25 exact */
    emit_s(out, "adversarial", "1.5624999999999", 53, MPFR_RNDN);
    emit_s(out, "adversarial", "1.5625000000001", 53, MPFR_RNDN);

    /* ============================================================== */
    /* fuzz: 60 -- PRNG-driven; bounded positive u; prec in [1, 63]. */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x59C415C415C415C4ULL);  /* SQRT1 seed (SQRT->5917) */
        for (int rep = 0; rep < 60; ++rep) {
            /* prec uniform in [1, 63]. */
            const uint64_t prec = 1 + xs64_below(&rng, 63);
            const uint64_t r = xs64_next(&rng);
            /* Generate u in roughly [1e-5, 1e5] (positive only). */
            double ud = (double)(r % 200000ULL) / 1000.0 + 1e-5;
            if (ud <= 0.0) ud = 1.0;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            emit_d(out, "fuzz", ud, prec, RNDS[rnd_idx]);
        }
    }

    /* ============================================================== */
    /* mined: 6 -- patterns from mpfr/tests/tsqrt.c (perfect squares,
     * irrational roots, prec-boundary samples). */
    /* ============================================================== */
    emit_d(out, "mined", 4.0, 53, MPFR_RNDN);    /* tsqrt.c basic */
    emit_d(out, "mined", 2.0, 53, MPFR_RNDN);    /* irrational */
    emit_d(out, "mined", 0.25, 53, MPFR_RNDN);   /* small perfect square */
    emit_d(out, "mined", 1.0, 53, MPFR_RNDN);    /* identity */
    emit_d(out, "mined", 9.0, 24, MPFR_RNDN);    /* low-prec perfect square */
    emit_d(out, "mined", 1.5, 53, MPFR_RNDU);    /* arbitrary directional */

    return 0;
}
