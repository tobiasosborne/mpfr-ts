/*
 * golden_driver.c — Golden master for MPFR's mpfr_init2.
 *
 * C signature
 * -----------
 *
 *   void mpfr_init2(mpfr_ptr x, mpfr_prec_t p);
 *
 * Initialises `x` to NaN at precision `p` bits, allocates the limb backing.
 * The caller MUST call `mpfr_set_*` before reading. See mpfr/src/init2.c.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * The TS port `mpfr_init2(prec) -> MPFR` returns `posZero(prec)` — a
 * deterministic `+0` at the requested precision — because the immutable
 * TS surface cannot encode "uninitialised mantissa". This driver therefore
 * emits the *expected TS output* (a +0 bare-MPFR record) rather than what
 * libmpfr's `mpfr_init2` literally produces (NaN).
 *
 * To still validate that the precision is acceptable to libmpfr (i.e. the
 * golden does not accidentally include precisions libmpfr would
 * MPFR_ASSERTN-abort on), each case does:
 *
 *   1. mpfr_init2(probe, prec)         — proves libmpfr accepts `prec`.
 *   2. mpfr_set_zero(probe, +1)        — overwrite NaN with +0 at `prec`.
 *   3. emit jl_output_mpfr(probe)      — wire the resulting +0 record.
 *   4. mpfr_clear(probe).
 *
 * Step 1 is the libmpfr accept-test; steps 2–3 construct the *TS-port-
 * expected* output. The grader compares the TS port's `mpfr_init2(prec)`
 * (which is `posZero(prec)`) against this +0 wire record.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>","inputs":{"prec":"<decimal>"},
 *    "output":{"kind":"zero","sign":1,"prec":"<decimal>","exp":"0","mant":"0"},
 *    "time_ns":<n>}
 *
 *   - `prec` is emitted as a decimal string via jl_kv_u64 so the TS
 *     decoder turns it into a BigInt natively (the port signature is
 *     `(prec: bigint) -> MPFR`).
 *   - `output` is the bare MPFR record (jl_output_mpfr) — no ternary,
 *     because the construction is exact.
 *
 * Tag distribution
 * ----------------
 *
 *   happy        :  35  (typical precisions used in real code)
 *   edge         :  31  (boundaries — PREC_MIN, very small, pivots, large)
 *   adversarial  :  15  (off-by-one neighbourhoods, primes, powers of two)
 *   fuzz         :  75  (PRNG; n in [1, 2048])
 *   ------------ ----
 *   total        : 156
 *
 * No `mined` tag: mpfr/tests/tinit.c exercises mpfr_init2 only as the
 * constructor inside larger init+set sequences — those aren't isolatable
 * as `mpfr_init2`-only test triples. Rule 7's "or all available from
 * mpfr/tests/<fn>.c, if fewer than 5 exist" applies; 0 mined is the
 * available count.
 *
 * Precision cap
 * -------------
 *
 * The golden uses precisions in `[1, 2^31 - 257]` — i.e. matching
 * `src/core.ts` PREC_MAX. The C-side `MPFR_PREC_MAX` may be larger on
 * 64-bit `_MPFR_PREC_FORMAT` builds, but the TS schema caps at the 32-bit
 * value (see src/core.ts L220–L236), so the golden mirrors the TS limit.
 *
 * Ref: mpfr/src/init2.c — the C reference.
 * Ref: mpfr/src/mpfr.h L191–L193 — PREC bounds.
 * Ref: src/core.ts L216–L236 — PREC_MIN / PREC_MAX.
 * Ref: src/ops/init2.ts — the production port.
 *
 * Build
 * -----
 *
 *   gcc -O2 -std=c11 -Wall -Wextra -Werror -I../../golden_master \
 *       golden_driver.c $(pkg-config --cflags --libs mpfr) -lgmp -lm \
 *       -o golden_driver
 *
 * The repo-wide eval/golden_master/build.sh finds and builds this file
 * automatically.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

/* ------------------------------------------------------------------ */
/* Compile-time invariants                                            */
/* ------------------------------------------------------------------ */

