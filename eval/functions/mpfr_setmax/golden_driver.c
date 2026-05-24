/*
 * golden_driver.c — Golden master for MPFR's mpfr_setmax.
 *
 * C signature
 * -----------
 *
 *   void mpfr_setmax(mpfr_ptr x, mpfr_exp_t e);
 *
 *   Constructs the maximum representable value at x's precision with
 *   exponent e: mantissa bits all 1 in positions [0, prec), low pad 0.
 *   Sign is whatever x carried before the call ("current sign is kept",
 *   mpfr/src/setmax.c L24 comment).
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS port `mpfr_setmax(prec, exp, sign?=1) -> MPFR` takes precision and
 * sign explicitly. To drive the C side at parity:
 *
 *   1. mpfr_init2(probe, prec)             — allocate at the chosen prec.
 *   2. Set sign on `probe` (MPFR_SET_POS / MPFR_SET_NEG via mpfr_setsign
 *      on a +0 placeholder — cleanest cross-version API).
 *   3. mpfr_setmax(probe, exp)             — the operation we mirror.
 *   4. Emit the resulting MPFR via jl_output_mpfr.
 *
 * The TS expected output is identical to libmpfr's output (modulo
 * representation): kind='normal', sign (per arg), prec, exp, and
 * mant = 2^prec - 1. The jl_kv_mpfr / jl_output_mpfr round-trip via
 * mpfr_get_z_2exp handles the limb-to-bigint conversion correctly.
 *
 * Exp range
 * ---------
 *
 * libmpfr's MPFR_SET_EXP asserts MPFR_EXP_IN_RANGE — i.e. e must lie
 * in [1 - MPFR_EXP_INVALID, MPFR_EXP_INVALID - 1]. On the default
 * mpfr-3.1+ build, __gmpfr_emin = -2^30 + 1 = -(MPFR_EMAX_DEFAULT) and
 * __gmpfr_emax = 2^30 - 1 = MPFR_EMAX_DEFAULT (mpfr/src/mpfr.h L231-L232).
 * We test exps from -1073741000 up to +1073741000 (just inside the
 * default range, leaving a small margin so any libmpfr-side range
 * tightening doesn't break the goldens).
 *
 * Per the TS port's no-range-constraint policy, exps outside libmpfr's
 * range are NOT tested in goldens (libmpfr would assert-abort), but the
 * TS port accepts them silently. That's a documented divergence in
 * spec.json's divergence_from_c field; the TS port's validateArgs gate
 * does check that exp is a bigint.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"prec":"<decimal>","exp":"<decimal>","sign":<1|-1>},
 *    "output":<MPFR-record>,
 *    "time_ns":<n>}
 *
 *   - `prec`, `exp` via jl_kv_u64 / jl_kv_i64 — decimal-string bigints
 *     round-trip through BigInt() on the TS side.
 *   - `sign` via jl_kv_int — bare JS number, matches the TS port's
 *     Sign type.
 *   - `output` via jl_output_mpfr — bare MPFR (no Result wrapper;
 *     setmax is an exact constructor, not a rounding op).
 *
 * Tag distribution (Rule 7 minimums)
 * ----------------------------------
 *
 *   happy        :  ~25  (common precs × small/medium exps, both signs)
 *   edge         :  ~50  (PREC_MIN, large precs, exp = 0 / large / negative
 *                         / near-libmpfr-emax, both signs, sign-only flip)
 *   adversarial  :  ~15  (primes / 2^k precs, exp boundary near libmpfr
 *                         range; very small + very large prec)
 *   fuzz         :   60  (PRNG: prec ∈ [1, 2048], exp ∈ [-100k, 100k], sign)
 *   mined        :    5  (mpfr/tests/tfma.c L90, tdiv.c L1285 patterns:
 *                         mpfr_setmax(x, mpfr_get_emax()) — but emax is
 *                         huge; substitute small visible exps for
 *                         readability with the same shape).
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/setmax.c — C reference.
 * Ref: src/ops/setmax.ts — production port.
 * Ref: src/core.ts L93-L135 — MPFR value model.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_setmax golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Forward declaration: mpfr_setmax / mpfr_setmin are declared in the
 * INTERNAL header mpfr/src/mpfr-impl.h (L2487-L2488), NOT in the
 * publicly-installed mpfr.h. They are however exported symbols in
 * libmpfr.so — we forward-declare them here so the public-header-only
 * compile of this driver picks them up at link time. */
extern void mpfr_setmax (mpfr_ptr, mpfr_exp_t);

