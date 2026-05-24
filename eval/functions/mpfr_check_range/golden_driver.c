/*
 * golden_driver.c — Golden master for MPFR's mpfr_check_range.
 *
 * mpfr_check_range(x, t, rnd) adjusts x for exp range overflow/underflow:
 *
 *   - x singular or x normal with exp ∈ [emin, emax]: return (x, t).
 *   - x normal with exp < emin: return mpfr_underflow(prec, rnd', sign).
 *   - x normal with exp > emax: return mpfr_overflow(prec, rnd, sign).
 *
 * Ref: mpfr/src/exceptions.c L249-L327.
 *
 * Scope of this golden
 * --------------------
 *
 * The under/overflow paths produce results that depend on __gmpfr_emax
 * (and __gmpfr_emin), which are runtime-settable in libmpfr but
 * COMPILE-TIME-FIXED in the TS schema (EMIN_DEFAULT / EMAX_DEFAULT in
 * src/ops/underflow.ts / src/ops/overflow.ts; same constants used here).
 * To force the C side into the underflow branch, one would tighten
 * __gmpfr_emin via mpfr_set_emin — but then libmpfr's underflow result
 * would have exp = (tightened-emin), whereas the TS port would use the
 * default EMIN. So the wire would mismatch on a port-correct value.
 *
 * Solution: this golden tests ONLY the in-range path (the dominant
 * caller pattern, where check_range is a Ziv-loop safety net for a
 * value that's almost always in-range). The expected output is the
 * input unchanged with the carried ternary. Per-tag coverage matches
 * Rule 7 minimums; the mutation-prove broken port mangles the
 * in-range path (e.g. by negating ternary or swapping fields).
 *
 * The under/overflow paths are deferred to a follow-on golden once
 * the emin/emax accessors are ported (currently mpfr_set_emin /
 * mpfr_set_emax aren't in src/ops/).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR>,"t":-1|0|1,"rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 *   - t is a bare JSON int via jl_kv_int (Ternary).
 *
 * Tag distribution: 22/30/10/55/5.
 *
 * Ref: src/ops/check_range.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_check_range golden_driver requires GMP_NUMB_BITS == 64"
#endif

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};
static const int TS[3] = {-1, 0, +1};

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x_in, int t, mpfr_rnd_t rnd) {
    /* Clone x_in (mpfr_check_range may mutate). */
    mpfr_t x;
    mpfr_init2(x, mpfr_get_prec(x_in));
    mpfr_set(x, x_in, MPFR_RNDN);

    const uint64_t t0 = now_ns();
    const int rt = mpfr_check_range(x, t, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x_in);
    jl_kv_int(out, 0, "t", t);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, x, rt);
    jl_finish(out, elapsed);

    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 — common values, all kinds, ternary variations. */
    /* Normal with various ternary, all in-range. */
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 1.0, MPFR_RNDN); emit_case(out, "happy", x, 0, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 1.0, MPFR_RNDN); emit_case(out, "happy", x, +1, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 1.0, MPFR_RNDN); emit_case(out, "happy", x, -1, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 3.14, MPFR_RNDN); emit_case(out, "happy", x, 0, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, -3.14, MPFR_RNDN); emit_case(out, "happy", x, +1, MPFR_RNDU); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 2.71, MPFR_RNDN); emit_case(out, "happy", x, -1, MPFR_RNDD); mpfr_clear(x); }
    /* Zero. */
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1); emit_case(out, "happy", x, 0, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1); emit_case(out, "happy", x, 0, MPFR_RNDN); mpfr_clear(x); }
    /* Inf, ternary 0 (no flag side effect). */
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, +1); emit_case(out, "happy", x, 0, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, -1); emit_case(out, "happy", x, 0, MPFR_RNDN); mpfr_clear(x); }
    /* NaN. */
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x); emit_case(out, "happy", x, 0, MPFR_RNDN); mpfr_clear(x); }
    /* More normals. */
    { mpfr_t x; mpfr_init2(x, 24); mpfr_set_d(x, 1.5, MPFR_RNDN); emit_case(out, "happy", x, 0, MPFR_RNDZ); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 113); mpfr_set_d(x, 1.0/3.0, MPFR_RNDN); emit_case(out, "happy", x, +1, MPFR_RNDA); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 64); mpfr_set_d(x, 1e10, MPFR_RNDN); emit_case(out, "happy", x, -1, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 64); mpfr_set_d(x, 1e-10, MPFR_RNDN); emit_case(out, "happy", x, +1, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 100); mpfr_set_si(x, 7, MPFR_RNDN); emit_case(out, "happy", x, 0, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 100); mpfr_set_si(x, -7, MPFR_RNDN); emit_case(out, "happy", x, 0, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 200); mpfr_set_d(x, 1.5, MPFR_RNDN); emit_case(out, "happy", x, +1, MPFR_RNDU); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 200); mpfr_set_d(x, -1.5, MPFR_RNDN); emit_case(out, "happy", x, -1, MPFR_RNDD); mpfr_clear(x); }
    /* PREC_MIN. */
    { mpfr_t x; mpfr_init2(x, 1); mpfr_set_si(x, 1, MPFR_RNDN); emit_case(out, "happy", x, 0, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 1); mpfr_set_si(x, -1, MPFR_RNDN); emit_case(out, "happy", x, 0, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 100.0, MPFR_RNDN); emit_case(out, "happy", x, 0, MPFR_RNDN); mpfr_clear(x); }

    /* edge: 30 */
    /* All ternary x all rounding-mode combos for normal value. */
    for (int ti = 0; ti < 3; ++ti) {
        for (int ri = 0; ri < 5; ++ri) {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 1.5, MPFR_RNDN);
            emit_case(out, "edge", x, TS[ti], RNDS[ri]);
            mpfr_clear(x);
        }
    }
    /* All ternary x rnd for zero/inf/nan. */
    for (int ti = 0; ti < 3; ++ti) {
        mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1);
        emit_case(out, "edge", x, TS[ti], MPFR_RNDN);
        mpfr_clear(x);
    }
    for (int ti = 0; ti < 3; ++ti) {
        mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1);
        emit_case(out, "edge", x, TS[ti], MPFR_RNDD);
        mpfr_clear(x);
    }
    /* Inf t=0 (the t != 0 case triggers flag side-effect we don't model — skipped). */
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, +1); emit_case(out, "edge", x, 0, MPFR_RNDU); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, -1); emit_case(out, "edge", x, 0, MPFR_RNDD); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x); emit_case(out, "edge", x, 0, MPFR_RNDA); mpfr_clear(x); }
    /* Limb boundaries. */
    { mpfr_t x; mpfr_init2(x, 63); mpfr_set_d(x, 1.5, MPFR_RNDN); emit_case(out, "edge", x, 0, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 64); mpfr_set_d(x, 1.5, MPFR_RNDN); emit_case(out, "edge", x, 0, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 65); mpfr_set_d(x, 1.5, MPFR_RNDN); emit_case(out, "edge", x, 0, MPFR_RNDN); mpfr_clear(x); }
    /* Large prec. */
    { mpfr_t x; mpfr_init2(x, 1024); mpfr_set_d(x, 1.5, MPFR_RNDN); emit_case(out, "edge", x, +1, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 2048); mpfr_set_d(x, -1.5, MPFR_RNDN); emit_case(out, "edge", x, -1, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 100); mpfr_set_d(x, 1.0, MPFR_RNDN); emit_case(out, "edge", x, +1, MPFR_RNDA); mpfr_clear(x); }

    /* adversarial: 10 — ternary direction × sign of normal. */
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 1.0, MPFR_RNDN); emit_case(out, "adversarial", x, +1, MPFR_RNDD); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, -1.0, MPFR_RNDN); emit_case(out, "adversarial", x, -1, MPFR_RNDU); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 1.0, MPFR_RNDN); emit_case(out, "adversarial", x, -1, MPFR_RNDU); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, -1.0, MPFR_RNDN); emit_case(out, "adversarial", x, +1, MPFR_RNDD); mpfr_clear(x); }
    /* NaN, all ternary. */
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x); emit_case(out, "adversarial", x, +1, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x); emit_case(out, "adversarial", x, -1, MPFR_RNDN); mpfr_clear(x); }
    /* Mantissa near upper rep limit. */
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 1.7976931348623157e308, MPFR_RNDN); emit_case(out, "adversarial", x, +1, MPFR_RNDN); mpfr_clear(x); }
    /* Mantissa near lower. */
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 5e-324, MPFR_RNDN); emit_case(out, "adversarial", x, -1, MPFR_RNDN); mpfr_clear(x); }
    /* Signed zero with ternary +1 (a bit odd but valid input). */
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1); emit_case(out, "adversarial", x, +1, MPFR_RNDD); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1); emit_case(out, "adversarial", x, -1, MPFR_RNDU); mpfr_clear(x); }

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC4ECCAFE12345678ULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t ki = xs64_below(&rng, 10);
            const uint64_t pi = 1 + xs64_below(&rng, 200);
            const uint64_t ti = xs64_below(&rng, 3);
            const uint64_t ri = xs64_below(&rng, 5);
            mpfr_t x;
            mpfr_init2(x, (mpfr_prec_t)pi);
            if (ki < 7) {
                const double mag = (double)(1 + xs64_below(&rng, 1000000));
                const int sx = (xs64_below(&rng, 2) == 0) ? +1 : -1;
                mpfr_set_d(x, sx * mag, MPFR_RNDN);
                if (!mpfr_regular_p(x)) { mpfr_clear(x); continue; }
            } else if (ki == 7) {
                mpfr_set_zero(x, (xs64_below(&rng, 2) == 0) ? +1 : -1);
            } else if (ki == 8) {
                /* Inf with t != 0 has flag side effect — only use t=0 for Inf in fuzz. */
                mpfr_set_inf(x, (xs64_below(&rng, 2) == 0) ? +1 : -1);
                emit_case(out, "fuzz", x, 0, RNDS[ri]);
                mpfr_clear(x);
                continue;
            } else {
                mpfr_set_nan(x);
            }
            emit_case(out, "fuzz", x, TS[ti], RNDS[ri]);
            mpfr_clear(x);
        }
    }

    /* mined: 5 — common Ziv-loop tail patterns. */
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 1.0, MPFR_RNDN); emit_case(out, "mined", x, 0, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_d(x, 3.14, MPFR_RNDN); emit_case(out, "mined", x, +1, MPFR_RNDU); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1); emit_case(out, "mined", x, 0, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, +1); emit_case(out, "mined", x, 0, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x); emit_case(out, "mined", x, 0, MPFR_RNDN); mpfr_clear(x); }

    return 0;
}
