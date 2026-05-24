/*
 * golden_driver.c — Golden master for MPFR's mpfr_set_inf.
 *
 * C signature
 * -----------
 *
 *   void mpfr_set_inf(mpfr_ptr x, int sign);
 *
 *   Mutates x to ±Inf at x's pre-existing precision. sign >= 0 →
 *   +Inf; sign < 0 → -Inf. See mpfr/src/set_inf.c.
 *
 * Divergence from C → TS
 * ----------------------
 *
 * TS port `mpfr_set_inf(prec, sign) -> MPFR` takes prec and sign as
 * positional arguments and returns a bare MPFR. The TS surface
 * restricts sign to a strict Sign (1 | -1); we therefore only emit
 * sign ∈ {-1, +1} cases.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"prec":"<decimal>","sign":<1|-1>},
 *    "output":{"kind":"inf","sign":<1|-1>,"prec":"<decimal>","exp":"0","mant":"0"},
 *    "time_ns":<n>}
 *
 *   - prec via jl_kv_u64 — decimal-string BigInt round-trip.
 *   - sign via jl_kv_int — bare JS number.
 *
 * Tag distribution mirrors mpfr_set_zero — same shape op (signed
 * singular value × prec):
 *
 *   happy        :  ~30
 *   edge         :  ~64
 *   adversarial  :  ~20
 *   fuzz         :   60
 *   mined        :    6
 *   ------------ ----
 *   total        : ~180
 *
 * Build via eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/set_inf.c — the C reference.
 * Ref: src/ops/set_inf.ts — the production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_set_inf golden_driver requires GMP_NUMB_BITS == 64"
#endif

#define TS_PREC_MAX ((uint64_t)((1ULL << 31) - 257ULL))
#define TS_PREC_MIN ((uint64_t)1)

static inline void emit_case(FILE *out, const char *tag,
                             uint64_t prec, int sign) {
    assert(prec >= TS_PREC_MIN && prec <= TS_PREC_MAX);
    assert(sign == 1 || sign == -1);
    mpfr_t probe;
    mpfr_init2(probe, (mpfr_prec_t)prec);

    const uint64_t t0 = now_ns();
    mpfr_set_inf(probe, sign);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_u64(out, 1, "prec", prec);
    jl_kv_int(out, 0, "sign", sign);
    jl_end_inputs(out);
    jl_output_mpfr(out, probe);
    jl_finish(out, elapsed);

    mpfr_clear(probe);
}

static inline void emit_both(FILE *out, const char *tag, uint64_t prec) {
    emit_case(out, tag, prec, +1);
    emit_case(out, tag, prec, -1);
}

int main(void) {
    FILE *out = stdout;

    /* happy: 30 cases. */
    {
        const uint64_t fixed[] = {
            24, 32, 53, 64, 80, 100, 113, 128, 200, 256, 512, 1024, 2048,
        };
        const size_t n_fixed = sizeof fixed / sizeof fixed[0];
        for (size_t i = 0; i < n_fixed; ++i) {
            emit_both(out, "happy", fixed[i]);
        }
        xs64_t rng;
        xs64_seed(&rng, 0xCAB0058E1E10ABCDULL);
        for (int rep = 0; rep < 4; ++rep) {
            emit_both(out, "happy", 2 + xs64_below(&rng, 2047));
        }
    }

    /* edge: 64 cases. */
    {
        emit_both(out, "edge", 1);
        emit_both(out, "edge", 2);
        emit_both(out, "edge", 3);
        emit_both(out, "edge", 4);
        emit_both(out, "edge", 8);

        emit_both(out, "edge", 63);
        emit_both(out, "edge", 64);
        emit_both(out, "edge", 65);
        emit_both(out, "edge", 127);
        emit_both(out, "edge", 128);
        emit_both(out, "edge", 129);

        emit_both(out, "edge", 52);
        emit_both(out, "edge", 53);
        emit_both(out, "edge", 54);

        emit_both(out, "edge", TS_PREC_MAX);
        emit_both(out, "edge", TS_PREC_MAX - 1);
        emit_both(out, "edge", TS_PREC_MAX - 100);
        emit_both(out, "edge", TS_PREC_MAX / 2);
        emit_both(out, "edge", TS_PREC_MAX / 4);

        emit_both(out, "edge", 4096);
        emit_both(out, "edge", 8192);
        emit_both(out, "edge", 65536);
        emit_both(out, "edge", 131072);
        emit_both(out, "edge", 1048576);

        emit_both(out, "edge", 100);
        emit_both(out, "edge", 200);
        emit_both(out, "edge", 300);
        emit_both(out, "edge", 1000);
        emit_both(out, "edge", 1500);

        emit_both(out, "edge", 12);
        emit_both(out, "edge", 16);
    }

    /* adversarial: 20 cases. */
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
        xs64_seed(&rng, 0xFEEDFACE5170E4DEULL);
        for (int rep = 0; rep < 60; ++rep) {
            const uint64_t prec = 1 + xs64_below(&rng, 2048);
            const int sign = (xs64_below(&rng, 2) == 0) ? +1 : -1;
            emit_case(out, "fuzz", prec, sign);
        }
    }

    /* mined: 6 cases — from mpfr/tests/tset.c. */
    {
        emit_case(out, "mined", 53, +1);
        emit_case(out, "mined", 53, -1);
        emit_case(out, "mined", 64, +1);
        emit_case(out, "mined", 64, -1);
        emit_case(out, "mined", 1, +1);
        emit_case(out, "mined", 1, -1);
    }

    return 0;
}
