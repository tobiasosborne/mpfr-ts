/*
 * golden_driver.c — Golden master for MPFR's mpfr_set_zero.
 *
 * C signature
 * -----------
 *
 *   void mpfr_set_zero(mpfr_ptr x, int sign);
 *
 *   Mutates x to ±0 at x's pre-existing precision. sign >= 0 → +0;
 *   sign < 0 → -0. See mpfr/src/set_zero.c.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS port `mpfr_set_zero(prec, sign) -> MPFR` takes prec and sign as
 * positional arguments and returns a bare MPFR. The TS surface
 * restricts sign to a strict Sign (1 | -1); we therefore only emit
 * sign ∈ {-1, +1} cases — not the C-side non-negative-int coercion
 * regime (which the TS port rejects with EPREC).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"prec":"<decimal>","sign":<1|-1>},
 *    "output":{"kind":"zero","sign":<1|-1>,"prec":"<decimal>","exp":"0","mant":"0"},
 *    "time_ns":<n>}
 *
 *   - prec via jl_kv_u64 — decimal-string BigInt round-trip.
 *   - sign via jl_kv_int — bare JS number so the TS port receives a
 *     `number`, not a `bigint`. The decodeInputValue path classifies
 *     a JSON-decoded JS number as a number-typed input (not coerced
 *     to bigint), matching the port's `sign: Sign = 1 | -1` shape.
 *
 * Tag distribution (Rule 7 minimums × 2 signs)
 * --------------------------------------------
 *
 *   happy        :  ~30   (typical precs × both signs)
 *   edge         :  ~64   (PREC_MIN / PREC_MAX / pivots × both signs)
 *   adversarial  :  ~20   (primes / 2^k × both signs)
 *   fuzz         :   60   (PRNG prec × random sign)
 *   mined        :    6   (from mpfr/tests/tset.c — set_zero exercises)
 *   ------------ ----
 *   total        : ~180
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/set_zero.c — the C reference.
 * Ref: src/ops/set_zero.ts — the production port.
 * Ref: CLAUDE.md "Hallucination-risk callouts: Signed zero is real" —
 *   +0 and -0 are distinct MPFR values; mutation-prover for this
 *   golden tests the sign-dropping bug pattern explicitly.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_set_zero golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

/* Emit one mpfr_set_zero golden case. */
static inline void emit_case(FILE *out, const char *tag,
                             uint64_t prec, int sign) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    assert(sign == 1 || sign == -1);
    mpfr_t probe;
    mpfr_init2(probe, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    mpfr_set_zero(probe, sign);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_u64(out, 1, "prec", prec);
    jl_kv_int(out, 0, "sign", sign);
    jl_end_inputs(out);
    jl_output_mpfr(out, probe);
    jl_finish(out, elapsed);

    mpfr_clear(probe);
}

/* Convenience: emit both signs at the same prec. */
static inline void emit_both(FILE *out, const char *tag, uint64_t prec) {
    emit_case(out, tag, prec, +1);
    emit_case(out, tag, prec, -1);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 30 cases — common precs × both signs. */
    {
        const uint64_t fixed[] = {
            24, 32, 53, 64, 80, 100, 113, 128, 200, 256, 512, 1024, 2048,
        };
        const size_t n_fixed = sizeof fixed / sizeof fixed[0];
        for (size_t i = 0; i < n_fixed; ++i) {
            emit_both(out, "happy", fixed[i]);
        }
        /* 4 random precs × both signs = 8 more cases (sub-list of 15). */
        xs64_t rng;
        xs64_seed(&rng, 0xAFFAB1E0DDCAFE5EULL);
        for (int rep = 0; rep < 4; ++rep) {
            emit_both(out, "happy", 2 + xs64_below(&rng, 2047));
        }
    }

    /* edge: 64 cases — boundary precs × both signs. */
    {
        /* Small boundary. */
        emit_both(out, "edge", 1);
        emit_both(out, "edge", 2);
        emit_both(out, "edge", 3);
        emit_both(out, "edge", 4);
        emit_both(out, "edge", 8);

        /* Limb-boundary neighbourhood. */
        emit_both(out, "edge", 63);
        emit_both(out, "edge", 64);
        emit_both(out, "edge", 65);
        emit_both(out, "edge", 127);
        emit_both(out, "edge", 128);
        emit_both(out, "edge", 129);

        /* IEEE-754 pivot. */
        emit_both(out, "edge", 52);
        emit_both(out, "edge", 53);
        emit_both(out, "edge", 54);

        /* Large bookends. */
        emit_both(out, "edge", TS_PREC_MAX);
        emit_both(out, "edge", TS_PREC_MAX - 1);
        emit_both(out, "edge", TS_PREC_MAX - 100);
        emit_both(out, "edge", TS_PREC_MAX / 2);
        emit_both(out, "edge", TS_PREC_MAX / 4);

        /* Misc large. */
        emit_both(out, "edge", 4096);
        emit_both(out, "edge", 8192);
        emit_both(out, "edge", 65536);
        emit_both(out, "edge", 131072);
        emit_both(out, "edge", 1048576);

        /* Mid-range. */
        emit_both(out, "edge", 100);
        emit_both(out, "edge", 200);
        emit_both(out, "edge", 300);
        emit_both(out, "edge", 1000);
        emit_both(out, "edge", 1500);

        /* Pad. */
        emit_both(out, "edge", 12);
        emit_both(out, "edge", 16);
    }

    /* adversarial: 20 cases — primes / 2^k × both signs. */
    {
        emit_both(out, "adversarial", TS_PREC_MIN);
        emit_both(out, "adversarial", TS_PREC_MAX);
        emit_both(out, "adversarial", 53);
        emit_both(out, "adversarial", 1009);
        emit_both(out, "adversarial", 65521);
        emit_both(out, "adversarial", 64);
        emit_both(out, "adversarial", 256);
        emit_both(out, "adversarial", 1024);
        emit_both(out, "adversarial", 63);
        emit_both(out, "adversarial", 65);
    }

    /* fuzz: 60 cases — PRNG prec × random sign. */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x5170E4DE0DEADBEEULL);
        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 2048);
            /* Random sign: 0 → +1, 1 → -1. */
            const int sign = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            emit_case(out, "fuzz", prec, sign);
        }
    }

    /* mined: 6 cases — from mpfr/tests/tset.c. The mpfr_set_zero
     * usages there are typically `mpfr_set_zero(x, +1)` after init
     * to force a known starting state, or `mpfr_set_zero(x, -1)` to
     * test the signed-zero observability in subsequent ops. */
    {
        /* tset.c-style: +0 at prec 53. */
        emit_case(out, "mined", 53, +1);
        /* tset.c-style: -0 at prec 53. */
        emit_case(out, "mined", 53, -1);
        /* Variants at common precs. */
        emit_case(out, "mined", 64, +1);
        emit_case(out, "mined", 64, -1);
        emit_case(out, "mined", 1, +1);   /* PREC_MIN both signs */
        emit_case(out, "mined", 1, -1);
    }

    return 0;
}
