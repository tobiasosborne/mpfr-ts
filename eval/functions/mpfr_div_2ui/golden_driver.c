/*
 * golden_driver.c — Golden master for MPFR's mpfr_div_2ui.
 *
 * C signature
 * -----------
 *
 *   int mpfr_div_2ui(mpfr_ptr y, mpfr_srcptr x, unsigned long n,
 *                    mpfr_rnd_t rnd_mode);
 *
 * Divides x by 2^n where n is non-negative. Mantissa unchanged;
 * exp -= n. Ref: mpfr/src/div_2ui.c L24-L68.
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  22
 *   edge         :  30
 *   adversarial  :  10
 *   fuzz         :  55
 *   mined        :   5
 *
 * Ref: src/ops/div_2ui.ts — production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_div_2ui golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

static const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr op, unsigned long n,
                             uint64_t prec, mpfr_rnd_t rnd) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    mpfr_t rop;
    mpfr_init2(rop, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    const int ternary = mpfr_div_2ui(rop, op, n, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", op);
    jl_kv_u64(out, 0, "n", (uint64_t)n);
    jl_kv_u64(out, 0, "prec", prec);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_result(out, rop, ternary);
    jl_finish(out, elapsed);

    mpfr_clear(rop);
}

static inline void emit_d(FILE *out, const char *tag, double d, uint64_t xprec,
                          unsigned long n, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec);
    mpfr_set_d(x, d, MPFR_RNDN);
    emit_case(out, tag, x, n, prec, rnd);
    mpfr_clear(x);
}

static inline void emit_si(FILE *out, const char *tag, long v, uint64_t xprec,
                           unsigned long n, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)xprec);
    mpfr_set_si(x, v, MPFR_RNDN);
    emit_case(out, tag, x, n, prec, rnd);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 22 */
    emit_si(out, "happy", 4, 53, 0, 53, MPFR_RNDN);
    emit_si(out, "happy", 4, 53, 1, 53, MPFR_RNDN);
    emit_si(out, "happy", 4, 53, 2, 53, MPFR_RNDN);
    emit_si(out, "happy", 8, 53, 3, 53, MPFR_RNDN);
    emit_si(out, "happy", 16, 53, 4, 53, MPFR_RNDN);
    emit_si(out, "happy", -16, 53, 4, 53, MPFR_RNDN);
    emit_si(out, "happy", 100, 53, 7, 53, MPFR_RNDN);
    emit_si(out, "happy", -100, 53, 7, 53, MPFR_RNDN);
    emit_si(out, "happy", 1024, 53, 10, 53, MPFR_RNDN);
    emit_si(out, "happy", 1, 53, 100, 53, MPFR_RNDN);
    emit_d(out, "happy", 12.0, 53, 2, 53, MPFR_RNDN);
    emit_d(out, "happy", -12.0, 53, 2, 53, MPFR_RNDN);
    emit_d(out, "happy", 3.14, 53, 1, 53, MPFR_RNDN);
    emit_d(out, "happy", 3.14, 53, 0, 53, MPFR_RNDN);
    emit_d(out, "happy", 1.0/3.0, 53, 5, 24, MPFR_RNDN);
    emit_d(out, "happy", -1.0/7.0, 53, 5, 24, MPFR_RNDN);
    emit_d(out, "happy", 1e10, 53, 30, 53, MPFR_RNDN);
    emit_d(out, "happy", 1e-10, 53, 30, 53, MPFR_RNDN);
    /* Singulars. */
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1); emit_case(out, "happy", x, 5, 53, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1); emit_case(out, "happy", x, 5, 53, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, +1); emit_case(out, "happy", x, 5, 53, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x); emit_case(out, "happy", x, 5, 53, MPFR_RNDN); mpfr_clear(x); }

    /* edge: 30 */
    for (int i = 0; i < 5; ++i) emit_si(out, "edge", 1, 1, 0, 1, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_si(out, "edge", 1, 1, 1, 1, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.0/3.0, 53, 5, 3, RNDS[i]);
    for (int i = 0; i < 5; ++i) emit_d(out, "edge", -1.0/3.0, 53, 5, 3, RNDS[i]);
    /* n = 0 (no-op). */
    emit_d(out, "edge", 3.14, 53, 0, 53, MPFR_RNDN);
    emit_d(out, "edge", 3.14, 53, 0, 24, MPFR_RNDU);
    /* Singulars at PREC_MIN. */
    { mpfr_t x; mpfr_init2(x, 1); mpfr_set_zero(x, -1); emit_case(out, "edge", x, 10, 1, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 1); mpfr_set_inf(x, -1); emit_case(out, "edge", x, 10, 1024, MPFR_RNDN); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 1); mpfr_set_nan(x); emit_case(out, "edge", x, 0, 53, MPFR_RNDN); mpfr_clear(x); }
    /* Limb-boundary precs. */
    emit_d(out, "edge", 1.5, 63, 1, 53, MPFR_RNDN);
    emit_d(out, "edge", 1.5, 64, 1, 64, MPFR_RNDN);
    emit_d(out, "edge", 1.5, 65, 1, 64, MPFR_RNDN);
    emit_d(out, "edge", 1.5, 200, 100, 100, MPFR_RNDN);
    emit_d(out, "edge", 3.14, 53, 1000, 53, MPFR_RNDU);

    /* adversarial: 10 */
    emit_d(out, "adversarial", 0.9999999999999999, 53, 1, 53, MPFR_RNDN);
    emit_d(out, "adversarial", 0.9999999999999999, 53, 1, 24, MPFR_RNDU);
    emit_d(out, "adversarial", 1.0000000000000002, 53, 5, 24, MPFR_RNDD);
    emit_d(out, "adversarial", -1.0/3.0, 53, 3, 8, MPFR_RNDU);
    emit_d(out, "adversarial", -1.0/3.0, 53, 3, 8, MPFR_RNDD);
    emit_d(out, "adversarial", 1.0/3.0, 53, 3, 8, MPFR_RNDA);
    emit_d(out, "adversarial", -1.0/3.0, 53, 3, 8, MPFR_RNDA);
    emit_d(out, "adversarial", 1.5, 24, 200, 53, MPFR_RNDN);
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1); emit_case(out, "adversarial", x, 5, 24, MPFR_RNDD); mpfr_clear(x); }
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_inf(x, -1); emit_case(out, "adversarial", x, 5, 24, MPFR_RNDU); mpfr_clear(x); }

    /* fuzz: 55 */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xBEEFCAFEDEAD1234ULL);
        for (int rep = 0; rep < 55; ++rep) {
            const uint64_t kind_sel = xs64_below(&rng, 10);
            const uint64_t srcPrec = 1 + xs64_below(&rng, 200);
            const uint64_t outPrec = 1 + xs64_below(&rng, 200);
            const uint64_t n = xs64_below(&rng, 200);
            const uint64_t rnd_idx = xs64_below(&rng, 5);
            mpfr_t x;
            mpfr_init2(x, (mpfr_prec_t)srcPrec);
            if (kind_sel < 7) {
                const double mag = (double)(1 + xs64_below(&rng, 1000000));
                const int sx = (xs64_below(&rng, 2) == 0) ? +1 : -1;
                mpfr_set_d(x, sx * mag, MPFR_RNDN);
                if (!mpfr_regular_p(x)) { mpfr_clear(x); continue; }
            } else if (kind_sel == 7) {
                mpfr_set_zero(x, (xs64_below(&rng, 2) == 0) ? +1 : -1);
            } else if (kind_sel == 8) {
                mpfr_set_inf(x, (xs64_below(&rng, 2) == 0) ? +1 : -1);
            } else {
                mpfr_set_nan(x);
            }
            emit_case(out, "fuzz", x, (unsigned long)n, outPrec, RNDS[rnd_idx]);
            mpfr_clear(x);
        }
    }

    /* mined: 5 */
    emit_si(out, "mined", 1, 53, 1, 53, MPFR_RNDN);
    emit_si(out, "mined", 1024, 53, 10, 53, MPFR_RNDN);
    emit_d(out, "mined", 12.0, 53, 2, 53, MPFR_RNDN);
    emit_d(out, "mined", 3.14, 53, 0, 53, MPFR_RNDN);
    { mpfr_t x; mpfr_init2(x, 53); mpfr_set_nan(x); emit_case(out, "mined", x, 5, 53, MPFR_RNDN); mpfr_clear(x); }

    return 0;
}
