/*
 * golden_driver.c -- Golden master for MPFR's mpfr_div_2.
 *
 * Static helper in mpfr/src/div.c L390-L640; exercised via public mpfr_div
 * with prec(q) == prec(u) == prec(v) AND GMP_NUMB_BITS < prec < 2*GMP_NUMB_BITS
 * (i.e. 65 <= prec <= 127 on a 64-bit GMP). The dispatcher routes here at
 * mpfr/src/div.c L841-L843. To force the route, every emitted case has
 * prec in [65, 127] and all three operand precisions equal.
 *
 * Tag distribution: happy 22, edge 30, adversarial 12, fuzz 60, mined 5.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"u":<MPFR>,"v":<MPFR>,"prec":"<dec>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Ref: mpfr/src/div.c L390-L640 -- C reference.
 * Ref: mpfr/src/div.c L841-L843 -- dispatcher entry conditions.
 * Ref: src/ops/div_2.ts -- production port (not yet written).
 * Ref: mpfr/tests/tdiv.c -- mined source.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <math.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_div_2 golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

/* Emit a single case at (u, v, prec, rnd). Asserts prec in (64, 128). */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr u, mpfr_srcptr v,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec > 64 && prec < 128);
    assert(mpfr_get_prec(u) == (mpfr_prec_t)prec);
    assert(mpfr_get_prec(v) == (mpfr_prec_t)prec);
    assert(mpfr_regular_p(u) && mpfr_regular_p(v));
    mpfr_t q; mpfr_init2(q, (mpfr_prec_t)prec);
    const uint64_t t0 = now_ns();
    const int ternary = mpfr_div(q, u, v, rnd);
    const uint64_t elapsed = now_ns() - t0;
    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "u", u);
    jl_kv_mpfr(out, 0, "v", v);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, q, ternary);
    jl_finish(out, elapsed);
    mpfr_clear(q);
}

/* Build u, v from doubles at the given prec, then emit. Skips if either
 * input rounds to a singular value (since mpfr_div_2 only sees regulars). */
static inline void emit_dd(FILE *out, const char *tag,
                           double ud, double vd, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t u, v;
    mpfr_init2(u, (mpfr_prec_t)prec); mpfr_set_d(u, ud, MPFR_RNDN);
    mpfr_init2(v, (mpfr_prec_t)prec); mpfr_set_d(v, vd, MPFR_RNDN);
    if (!mpfr_regular_p(u) || !mpfr_regular_p(v)) { mpfr_clear(u); mpfr_clear(v); return; }
    emit_case(out, tag, u, v, prec, rnd);
    mpfr_clear(u); mpfr_clear(v);
}

/* Build u, v from base-10 strings at the given prec. Useful when the
 * input value needs more than 53 bits to express. */