/* Mirror src/core.ts PREC_MAX/PREC_MIN. */
#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Memory cap: setmax allocates the full limb array; PREC_MAX would
 * burn ~256MB per case. 65536 bits (~8 KB per mantissa) keeps the
 * goldens small and fast while letting us test up to the 65521-prime
 * adversarial case. */
#define MAX_ALLOC_PREC ((uint64_t)65536)

/* Default libmpfr emax = 2^30 - 1; the safe symmetric range we test
 * within. Leave a 1000-margin for hypothetical platform variation. */
#define SAFE_EXP_MAX ((int64_t)1073740823)   /* MPFR_EMAX_DEFAULT - 1000 */
#define SAFE_EXP_MIN ((int64_t)(-SAFE_EXP_MAX))

/* Emit one mpfr_setmax golden case.
 *
 * Drives the C side at parity with the TS port's (prec, exp, sign)
 * signature. */
static inline void emit_case(FILE *out, const char *tag,
                             uint64_t prec, int64_t exp, int sign) {
    assert(prec >= TS_PREC_MIN && prec <= MAX_ALLOC_PREC);
    assert(sign == +1 || sign == -1);
    assert(exp >= SAFE_EXP_MIN && exp <= SAFE_EXP_MAX);

    mpfr_t probe;
    mpfr_init2(probe, (mpfr_prec_t)prec);
    /* Force sign before setmax (the C comment says "current sign is
     * kept"). mpfr_set_zero(x, sign) initialises probe to ±0 at the
     * chosen sign, then setmax overwrites the mantissa and exp but
     * preserves sign. This is the cleanest cross-version idiom (works
     * on every mpfr 3.x / 4.x). */
    mpfr_set_zero(probe, sign);

    const uint64_t t0 = now_ns();
    mpfr_setmax(probe, (mpfr_exp_t)exp);
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

/* Convenience: emit both signs at the same (prec, exp). */
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
        /* (12 cases) Common precs at exp = 0: smallest mantissa-aligned. */
        emit_both_signs(out, "happy", 24, 0);     /* IEEE float32 */
        emit_both_signs(out, "happy", 53, 0);     /* IEEE float64 */
        emit_both_signs(out, "happy", 64, 0);     /* x86 long double */
        emit_both_signs(out, "happy", 113, 0);    /* IEEE float128 */
        emit_both_signs(out, "happy", 128, 0);
        emit_both_signs(out, "happy", 256, 0);

        /* (8 cases) Common precs × {exp=10, exp=-10}, sign=+1. */
        emit_case(out, "happy", 53, 10, +1);
        emit_case(out, "happy", 53, -10, +1);
        emit_case(out, "happy", 64, 100, +1);
        emit_case(out, "happy", 64, -100, +1);
        emit_case(out, "happy", 113, 1000, +1);
        emit_case(out, "happy", 113, -1000, +1);
        emit_case(out, "happy", 256, 50, +1);
        emit_case(out, "happy", 256, -50, +1);

        /* (5 cases) Negative-sign sanity with non-zero exps. */
        emit_case(out, "happy", 53, 100, -1);
        emit_case(out, "happy", 64, -50, -1);
        emit_case(out, "happy", 128, 200, -1);
        emit_case(out, "happy", 113, -200, -1);
        emit_case(out, "happy", 100, 0, -1);
    }

    /* ============================================================== */
    /* edge: 50 cases — boundary precs, signed-sign coverage, exp     */
    /* extremes within the safe range.                                */
    /* ============================================================== */
    {
        /* (8 cases) PREC_MIN at exps -, 0, +, near-emax. */
        emit_both_signs(out, "edge", 1, 0);
        emit_both_signs(out, "edge", 1, 1);
        emit_both_signs(out, "edge", 1, -1);
        emit_both_signs(out, "edge", 1, SAFE_EXP_MAX);

        /* (4 cases) PREC_MIN at very negative exp. */
        emit_both_signs(out, "edge", 1, SAFE_EXP_MIN);
        emit_both_signs(out, "edge", 1, SAFE_EXP_MIN + 1);

        /* (12 cases) Limb boundaries (63/64/65, 127/128/129). */
        emit_both_signs(out, "edge", 63, 0);
        emit_both_signs(out, "edge", 64, 0);
        emit_both_signs(out, "edge", 65, 0);
        emit_both_signs(out, "edge", 127, 0);
        emit_both_signs(out, "edge", 128, 0);
        emit_both_signs(out, "edge", 129, 0);

        /* (4 cases) Large prec, large exp. */
        emit_both_signs(out, "edge", 2048, 10000);
        emit_both_signs(out, "edge", 4096, -10000);

        /* (6 cases) Max-allowed prec (within MAX_ALLOC_PREC). */
        emit_both_signs(out, "edge", MAX_ALLOC_PREC, 0);
        emit_both_signs(out, "edge", MAX_ALLOC_PREC, 100);
        emit_both_signs(out, "edge", MAX_ALLOC_PREC - 1, 0);

        /* (4 cases) Near-emax safe boundary. */
        emit_both_signs(out, "edge", 53, SAFE_EXP_MAX);
        emit_both_signs(out, "edge", 53, SAFE_EXP_MIN);

        /* (10 cases) Pivots & mid-range. */
        emit_both_signs(out, "edge", 50, 0);
        emit_both_signs(out, "edge", 51, 0);
        emit_both_signs(out, "edge", 52, 0);
        emit_both_signs(out, "edge", 53, 0);
        emit_both_signs(out, "edge", 54, 0);

        /* (2 cases) Exp = 0 at large prec. */
        emit_case(out, "edge", 1024, 0, +1);
        emit_case(out, "edge", 1024, 0, -1);
    }

    /* ============================================================== */
    /* adversarial: 15 cases — primes / 2^k / near-boundary edge      */
    /* combinations a naive port might branch on.                      */
    /* ============================================================== */
    {
        /* Primes. */
        emit_case(out, "adversarial", 53, 1, +1);
        emit_case(out, "adversarial", 127, 1, +1);
        emit_case(out, "adversarial", 1009, 1, +1);
        emit_case(out, "adversarial", 65521, 1, +1);  /* largest prime < 2^16 */
        emit_case(out, "adversarial", 65521, 1, -1);

        /* 2^k precs. */
        emit_case(out, "adversarial", 64, 0, +1);
        emit_case(out, "adversarial", 256, 0, -1);
        emit_case(out, "adversarial", 1024, 0, +1);

        /* 2^k ± 1 — limb-rounding boundary. */
        emit_case(out, "adversarial", 63, 0, +1);
        emit_case(out, "adversarial", 65, 0, -1);

        /* PREC_MIN combined with the most extreme safe exps. */
        emit_case(out, "adversarial", TS_PREC_MIN, SAFE_EXP_MAX, +1);
        emit_case(out, "adversarial", TS_PREC_MIN, SAFE_EXP_MIN, -1);

        /* Large prec + edge exp. */
        emit_case(out, "adversarial", MAX_ALLOC_PREC, SAFE_EXP_MAX, +1);
        emit_case(out, "adversarial", MAX_ALLOC_PREC, SAFE_EXP_MIN, -1);

        /* The prec=limb boundary across positive/negative exp. */
        emit_case(out, "adversarial", 64, SAFE_EXP_MAX, -1);
    }

    /* ============================================================== */
    /* fuzz: 60 cases — PRNG-driven prec / exp / sign                 */
    /* ============================================================== */
    {
        /* Seed: "SETMAX D4D1FACE" — distinct from other drivers. */
        xs64_t rng;
        xs64_seed(&rng, 0x5E7714AD471FACE5ULL);

        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 2048);  /* [1, 2048] */
            /* Symmetric exp in roughly [-100000, +100000]; uses two
             * xs64_next calls to widen the range past xs64_below's 64-bit
             * modulo bias on small bounds. */
            const int64_t exp = (int64_t)(xs64_below(&rng, 200001)) - 100000;
            const int sign = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            emit_case(out, "fuzz", prec, exp, sign);
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — adapted from mpfr/tests/tfma.c, tdiv.c        */
    /*                                                                  */
    /* Upstream usage is `mpfr_setmax(x, mpfr_get_emax())` — but emax  */
    /* is library-state-dependent and large (default 2^30 - 1). We     */
    /* keep the shape (small prec, sign forced positive, max in-safe   */
    /* exp) so the wire output matches.                                */
    /* ============================================================== */
    {
        /* tfma.c L90 pattern: prec=8, sign=+, exp at the emax-like edge. */
        emit_case(out, "mined", 8, SAFE_EXP_MAX, +1);
        /* tdiv.c L1285 pattern: prec=53, sign=+, exp large. */
        emit_case(out, "mined", 53, SAFE_EXP_MAX, +1);
        /* tfma.c-style with non-default sign. */
        emit_case(out, "mined", 8, SAFE_EXP_MAX, -1);
        /* Common typical exp around 100. */
        emit_case(out, "mined", 24, 100, +1);
        /* Default IEEE float64 + zero exp. */
        emit_case(out, "mined", 53, 0, +1);
    }

    return 0;
}