/* mp_limb_t must be exactly 64 bits to match the TS port's BigInt
 * mantissa convention. */
#if GMP_NUMB_BITS != 64
#  error "mpfr_init2 golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* PREC_MAX matches src/core.ts L236: 2^31 - 257. We use uint64_t for
 * the prec values so the same constant survives without overflow
 * regardless of mpfr_prec_t's signed underlying width. */
#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* ------------------------------------------------------------------ */
/* Per-case emitter                                                   */
/* ------------------------------------------------------------------ */

/* Emit one case for a given precision.
 *
 * Steps:
 *   1. Call mpfr_init2(probe, prec) — this is the libmpfr accept-test;
 *      a precision libmpfr couldn't represent would abort here. We
 *      time only this call to populate `time_ns`, since that is the
 *      operation the TS port mirrors.
 *   2. mpfr_set_zero(probe, +1) — overwrite the post-init NaN with a
 *      +0 at the same precision. This is the *TS-port-expected*
 *      output value.
 *   3. Emit the JSONL record.
 *   4. mpfr_clear.
 */
static inline void emit_case(FILE *out, const char *tag, uint64_t prec) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);

    mpfr_t probe;
    const uint64_t t0 = now_ns();
    mpfr_init2(probe, (mpfr_prec_t)prec);
    const uint64_t elapsed = now_ns() - t0;

    /* Replace post-init NaN with +0 at this precision — this is the
     * value the TS port `mpfr_init2(prec)` returns (posZero(prec)). */
    mpfr_set_zero(probe, +1);

    jl_begin(out, tag);
    jl_kv_u64(out, 1, "prec", prec);
    jl_end_inputs(out);
    jl_output_mpfr(out, probe);
    jl_finish(out, elapsed);

    mpfr_clear(probe);
}