static inline void emit_ss(FILE *out, const char *tag,
                           const char *us, const char *vs,
                           uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t u, v;
    mpfr_init2(u, (mpfr_prec_t)prec); mpfr_set_str(u, us, 10, MPFR_RNDN);
    mpfr_init2(v, (mpfr_prec_t)prec); mpfr_set_str(v, vs, 10, MPFR_RNDN);
    if (!mpfr_regular_p(u) || !mpfr_regular_p(v)) { mpfr_clear(u); mpfr_clear(v); return; }
    emit_case(out, tag, u, v, prec, rnd);
    mpfr_clear(u); mpfr_clear(v);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 22 -- common ratios at typical 2-limb precisions. */
    /* ============================================================== */
    emit_dd(out, "happy", 6.0, 2.0, 65, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 4.0, 80, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 3.0, 96, MPFR_RNDN);
    emit_dd(out, "happy", -6.0, 2.0, 100, MPFR_RNDN);
    emit_dd(out, "happy", -6.0, -2.0, 113, MPFR_RNDN);   /* IEEE-754 binary128 */
    emit_dd(out, "happy", 1.0, 1.0, 127, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 2.0, 65, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 7.0, 80, MPFR_RNDU);
    emit_dd(out, "happy", 22.0, 7.0, 113, MPFR_RNDN);
    emit_dd(out, "happy", 2.0, 2.0, 65, MPFR_RNDN);
    emit_dd(out, "happy", -1.0, -1.0, 96, MPFR_RNDN);
    emit_dd(out, "happy", 7.0, 11.0, 100, MPFR_RNDN);
    emit_dd(out, "happy", 0.1, 10.0, 113, MPFR_RNDN);
    emit_dd(out, "happy", 1e5, 1e3, 80, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, -1.0, 65, MPFR_RNDN);
    emit_dd(out, "happy", -1.0, 2.0, 96, MPFR_RNDD);
    emit_dd(out, "happy", 100.5, 0.25, 100, MPFR_RNDA);
    emit_dd(out, "happy", -100.5, 0.25, 113, MPFR_RNDA);
    emit_dd(out, "happy", 1.5, 1.5, 65, MPFR_RNDU);
    emit_dd(out, "happy", 1.5, 1.5, 127, MPFR_RNDD);
    emit_dd(out, "happy", 1.0, 1.0/3.0, 100, MPFR_RNDN);
    emit_dd(out, "happy", 1.0, 1.0/7.0, 80, MPFR_RNDN);

    /* ============================================================== */
    /* edge: 30 -- prec extremes (65/66/127), all 5 rnd modes, plus
     * dispatcher boundary cases. */
    /* ============================================================== */
    /* prec=65: lowest dispatch-valid value. */
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.0, 1.0, 65, RNDS[i]);
    /* prec=127: highest dispatch-valid value. */
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.0, 1.0, 127, RNDS[i]);
    /* prec=66 / 96 / 113 boundary samples. */
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.0, 3.0, 96, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 22.0, 7.0, 113, RNDS[i]);
    /* Large-magnitude operands at top prec. */
    emit_dd(out, "edge", 1e10, 1e-10, 127, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 1e100, 100, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 1e-100, 80, MPFR_RNDN);
    /* Hi-prec quotient (need set_str for inputs that exceed 53 bits). */
    emit_ss(out, "edge", "0.99999999999999999999999999999999999999",
                          "1.00000000000000000000000000000000000001",
                          100, MPFR_RNDN);
    /* Near-1 quotient (the dispatcher's hardest case: round-bit hits). */
    emit_dd(out, "edge", 7.0, 3.0, 80, MPFR_RNDN);
    /* Bottom and top of the dispatch window, adjacent. */
    emit_dd(out, "edge", 1.0, 7.0, 65, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 7.0, 127, MPFR_RNDN);
    /* prec = exactly halfway in the window. */
    emit_dd(out, "edge", 1.0, 11.0, 96, MPFR_RNDN);
    emit_dd(out, "edge", 1.0, 11.0, 97, MPFR_RNDU);
    emit_dd(out, "edge", 1.0, 11.0, 95, MPFR_RNDD);

    /* ============================================================== */
    /* adversarial: 12 -- rounding boundaries, ULP traps, cancellation
     * neighbours in the 2-limb window. */
    /* ============================================================== */
    emit_dd(out, "adversarial", 1.0, 3.0, 65, MPFR_RNDU);
    emit_dd(out, "adversarial", 1.0, 3.0, 65, MPFR_RNDD);
    emit_dd(out, "adversarial", -1.0, 3.0, 96, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 7.0, 100, MPFR_RNDU);
    emit_dd(out, "adversarial", 1.0, 7.0, 100, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 7.0, 113, MPFR_RNDA);
    emit_dd(out, "adversarial", 1.0, 11.0, 65, MPFR_RNDN);
    emit_dd(out, "adversarial", 0.999, 1.001, 127, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.001, 0.999, 127, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.0, 9.0, 80, MPFR_RNDN);
    emit_dd(out, "adversarial", -1.0, 9.0, 96, MPFR_RNDD);
    emit_dd(out, "adversarial", 1.0, 13.0, 113, MPFR_RNDU);

    /* ============================================================== */
    /* fuzz: 60 -- PRNG-driven; bounded so v != 0; prec in [65, 127]. */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xD12D12D12D12D12DULL);  /* DIV2 seed */
        for (int rep = 0; rep < 60; ++rep) {
            /* prec uniform in [65, 127] (window is 63 wide). */
            const uint64_t prec = 65 + xs64_below(&rng, 63);
            const uint64_t r1 = xs64_next(&rng);
            double ud = ((double)(r1 % 200000ULL) - 100000.0) / 100.0;
            const uint64_t r2 = xs64_next(&rng);
            double vd = ((double)((r2 % 199998ULL) + 1) - 99999.0) / 100.0;
            if (ud == 0.0) ud = 1.0;
            if (vd == 0.0) vd = 1.0;
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            emit_dd(out, "fuzz", ud, vd, prec, RNDS[rnd_idx]);
        }
    }

    /* ============================================================== */
    /* mined: 5 -- patterns from mpfr/tests/tdiv.c shape (integer
     * ratios + irrational quotients) at 2-limb precs. */
    /* ============================================================== */
    emit_dd(out, "mined", 6.0, 2.0, 100, MPFR_RNDN);
    emit_dd(out, "mined", 1.0, 3.0, 113, MPFR_RNDU);
    emit_dd(out, "mined", 1.5, 1.5, 80, MPFR_RNDN);
    emit_dd(out, "mined", -1.0, 2.0, 96, MPFR_RNDN);
    emit_dd(out, "mined", 22.0, 7.0, 127, MPFR_RNDN);

    return 0;
}
