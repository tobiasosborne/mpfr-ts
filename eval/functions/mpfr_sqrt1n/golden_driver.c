/*
 * golden_driver.c -- Golden master for MPFR's mpfr_sqrt1n.
 *
 * Static helper in mpfr/src/sqrt.c L220-L346; exercised via public mpfr_sqrt
 * with prec(r) == GMP_NUMB_BITS (64) AND prec(u) <= GMP_NUMB_BITS. The
 * dispatcher routes here at mpfr/src/sqrt.c L569-L570. To force the route,
 * every emitted case has prec == 64 (the r-prec); u may have prec in [1, 64].
 * Singular inputs (NaN/Inf/zero/negative) are dispatched away by mpfr_sqrt
 * before reaching sqrt1n.
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
 * Ref: mpfr/src/sqrt.c L220-L346 -- C reference.
 * Ref: mpfr/src/sqrt.c L569-L570 -- dispatcher entry condition.
 * Ref: src/ops/sqrt.ts -- unified TS sqrt that the port delegates to.
 * Ref: mpfr/tests/tsqrt.c -- mined source.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_sqrt1n golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

/* Emit a single case at (u, prec=64, rnd). Asserts prec(u) in [1, 64]. */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr u, mpfr_rnd_t rnd) {
    const uint64_t prec = 64;
    const mpfr_prec_t uprec = mpfr_get_prec(u);
    assert(uprec >= 1 && uprec <= 64);
    assert(mpfr_regular_p(u));
    assert(mpfr_sgn(u) > 0);
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

/* Build u from a double at the given u-prec, then emit at output prec=64. */
static inline void emit_d(FILE *out, const char *tag,
                          double ud, uint64_t uprec, mpfr_rnd_t rnd) {
    assert(uprec >= 1 && uprec <= 64);
    mpfr_t u;
    mpfr_init2(u, (mpfr_prec_t)uprec); mpfr_set_d(u, ud, MPFR_RNDN);
    if (!mpfr_regular_p(u) || mpfr_sgn(u) <= 0) { mpfr_clear(u); return; }
    emit_case(out, tag, u, rnd);
    mpfr_clear(u);
}

/* Build u from a base-10 string at the given u-prec, then emit at prec=64. */
static inline void emit_s(FILE *out, const char *tag,
                          const char *us, uint64_t uprec, mpfr_rnd_t rnd) {
    assert(uprec >= 1 && uprec <= 64);
    mpfr_t u;
    mpfr_init2(u, (mpfr_prec_t)uprec); mpfr_set_str(u, us, 10, MPFR_RNDN);
    if (!mpfr_regular_p(u) || mpfr_sgn(u) <= 0) { mpfr_clear(u); return; }
    emit_case(out, tag, u, rnd);
    mpfr_clear(u);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 22 -- perfect squares + common values, u-prec at 64 mostly. */
    /* ============================================================== */
    emit_d(out, "happy", 4.0, 64, MPFR_RNDN);
    emit_d(out, "happy", 9.0, 64, MPFR_RNDN);
    emit_d(out, "happy", 16.0, 64, MPFR_RNDN);
    emit_d(out, "happy", 25.0, 64, MPFR_RNDN);
    emit_d(out, "happy", 100.0, 64, MPFR_RNDN);
    emit_d(out, "happy", 2.0, 64, MPFR_RNDN);
    emit_d(out, "happy", 3.0, 64, MPFR_RNDN);
    emit_d(out, "happy", 0.25, 64, MPFR_RNDN);
    emit_d(out, "happy", 0.5, 64, MPFR_RNDN);
    emit_d(out, "happy", 1.0, 64, MPFR_RNDN);
    emit_d(out, "happy", 49.0, 64, MPFR_RNDN);
    emit_d(out, "happy", 144.0, 64, MPFR_RNDN);
    emit_d(out, "happy", 10.0, 64, MPFR_RNDN);
    emit_d(out, "happy", 7.0, 64, MPFR_RNDU);
    emit_d(out, "happy", 7.0, 64, MPFR_RNDD);
    /* mixed u-prec < 64 (the dispatcher allows; the parity-low-bit
     * handling kicks in). */
    emit_d(out, "happy", 2.0, 53, MPFR_RNDN);
    emit_d(out, "happy", 3.0, 53, MPFR_RNDN);
    emit_d(out, "happy", 4.0, 24, MPFR_RNDN);
    emit_d(out, "happy", 1.5, 32, MPFR_RNDN);
    emit_d(out, "happy", 1234.5678, 53, MPFR_RNDN);
    emit_d(out, "happy", 1e10, 64, MPFR_RNDN);
    emit_d(out, "happy", 1e-10, 64, MPFR_RNDN);

    /* ============================================================== */
    /* edge: 32 -- prec(u) extremes, all 5 rnd modes, exponent parity,
     * the unequal-prec (u.prec < 64) coverage that distinguishes sqrt1n
     * from sqrt1. */
    /* ============================================================== */
    /* u-prec = 64 (matches r-prec), all 5 rnd modes on irrational. */
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 2.0, 64, RNDS[i]);
    /* u-prec = 1 (minimum), all 5 rnd modes -- exercises the prec(u) <
     * prec(r) low-bit-handling path. */
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 2.0, 1, RNDS[i]);
    /* u-prec = 53 (binary64), all 5 rnd modes. */
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 3.0, 53, RNDS[i]);
    /* u-prec = 32, all 5 rnd modes. */
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 5.0, 32, RNDS[i]);
    /* Even vs odd EXP(u). EXP(u=4.0)=3 (odd), EXP(u=8.0)=4 (even). */
    emit_d(out, "edge", 4.0, 64, MPFR_RNDN);
    emit_d(out, "edge", 8.0, 64, MPFR_RNDN);
    emit_d(out, "edge", 4.0, 53, MPFR_RNDN);
    emit_d(out, "edge", 8.0, 53, MPFR_RNDN);
    /* Subnormal-ish very small / large. */
    emit_d(out, "edge", 1e-100, 64, MPFR_RNDN);
    emit_d(out, "edge", 1e100, 64, MPFR_RNDN);
    /* String input -- full 64-bit precision irrational. */
    emit_s(out, "edge", "1.4142135623730950488", 64, MPFR_RNDN);
    emit_s(out, "edge", "0.5",                  64, MPFR_RNDN);
    /* Additional u-prec values to span the [1, 64] window evenly. */
    emit_d(out, "edge", 2.0, 16, MPFR_RNDN);
    emit_d(out, "edge", 2.0, 48, MPFR_RNDN);
    emit_d(out, "edge", 3.0, 7,  MPFR_RNDN);
    emit_d(out, "edge", 3.0, 47, MPFR_RNDU);

    /* ============================================================== */
    /* adversarial: 12 -- rounding-bit boundaries, ULP traps, the parity
     * fixup case (u.exp odd -> low bit saved). */
    /* ============================================================== */
    /* sqrt(2) at u-prec=64 across all rounding directions. */
    emit_d(out, "adversarial", 2.0, 64, MPFR_RNDN);
    emit_d(out, "adversarial", 2.0, 64, MPFR_RNDZ);
    emit_d(out, "adversarial", 2.0, 64, MPFR_RNDU);
    emit_d(out, "adversarial", 2.0, 64, MPFR_RNDD);
    emit_d(out, "adversarial", 2.0, 64, MPFR_RNDA);
    /* sqrt(3) at u-prec < 64 -- exercises the parity-low-bit case. */
    emit_d(out, "adversarial", 3.0, 30, MPFR_RNDN);
    emit_d(out, "adversarial", 3.0, 30, MPFR_RNDA);
    /* sqrt(0.1) at full prec. */
    emit_d(out, "adversarial", 0.1, 64, MPFR_RNDN);
    emit_d(out, "adversarial", 0.1, 64, MPFR_RNDU);
    /* Halfway-case neighbours for round-to-nearest. */
    emit_s(out, "adversarial", "1.5625",          64, MPFR_RNDN);
    emit_s(out, "adversarial", "1.5624999999999", 64, MPFR_RNDN);
    emit_s(out, "adversarial", "1.5625000000001", 64, MPFR_RNDN);

    /* ============================================================== */
    /* fuzz: 60 -- PRNG-driven; positive u; u-prec in [1, 64]; output
     * prec fixed at 64. */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x59C415C415C415CEULL);  /* SQRT1N seed */
        for (int rep = 0; rep < 60; ++rep) {
            /* u-prec uniform in [1, 64]. */
            const uint64_t uprec = 1 + xs64_below(&rng, 64);
            const uint64_t r = xs64_next(&rng);
            double ud = (double)(r % 200000ULL) / 1000.0 + 1e-5;
            if (ud <= 0.0) ud = 1.0;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            emit_d(out, "fuzz", ud, uprec, RNDS[rnd_idx]);
        }
    }

    /* ============================================================== */
    /* mined: 6 -- patterns from mpfr/tests/tsqrt.c at the prec=64
     * single-limb boundary. */
    /* ============================================================== */
    emit_d(out, "mined", 4.0, 64, MPFR_RNDN);
    emit_d(out, "mined", 2.0, 64, MPFR_RNDN);
    emit_d(out, "mined", 0.25, 64, MPFR_RNDN);
    emit_d(out, "mined", 1.0, 64, MPFR_RNDN);
    emit_d(out, "mined", 9.0, 53, MPFR_RNDN);  /* u-prec != 64 case */
    emit_d(out, "mined", 1.5, 64, MPFR_RNDU);

    return 0;
}
