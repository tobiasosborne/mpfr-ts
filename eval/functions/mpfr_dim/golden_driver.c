/*
 * golden_driver.c — Golden master for MPFR's mpfr_dim.
 *
 * dim(b, c) = max(0, b - c). NaN if either input is NaN. Ref:
 * mpfr/src/dim.c L24-L47.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"b":<MPFR>,"c":<MPFR>,"prec":"<decimal>","rnd":"RND[NZUDA]"},
 *    "output":{"value":<MPFR>,"ternary":<-1|0|1>},
 *    "time_ns":<n>}
 *
 * Tag distribution: 22/30/10/55/5.
 *
 * Ref: src/ops/dim.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_dim golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr b, mpfr_srcptr c,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t z;
    mpfr_init2(z, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_dim(z, b, c, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "b", b);
    jl_kv_mpfr(out, 0, "c", c);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, z, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(z);
}

static inline void emit_dd(FILE *out, const char *tag, double bd, double cd,
                           uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t b, c;
    mpfr_init2(b, 53); mpfr_set_d(b, bd, MPFR_RNDN);
    mpfr_init2(c, 53); mpfr_set_d(c, cd, MPFR_RNDN);
    emit_case(out, tag, b, c, prec, rnd);
    mpfr_clear(b); mpfr_clear(c);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 — basic b/c pairs. */
    emit_dd(out, "happy", 5.0, 3.0, 53, MPFR_RNDN);   /* b > c → 2 */
    emit_dd(out, "happy", 3.0, 5.0, 53, MPFR_RNDN);   /* b < c → +0 */
    emit_dd(out, "happy", 5.0, 5.0, 53, MPFR_RNDN);   /* b == c → +0 */
    emit_dd(out, "happy", 1.0, 0.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 0.0, 1.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", -3.0, -5.0, 53, MPFR_RNDN); /* -3 > -5 → 2 */
    emit_dd(out, "happy", -5.0, -3.0, 53, MPFR_RNDN); /* -5 < -3 → +0 */
    emit_dd(out, "happy", 3.14, 2.71, 53, MPFR_RNDN);
    emit_dd(out, "happy", 2.71, 3.14, 53, MPFR_RNDN);
    emit_dd(out, "happy", 100.0, 50.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", -100.0, 50.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 100.0, -50.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.0/3.0, 1.0/7.0, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1.5, 0.5, 24, MPFR_RNDN);
    emit_dd(out, "happy", 1e10, 1e5, 53, MPFR_RNDN);
    emit_dd(out, "happy", 1e-10, 1e-5, 53, MPFR_RNDN);   /* tiny vs less-tiny */
    /* NaN cases. */
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_nan(b); mpfr_init2(c, 53); mpfr_set_d(c, 1.0, MPFR_RNDN); emit_case(out, "happy", b, c, 53, MPFR_RNDN); mpfr_clear(b); mpfr_clear(c); }
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_d(b, 1.0, MPFR_RNDN); mpfr_init2(c, 53); mpfr_set_nan(c); emit_case(out, "happy", b, c, 53, MPFR_RNDN); mpfr_clear(b); mpfr_clear(c); }
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_nan(b); mpfr_init2(c, 53); mpfr_set_nan(c); emit_case(out, "happy", b, c, 53, MPFR_RNDN); mpfr_clear(b); mpfr_clear(c); }
    /* Inf cases. */
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_inf(b, +1); mpfr_init2(c, 53); mpfr_set_d(c, 1.0, MPFR_RNDN); emit_case(out, "happy", b, c, 53, MPFR_RNDN); mpfr_clear(b); mpfr_clear(c); }   /* +inf > 1 → +inf */
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_d(b, 1.0, MPFR_RNDN); mpfr_init2(c, 53); mpfr_set_inf(c, +1); emit_case(out, "happy", b, c, 53, MPFR_RNDN); mpfr_clear(b); mpfr_clear(c); }   /* 1 < +inf → +0 */
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_inf(b, +1); mpfr_init2(c, 53); mpfr_set_inf(c, -1); emit_case(out, "happy", b, c, 53, MPFR_RNDN); mpfr_clear(b); mpfr_clear(c); }   /* +inf > -inf → +inf */

    /* edge: 30 — all rounding modes, kind combinations, prec extremes. */
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 5.0, 3.0, 53, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 3.0, 5.0, 53, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", 1.0/3.0, 1.0/7.0, 3, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_dd(out, "edge", -1.0/3.0, -1.0/7.0, 3, RNDS[i]);
    /* PREC_MIN. */
    emit_dd(out, "edge", 5.0, 3.0, 1, MPFR_RNDN);
    emit_dd(out, "edge", 3.0, 5.0, 1, MPFR_RNDN);
    /* Zero vs zero (sign matters): +0 - -0 case (which is +0). */
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_zero(b, +1); mpfr_init2(c, 53); mpfr_set_zero(c, -1); emit_case(out, "edge", b, c, 53, MPFR_RNDN); mpfr_clear(b); mpfr_clear(c); }
    /* -inf vs anything finite. */
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_inf(b, -1); mpfr_init2(c, 53); mpfr_set_d(c, 1.0, MPFR_RNDN); emit_case(out, "edge", b, c, 53, MPFR_RNDN); mpfr_clear(b); mpfr_clear(c); }
    /* +inf - +inf = NaN, but dim of +inf and +inf where neither greater → +0 (since +inf <= +inf). */
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_inf(b, +1); mpfr_init2(c, 53); mpfr_set_inf(c, +1); emit_case(out, "edge", b, c, 53, MPFR_RNDN); mpfr_clear(b); mpfr_clear(c); }
    /* NaN vs inf. */
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_nan(b); mpfr_init2(c, 53); mpfr_set_inf(c, +1); emit_case(out, "edge", b, c, 53, MPFR_RNDN); mpfr_clear(b); mpfr_clear(c); }
    /* Limb boundaries. */
    emit_dd(out, "edge", 5.0, 3.0, 63, MPFR_RNDN);
    emit_dd(out, "edge", 5.0, 3.0, 64, MPFR_RNDN);
    emit_dd(out, "edge", 5.0, 3.0, 65, MPFR_RNDN);
    emit_dd(out, "edge", 7.0, 4.0, 100, MPFR_RNDU);

    /* adversarial: 10 — cancellation, sign-asymmetric rounding, NaN. */
    /* Near-cancellation: b - c is tiny. */
    emit_dd(out, "adversarial", 1.0 + 1e-10, 1.0, 24, MPFR_RNDN);
    emit_dd(out, "adversarial", 1.0 + 1e-15, 1.0, 24, MPFR_RNDU);
    emit_dd(out, "adversarial", -1.0/3.0, -1.0/3.0, 53, MPFR_RNDN);   /* b == c → +0 */
    emit_dd(out, "adversarial", 1.0/3.0, 1.0/3.0 + 1e-50, 53, MPFR_RNDN);  /* b < c → +0 */
    /* +0 vs -0 — both finite, +0 == -0, so dim = +0. */
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_zero(b, +1); mpfr_init2(c, 53); mpfr_set_zero(c, -1); emit_case(out, "adversarial", b, c, 53, MPFR_RNDD); mpfr_clear(b); mpfr_clear(c); }
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_zero(b, -1); mpfr_init2(c, 53); mpfr_set_zero(c, +1); emit_case(out, "adversarial", b, c, 53, MPFR_RNDD); mpfr_clear(b); mpfr_clear(c); }
    /* NaN at PREC_MIN. */
    { mpfr_t b, c; mpfr_init2(b, 1); mpfr_set_nan(b); mpfr_init2(c, 1); mpfr_set_nan(c); emit_case(out, "adversarial", b, c, 1, MPFR_RNDA); mpfr_clear(b); mpfr_clear(c); }
    /* Large vs small. */
    emit_dd(out, "adversarial", 1e300, 1e-300, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", 1e-300, 1e300, 53, MPFR_RNDN);
    emit_dd(out, "adversarial", -1.0/3.0, 1.0/3.0, 53, MPFR_RNDU);

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xD144ABCDEFABCD12ULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t ka = xs64_below(&rng, 10);
            const uint64_t kb = xs64_below(&rng, 10);
            const uint64_t pa = 1 + xs64_below(&rng, 200);
            const uint64_t pb = 1 + xs64_below(&rng, 200);
            const uint64_t prec = 1 + xs64_below(&rng, 200);
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            mpfr_t b, c;
            mpfr_init2(b, (mpfr_prec_t)pa);
            mpfr_init2(c, (mpfr_prec_t)pb);
            /* Populate b. */
            if (ka < 7) {
                const double mag = (double)(1 + xs64_below(&rng, 1000000));
                const int sa = (xs64_below(&rng, 2) == 0) ? +1 : -1;
                mpfr_set_d(b, sa * mag, MPFR_RNDN);
                if (!mpfr_regular_p(b)) { mpfr_clear(b); mpfr_clear(c); continue; }
            } else if (ka == 7) {
                mpfr_set_zero(b, (xs64_below(&rng, 2) == 0) ? +1 : -1);
            } else if (ka == 8) {
                mpfr_set_inf(b, (xs64_below(&rng, 2) == 0) ? +1 : -1);
            } else {
                mpfr_set_nan(b);
            }
            /* Populate c. */
            if (kb < 7) {
                const double mag = (double)(1 + xs64_below(&rng, 1000000));
                const int sb = (xs64_below(&rng, 2) == 0) ? +1 : -1;
                mpfr_set_d(c, sb * mag, MPFR_RNDN);
                if (!mpfr_regular_p(c)) { mpfr_clear(b); mpfr_clear(c); continue; }
            } else if (kb == 7) {
                mpfr_set_zero(c, (xs64_below(&rng, 2) == 0) ? +1 : -1);
            } else if (kb == 8) {
                mpfr_set_inf(c, (xs64_below(&rng, 2) == 0) ? +1 : -1);
            } else {
                mpfr_set_nan(c);
            }
            emit_case(out, "fuzz", b, c, prec, RNDS[rnd_idx]);
            mpfr_clear(b); mpfr_clear(c);
        }
    }

    /* mined: 5 — patterns from mpfr/tests/tdim.c. */
    emit_dd(out, "mined", 5.0, 3.0, 53, MPFR_RNDN);
    emit_dd(out, "mined", 3.0, 5.0, 53, MPFR_RNDN);
    emit_dd(out, "mined", 0.0, 0.0, 53, MPFR_RNDN);
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_nan(b); mpfr_init2(c, 53); mpfr_set_d(c, 1.0, MPFR_RNDN); emit_case(out, "mined", b, c, 53, MPFR_RNDN); mpfr_clear(b); mpfr_clear(c); }
    { mpfr_t b, c; mpfr_init2(b, 53); mpfr_set_inf(b, +1); mpfr_init2(c, 53); mpfr_set_inf(c, -1); emit_case(out, "mined", b, c, 53, MPFR_RNDN); mpfr_clear(b); mpfr_clear(c); }

    return 0;
}