/* ------------------------------------------------------------------ */
/* main                                                               */
/* ------------------------------------------------------------------ */

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: 25 cases — typical precisions used in real code         */
    /*                                                                */
    /* Includes IEEE-754 widths (24, 53, 113), MPFR's traditional     */
    /* defaults (64, 128, 256, ..., 4096), and a sprinkling of random */
    /* precisions in the same regime.                                 */
    /* ============================================================== */
    {
        /* Fixed list — 15 canonical precisions. */
        const uint64_t fixed[] = {
            24,    /* IEEE float32 mantissa */
            32,
            53,    /* IEEE float64 mantissa */
            64,    /* x86 long double */
            80,
            100,
            113,   /* IEEE float128 mantissa */
            128,
            200,
            256,
            512,
            768,
            1024,
            1536,
            2048,
        };
        const size_t n_fixed = sizeof fixed / sizeof fixed[0];
        for (size_t i = 0; i < n_fixed; ++i) {
            emit_case(out, "happy", fixed[i]);
        }

        /* 20 random precisions in [2, 2048] for the rest of the happy
         * budget. Seed distinct from fuzz so reproducers don't depend
         * on the fuzz stream. */
        xs64_t rng;
        xs64_seed(&rng, 0x141700ED2ULL);
        for (int rep = 0; rep < 20; ++rep) {
            const uint64_t p = 2 + xs64_below(&rng, 2047);  /* [2, 2048] */
            emit_case(out, "happy", p);
        }
    }

    /* ============================================================== */
    /* edge: 32 cases — boundary precisions                           */
    /*                                                                */
    /* Two zones: small (PREC_MIN-neighbourhood, very small generally) */
    /* and large (PREC_MAX-neighbourhood). Plus pivot-point ±1.       */
    /* ============================================================== */
    {
        /* Small boundary: PREC_MIN, PREC_MIN+1, plus 2..8 individually
         * — these stress the "single-limb" allocation path on the C
         * side. */
        emit_case(out, "edge", 1);        /* PREC_MIN */
        emit_case(out, "edge", 2);
        emit_case(out, "edge", 3);
        emit_case(out, "edge", 4);
        emit_case(out, "edge", 5);
        emit_case(out, "edge", 6);
        emit_case(out, "edge", 7);
        emit_case(out, "edge", 8);

        /* Pivot points: 50/51/52/53/54 — the IEEE-754 double-mantissa
         * neighbourhood where conversion paths often hard-code 53. */
        emit_case(out, "edge", 50);
        emit_case(out, "edge", 51);
        emit_case(out, "edge", 52);
        emit_case(out, "edge", 53);
        emit_case(out, "edge", 54);

        /* Limb-boundary neighbours: 63, 64, 65 — single vs multi-limb
         * split on 64-bit limb hosts. */
        emit_case(out, "edge", 63);
        emit_case(out, "edge", 64);
        emit_case(out, "edge", 65);

        /* Two-limb boundary: 127, 128, 129. */
        emit_case(out, "edge", 127);
        emit_case(out, "edge", 128);
        emit_case(out, "edge", 129);

        /* Large boundary: PREC_MAX, PREC_MAX-1, PREC_MAX-100, and a few
         * widely-spaced large values. These exercise the upper end
         * without burning megabytes per case. */
        emit_case(out, "edge", TS_PREC_MAX);
        emit_case(out, "edge", TS_PREC_MAX - 1);
        emit_case(out, "edge", TS_PREC_MAX - 100);
        emit_case(out, "edge", TS_PREC_MAX - 1000);
        emit_case(out, "edge", TS_PREC_MAX / 2);
        emit_case(out, "edge", TS_PREC_MAX / 4);

        /* A handful of intermediate "large but tractable" values to
         * round out to 32. */
        emit_case(out, "edge", 4096);
        emit_case(out, "edge", 8192);
        emit_case(out, "edge", 16384);
        emit_case(out, "edge", 65536);
        emit_case(out, "edge", 131072);
        emit_case(out, "edge", 1048576);  /* 2^20 */
    }

    /* ============================================================== */
    /* adversarial: 15 cases — off-by-one and structural triggers     */
    /*                                                                */
    /* All in-range — Rule 7 forbids "expected throw" cases because   */
    /* the harness can't grade them. Bias toward primes and 2^k       */
    /* boundaries that a naive port might branch on.                  */
    /* ============================================================== */
    {
        /* PREC_MIN and PREC_MAX bookends (also in edge — repetition is
         * intentional for tag class coverage). */
        emit_case(out, "adversarial", TS_PREC_MIN);
        emit_case(out, "adversarial", TS_PREC_MIN + 1);
        emit_case(out, "adversarial", TS_PREC_MAX);
        emit_case(out, "adversarial", TS_PREC_MAX - 1);

        /* Primes — non-aligned to any limb boundary. */
        emit_case(out, "adversarial", 53);
        emit_case(out, "adversarial", 127);
        emit_case(out, "adversarial", 1009);
        emit_case(out, "adversarial", 65521);  /* largest prime < 2^16 */

        /* Powers of two — limb boundaries, common branch targets. */
        emit_case(out, "adversarial", 64);
        emit_case(out, "adversarial", 128);
        emit_case(out, "adversarial", 256);
        emit_case(out, "adversarial", 512);
        emit_case(out, "adversarial", 1024);

        /* 2^k - 1 (Mersenne-shaped) and 2^k + 1 — sit-on-the-edge of
         * the prec→limbs rounding boundary. */
        emit_case(out, "adversarial", 63);
        emit_case(out, "adversarial", 65);
    }

    /* ============================================================== */
    /* fuzz: 75 cases — PRNG-driven, prec in [1, 2048]                */
    /*                                                                */
    /* Distinct seed from happy. Range deliberately stops at 2048: a  */
    /* fuzz value of 2^30 would be valid but emits megabytes per      */
    /* case, and the carry-over for the harness is the *count* of     */
    /* distinct precisions exercised, not their absolute size.        */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x101701A57E1F11ULL);
        for (int rep = 0; rep < 75; ++rep) {
            /* [1, 2048] inclusive. */
            const uint64_t p = 1 + xs64_below(&rng, 2048);
            emit_case(out, "fuzz", p);
        }
    }

    return 0;
}
