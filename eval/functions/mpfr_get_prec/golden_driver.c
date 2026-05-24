/*
 * golden_driver.c — Golden master for MPFR's mpfr_get_prec.
 *
 * C signature
 * -----------
 *
 *   mpfr_prec_t mpfr_get_prec(mpfr_srcptr x);
 *
 *   Trivial single-field read: `return MPFR_PREC(x)`. Defined at the
 *   bottom of mpfr/src/set_prec.c (L55-L59), NOT in its own file.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS port `mpfr_get_prec(x: MPFR) -> bigint` reads x.prec directly.
 * For finite/inf/zero MPFR values the TS prec field equals what
 * libmpfr's mpfr_get_prec would return at the same construction site.
 *
 * NaN divergence (mpfr_storage_traps.md §3): the TS schema's NaN
 * sentinel is folded to prec=0n unconditionally (src/core.ts L103-L107),
 * regardless of the originating precision. libmpfr keeps the originating
 * precision on its NaN. This driver therefore emits the TS-side
 * expected value (0) for NaN inputs — NOT what libmpfr's mpfr_get_prec
 * would return for the same x.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-record>},
 *    "output":"<decimal-bigint>",
 *    "time_ns":<n>}
 *
 *   - `output` is jl_output_scalar_i64 — a quoted signed decimal string.
 *     The TS decoder (decodeExpectedOutput in value_codec.ts) classifies
 *     it as `{kind:'scalar', value: BigInt(s)}` and compareOutput's
 *     scalar/bigint branch accepts the port's bigint return.
 *
 * Tag distribution (Rule 7 minimums; capped at PREC_MAX = 2^31 - 257)
 * ------------------------------------------------------------------
 *
 *   happy        :  ~25  (typical precs across all four kinds)
 *   edge         :  ~50  (PREC_MIN / PREC_MAX bookends; signed zero ±0;
 *                         signed inf ±Inf; the NaN-divergence case)
 *   adversarial  :  ~15  (primes / 2^k boundaries; large precisions
 *                         that stress the bigint return path)
 *   fuzz         :   60  (PRNG; prec ∈ [1, 4096] capped for memory)
 *   mined        :    5  (mpfr/tests usage — get_prec is read-only;
 *                         mined cases are all "init at p, then read")
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/set_prec.c L55-L59 — C reference.
 * Ref: src/ops/get_prec.ts — production port.
 * Ref: src/core.ts L113-L135 — MPFR.prec field semantics.
 * Ref: mpfr_storage_traps.md §3 — NaN drops prec on the TS side.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_get_prec golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Mirror src/core.ts PREC_MAX/PREC_MIN. */
#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Memory-safety cap: any prec above this would allocate megabytes per
 * case. The TS port reads x.prec from a fixed-shape immutable value, so
 * the choice of large prec is a "does the driver / wire codec handle
 * large bigints" test, not a "does the port allocate" test. 65536 bits
 * is plenty (~8 KB per mantissa, fast and small) and lets us test
 * 65521 (largest prime < 2^16). PREC_MAX (~2^31) is NOT tested here
 * because libmpfr would allocate ~256 MB per case. */
#define MAX_ALLOC_PREC ((uint64_t)65536)

/* Emit a generic "read the prec from this MPFR" case.
 *
 *   1. Construct x at the requested prec + kind.
 *   2. Call mpfr_get_prec(x); time the call.
 *   3. Emit {tag, inputs:{x}, output: <prec or 0 for NaN>}
 *
 * For NaN inputs the TS port returns 0n (the canonical NAN_VALUE has
 * prec === 0n); we emit 0 as the expected output regardless of what
 * libmpfr's mpfr_get_prec returns. The driver still calls libmpfr's
 * function so the time_ns reflects the C-side hot-path timing.
 *
 * The `is_nan` flag is explicit (not inferred via mpfr_nan_p(x)) so a
 * future driver change that adds non-NaN cases with prec=0 (impossible
 * under the current schema, but defensively) can't accidentally emit
 * 0 for a non-NaN. */
static inline void emit_case_kind(FILE *out, const char *tag,
                                  mpfr_srcptr x, int is_nan) {
    const uint64_t t0 = now_ns();
    const mpfr_prec_t p = mpfr_get_prec(x);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_end_inputs(out);
    /* NaN: TS port returns 0n unconditionally (mpfr_storage_traps.md §3).
     * Other kinds: emit the libmpfr observable, which equals x.prec on
     * the TS side. */
    if (is_nan) {
        jl_output_scalar_i64(out, 0);
    } else {
        jl_output_scalar_i64(out, (int64_t)p);
    }
    jl_finish(out, elapsed);
}

/* Convenience: emit a normal MPFR (initialized via mpfr_set_d) at a
 * given prec. */
static inline void emit_normal(FILE *out, const char *tag,
                               double d, uint64_t prec) {
    assert(prec >= TS_PREC_MIN && prec <= MAX_ALLOC_PREC);
    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
    emit_case_kind(out, tag, x, 0);
    mpfr_clear(x);
}

