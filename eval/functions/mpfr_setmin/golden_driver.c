/*
 * golden_driver.c — Golden master for MPFR's mpfr_setmin.
 *
 * C signature
 * -----------
 *
 *   void mpfr_setmin(mpfr_ptr x, mpfr_exp_t e);
 *
 *   Constructs the minimum representable normal value at x's precision
 *   with exponent e: a single MSB bit set, every other mantissa bit
 *   zero. Magnitude = 2^(e - 1). Sign is preserved from x's pre-call
 *   state (mpfr/src/setmin.c L24 comment).
 *
 * Driver shape mirrors setmax's exactly — same wire format, same exp
 * range, same memory cap. The only differences are the C call
 * (mpfr_setmin vs mpfr_setmax) and the expected output (different
 * mantissa).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"prec":"<decimal>","exp":"<decimal>","sign":<1|-1>},
 *    "output":<MPFR-record>,
 *    "time_ns":<n>}
 *
 *   - `prec`, `exp` via jl_kv_u64 / jl_kv_i64.
 *   - `sign` via jl_kv_int — bare JS number, matches the TS port's
 *     Sign type.
 *   - `output` via jl_output_mpfr — bare MPFR.
 *
 * Tag distribution: same shape as setmax's golden_driver. The intent is
 * symmetry — both functions are sibling constructors and a unified set
 * of input combinations exercises both ports' identical structure.
 *
 * Ref: mpfr/src/setmin.c — C reference.
 * Ref: src/ops/setmin.ts — production port.
 * Ref: eval/functions/mpfr_setmax/golden_driver.c — sibling driver
 *   (this one is the structural mirror).
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_setmin golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Forward declaration: see the setmax driver's forward-decl comment.
 * mpfr_setmin is in mpfr-impl.h, not the public mpfr.h. */
extern void mpfr_setmin (mpfr_ptr, mpfr_exp_t);

/* Mirror src/core.ts PREC_MAX/PREC_MIN. */
#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Memory cap — same as setmax driver. 65536 bits to admit the 65521
 * prime adversarial case while staying ~8 KB per allocation. */
#define MAX_ALLOC_PREC ((uint64_t)65536)

/* Default libmpfr emax = 2^30 - 1; safe symmetric range. */
#define SAFE_EXP_MAX ((int64_t)1073740823)
#define SAFE_EXP_MIN ((int64_t)(-SAFE_EXP_MAX))

/* Emit one mpfr_setmin golden case. */
static inline void emit_case(FILE *out, const char *tag,
                             uint64_t prec, int64_t exp, int sign) {
    assert(prec >= TS_PREC_MIN && prec <= MAX_ALLOC_PREC);
    assert(sign == +1 || sign == -1);
    assert(exp >= SAFE_EXP_MIN && exp <= SAFE_EXP_MAX);

    mpfr_t probe;
    mpfr_init2(probe, (mpfr_prec_t)prec);
    /* Force sign before setmin: init to ±0, then setmin overwrites
     * mantissa and exp while preserving sign. */
    mpfr_set_zero(probe, sign);

    const uint64_t t0 = now_ns();
    mpfr_setmin(probe, (mpfr_exp_t)exp);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_u64(out, 1, "prec", prec);
    jl_kv_i64(out, 0, "exp", exp);
    jl_kv_int(out, 0, "sign", sign);
    jl_end_inputs(out);
    jl_output_mpfr(out, probe);
    jl_finish(out, elapsed);

    mpfr_clear(probe);
}