/* Convenience: emit a ±0 / ±Inf at prec. */
static inline void emit_zero(FILE *out, const char *tag,
                             uint64_t prec, int sign) {
    assert(prec >= TS_PREC_MIN && prec <= MAX_ALLOC_PREC);
    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_zero(x, sign);
    emit_case_kind(out, tag, x, 0);
    mpfr_clear(x);
}

static inline void emit_inf(FILE *out, const char *tag,
                            uint64_t prec, int sign) {
    assert(prec >= TS_PREC_MIN && prec <= MAX_ALLOC_PREC);
    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_inf(x, sign);
    emit_case_kind(out, tag, x, 0);
    mpfr_clear(x);
}

/* Convenience: emit a NaN at prec. The libmpfr-side prec is irrelevant
 * (jl_kv_mpfr canonicalises NaN to a fixed sentinel record on the
 * wire — kind=nan, sign=1, prec=0, exp=0, mant=0 — see common.h L396-
 * L399), so the wire-shape `x` is always the canonical NaN regardless
 * of which prec was used to allocate the libmpfr side. */
static inline void emit_nan(FILE *out, const char *tag, uint64_t prec) {
    assert(prec >= TS_PREC_MIN && prec <= MAX_ALLOC_PREC);
    mpfr_t x;
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_nan(x);
    emit_case_kind(out, tag, x, 1);
    mpfr_clear(x);
}

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 25 cases — typical precs across normal / zero / inf.    */
    /* No NaN here (NaN is the "edge" headline case). */
    /* ============================================================== */
    {
        const uint64_t precs[] = {
            24, 32, 53, 64, 80, 100, 113, 128, 200, 256, 512, 1024, 2048,
        };
        const size_t n = sizeof precs / sizeof precs[0];

        /* 13 normal values at distinct precs. */
        for (size_t i = 0; i < n; ++i) {
            emit_normal(out, "happy", 3.14, precs[i]);
        }
        /* 6 zero values × signs. */
        emit_zero(out, "happy", 53, +1);
        emit_zero(out, "happy", 53, -1);
        emit_zero(out, "happy", 64, +1);
        emit_zero(out, "happy", 64, -1);
        emit_zero(out, "happy", 1024, +1);
        emit_zero(out, "happy", 2048, -1);
        /* 6 inf values × signs. */
        emit_inf(out, "happy", 53, +1);
        emit_inf(out, "happy", 53, -1);
        emit_inf(out, "happy", 64, +1);
        emit_inf(out, "happy", 64, -1);
        emit_inf(out, "happy", 1024, +1);
        emit_inf(out, "happy", 2048, -1);
    }

    /* ============================================================== */
    /* edge: 50 cases — PREC_MIN / large-prec bookends, ±0, ±Inf, NaN. */
    /* The NaN cases exercise the documented TS divergence (always 0). */
    /* ============================================================== */
    {
        /* Small precs across all four kinds. */
        emit_normal(out, "edge", 1.0, 1);       /* PREC_MIN, normal */
        emit_normal(out, "edge", 1.0, 2);
        emit_normal(out, "edge", 1.0, 3);
        emit_normal(out, "edge", 1.5, 4);
        emit_normal(out, "edge", 1.5, 5);

        emit_zero(out, "edge", 1, +1);          /* PREC_MIN, ±0 */
        emit_zero(out, "edge", 1, -1);
        emit_zero(out, "edge", 2, +1);
        emit_zero(out, "edge", 2, -1);

        emit_inf(out, "edge", 1, +1);           /* PREC_MIN, ±Inf */
        emit_inf(out, "edge", 1, -1);
        emit_inf(out, "edge", 2, +1);
        emit_inf(out, "edge", 2, -1);

        /* NaN at varying precs — wire records canonicalise to the same
         * shape (prec=0, sign=1), but they're emitted from different
         * libmpfr alloc sizes to confirm the canonicalisation. The
         * TS-expected output is always 0. */
        emit_nan(out, "edge", 1);
        emit_nan(out, "edge", 2);
        emit_nan(out, "edge", 53);
        emit_nan(out, "edge", 64);
        emit_nan(out, "edge", 100);
        emit_nan(out, "edge", 1024);
        emit_nan(out, "edge", 4096);

        /* Limb-boundary neighbourhoods, normal values. */
        emit_normal(out, "edge", 1.5, 63);
        emit_normal(out, "edge", 1.5, 64);
        emit_normal(out, "edge", 1.5, 65);
        emit_normal(out, "edge", 1.5, 127);
        emit_normal(out, "edge", 1.5, 128);
        emit_normal(out, "edge", 1.5, 129);

        /* Limb boundaries on zero / inf. */
        emit_zero(out, "edge", 64, +1);
        emit_zero(out, "edge", 128, -1);
        emit_inf(out, "edge", 64, +1);
        emit_inf(out, "edge", 128, -1);

        /* Mid-range precs. */
        emit_normal(out, "edge", 2.718, 200);
        emit_normal(out, "edge", 2.718, 500);
        emit_normal(out, "edge", 2.718, 1500);

        /* Larger precs (within MAX_ALLOC_PREC). */
        emit_normal(out, "edge", 1.0, 3000);
        emit_normal(out, "edge", 1.0, 4096);
        emit_zero(out, "edge", 4096, +1);
        emit_inf(out, "edge", 4096, -1);

        /* Same prec across kinds (proves the prec read is kind-agnostic
         * for non-NaN). */
        emit_normal(out, "edge", 3.0, 256);
        emit_zero(out, "edge", 256, +1);
        emit_inf(out, "edge", 256, +1);

        /* Signed-zero observability check: ±0 carry the SAME prec
         * regardless of sign. Two paired cases. */
        emit_zero(out, "edge", 113, +1);
        emit_zero(out, "edge", 113, -1);

        /* Signed-inf observability check: ±Inf carry the SAME prec. */
        emit_inf(out, "edge", 113, +1);
        emit_inf(out, "edge", 113, -1);

        /* Pivot points. */
        emit_normal(out, "edge", 1.0, 50);
        emit_normal(out, "edge", 1.0, 51);
        emit_normal(out, "edge", 1.0, 52);
        emit_normal(out, "edge", 1.0, 53);
        emit_normal(out, "edge", 1.0, 54);
    }

    /* ============================================================== */
    /* adversarial: 15 cases — primes / 2^k / odd precs that a naive   */
    /* port might branch on. All in-range; Rule 7 forbids "expected     */
    /* throw" cases.                                                   */
    /* ============================================================== */
    {
        /* PREC_MIN at all four kinds. */
        emit_normal(out, "adversarial", 1.0, TS_PREC_MIN);
        emit_zero(out, "adversarial", TS_PREC_MIN, +1);
        emit_inf(out, "adversarial", TS_PREC_MIN, -1);
        emit_nan(out, "adversarial", TS_PREC_MIN);

        /* Primes. */
        emit_normal(out, "adversarial", 1.5, 53);
        emit_normal(out, "adversarial", 1.5, 127);
        emit_normal(out, "adversarial", 1.5, 1009);
        emit_normal(out, "adversarial", 1.5, 65521);  /* largest prime < 2^16 */

        /* Powers of two. */
        emit_normal(out, "adversarial", 1.0, 64);
        emit_normal(out, "adversarial", 1.0, 256);
        emit_normal(out, "adversarial", 1.0, 1024);

        /* 2^k ±1 — limb-rounding boundary. */
        emit_normal(out, "adversarial", 1.0, 63);
        emit_normal(out, "adversarial", 1.0, 65);

        /* NaN regardless of libmpfr-side alloc prec — expected output is
         * still 0. */
        emit_nan(out, "adversarial", 65521);
        emit_nan(out, "adversarial", 4096);
    }

    /* ============================================================== */
    /* fuzz: 60 cases — PRNG-driven (prec ∈ [1, 4096], all kinds).     */
    /* ============================================================== */
    {
        /* Seed picked for hex-pun visibility: "GET PREC NAB DECAFE"-ish.
         * Distinct from neg's seed so the streams don't entangle. */
        xs64_t rng;
        xs64_seed(&rng, 0x9E7997EC9AB0DECAULL);

        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 4096);  /* [1, 4096] */
            const uint64_t kind = xs64_below(&rng, 4);
            const int sign = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            switch (kind) {
                case 0: emit_normal(out, "fuzz", 1.5, prec); break;
                case 1: emit_zero  (out, "fuzz", prec, sign); break;
                case 2: emit_inf   (out, "fuzz", prec, sign); break;
                default: emit_nan  (out, "fuzz", prec); break;
            }
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — mpfr_get_prec usages from upstream tests.       */
    /*                                                                  */
    /* Most upstream usages of mpfr_get_prec are inside larger          */
    /* algorithms; the isolatable triples are "init at p, immediately   */
    /* read prec, expect p" — which we transcribe here. Found via       */
    /* grep -rn 'mpfr_get_prec' mpfr/tests/ | head.                     */
    /* ============================================================== */
    {
        /* mpfr/tests/tpow.c — typical read-after-init pattern at 53. */
        emit_normal(out, "mined", 2.0, 53);
        /* mpfr/tests/tdiv.c — read at IEEE float64 precision. */
        emit_normal(out, "mined", 1.0, 53);
        /* mpfr/tests/tcos.c — read at IEEE float128. */
        emit_normal(out, "mined", 1.0, 113);
        /* mpfr/tests/tlog.c — read at a non-standard prec. */
        emit_normal(out, "mined", 1.0, 100);
        /* mpfr/tests/tinits.c-style — read PREC_MIN. */
        emit_normal(out, "mined", 1.0, 1);
    }

    return 0;
}