static inline void emit_both_signs(FILE *out, const char *tag,
                                   uint64_t prec, int64_t exp) {
    emit_case(out, tag, prec, exp, +1);
    emit_case(out, tag, prec, exp, -1);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 25 cases — common precs × small/medium exps × both signs */
    /* ============================================================== */
    {
        emit_both_signs(out, "happy", 24, 0);
        emit_both_signs(out, "happy", 53, 0);
        emit_both_signs(out, "happy", 64, 0);
        emit_both_signs(out, "happy", 113, 0);
        emit_both_signs(out, "happy", 128, 0);
        emit_both_signs(out, "happy", 256, 0);

        emit_case(out, "happy", 53, 10, +1);
        emit_case(out, "happy", 53, -10, +1);
        emit_case(out, "happy", 64, 100, +1);
        emit_case(out, "happy", 64, -100, +1);
        emit_case(out, "happy", 113, 1000, +1);
        emit_case(out, "happy", 113, -1000, +1);
        emit_case(out, "happy", 256, 50, +1);
        emit_case(out, "happy", 256, -50, +1);

        emit_case(out, "happy", 53, 100, -1);
        emit_case(out, "happy", 64, -50, -1);
        emit_case(out, "happy", 128, 200, -1);
        emit_case(out, "happy", 113, -200, -1);
        emit_case(out, "happy", 100, 0, -1);
    }

    /* ============================================================== */
    /* edge: 50 cases — boundary precs, signed-sign coverage, exp     */
    /* extremes.                                                      */
    /* ============================================================== */
    {
        /* PREC_MIN at exps -, 0, +, near-emax. */
        emit_both_signs(out, "edge", 1, 0);
        emit_both_signs(out, "edge", 1, 1);
        emit_both_signs(out, "edge", 1, -1);
        emit_both_signs(out, "edge", 1, SAFE_EXP_MAX);
        emit_both_signs(out, "edge", 1, SAFE_EXP_MIN);
        emit_both_signs(out, "edge", 1, SAFE_EXP_MIN + 1);

        /* Limb boundaries. */
        emit_both_signs(out, "edge", 63, 0);
        emit_both_signs(out, "edge", 64, 0);
        emit_both_signs(out, "edge", 65, 0);
        emit_both_signs(out, "edge", 127, 0);
        emit_both_signs(out, "edge", 128, 0);
        emit_both_signs(out, "edge", 129, 0);

        /* Large prec, large exp. */
        emit_both_signs(out, "edge", 2048, 10000);
        emit_both_signs(out, "edge", 4096, -10000);

        /* Max-allowed prec. */
        emit_both_signs(out, "edge", MAX_ALLOC_PREC, 0);
        emit_both_signs(out, "edge", MAX_ALLOC_PREC, 100);
        emit_both_signs(out, "edge", MAX_ALLOC_PREC - 1, 0);

        /* Near-emax safe boundary. */
        emit_both_signs(out, "edge", 53, SAFE_EXP_MAX);
        emit_both_signs(out, "edge", 53, SAFE_EXP_MIN);

        /* Pivots & mid-range. */
        emit_both_signs(out, "edge", 50, 0);
        emit_both_signs(out, "edge", 51, 0);
        emit_both_signs(out, "edge", 52, 0);
        emit_both_signs(out, "edge", 53, 0);
        emit_both_signs(out, "edge", 54, 0);

        /* Exp = 0 at large prec. */
        emit_case(out, "edge", 1024, 0, +1);
        emit_case(out, "edge", 1024, 0, -1);
    }

    /* ============================================================== */
    /* adversarial: 15 cases — primes / 2^k / near-boundary.          */
    /* ============================================================== */
    {
        emit_case(out, "adversarial", 53, 1, +1);
        emit_case(out, "adversarial", 127, 1, +1);
        emit_case(out, "adversarial", 1009, 1, +1);
        emit_case(out, "adversarial", 65521, 1, +1);
        emit_case(out, "adversarial", 65521, 1, -1);

        emit_case(out, "adversarial", 64, 0, +1);
        emit_case(out, "adversarial", 256, 0, -1);
        emit_case(out, "adversarial", 1024, 0, +1);

        emit_case(out, "adversarial", 63, 0, +1);
        emit_case(out, "adversarial", 65, 0, -1);

        emit_case(out, "adversarial", TS_PREC_MIN, SAFE_EXP_MAX, +1);
        emit_case(out, "adversarial", TS_PREC_MIN, SAFE_EXP_MIN, -1);
        emit_case(out, "adversarial", MAX_ALLOC_PREC, SAFE_EXP_MAX, +1);
        emit_case(out, "adversarial", MAX_ALLOC_PREC, SAFE_EXP_MIN, -1);
        emit_case(out, "adversarial", 64, SAFE_EXP_MAX, -1);
    }

    /* ============================================================== */
    /* fuzz: 60 cases — PRNG-driven                                   */
    /* ============================================================== */
    {
        /* Seed distinct from setmax's. "SETMIN F00BAR DA7A". */
        xs64_t rng;
        xs64_seed(&rng, 0x5E771F00BA0DA7AAULL);

        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 2048);
            const int64_t exp = (int64_t)(xs64_below(&rng, 200001)) - 100000;
            const int sign = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            emit_case(out, "fuzz", prec, exp, sign);
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — adapted from mpfr/tests/tfma.c L123, L438     */
    /*                                                                  */
    /* Upstream patterns: `mpfr_setmin(x, mpfr_get_emax())` (tfma L123,*/
    /* "0.1 @ emax" — smallest at max exp), `mpfr_setmin(x, emin)`     */
    /* (tfma L438 "0.1 @ emin"), and similar.                          */
    /* ============================================================== */
    {
        /* tfma.c L123 pattern: small prec, sign +, exp at safe-max. */
        emit_case(out, "mined", 8, SAFE_EXP_MAX, +1);
        /* tfma.c L438 pattern: small prec, sign +, exp at safe-min. */
        emit_case(out, "mined", 8, SAFE_EXP_MIN, +1);
        /* Variant: negative sign with same magnitude. */
        emit_case(out, "mined", 8, SAFE_EXP_MAX, -1);
        /* IEEE float64 prec at zero exp (common test). */
        emit_case(out, "mined", 53, 0, +1);
        /* IEEE float64 prec at typical near-zero exp. */
        emit_case(out, "mined", 53, 1, +1);
    }

    return 0;
}
