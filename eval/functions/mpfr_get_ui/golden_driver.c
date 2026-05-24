/*
 * golden_driver.c — Golden master for MPFR's mpfr_get_ui.
 *
 * C signature
 * -----------
 *
 *   unsigned long mpfr_get_ui(mpfr_srcptr op, mpfr_rnd_t rnd);
 *
 *   Rounds `op` to an unsigned long per `rnd`. Returns 0/ULONG_MAX +
 *   sets ERANGE on NaN / Inf / out-of-range / negative-not-rounding-to-0.
 *
 * TS divergence
 * -------------
 *
 * TS port throws on the ERANGE cases. The "negative value that rounds
 * to 0" subcase RETURNS 0n successfully (consistent with the C
 * `mpfr_fits_ulong_p` contract — see mpfr/src/fits_u.h L39–L43).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":<MPFR-record>,"rnd":"RND[NZUDA]"},
 *    "output":"<uint64-decimal>",
 *    "time_ns":<n>}
 *
 *   - `output` is jl_output_scalar_u64 (quoted unsigned decimal string).
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_get_ui golden_driver requires GMP_NUMB_BITS == 64"
#endif

_Static_assert(sizeof(unsigned long) == 8, "mpfr_get_ui golden requires 64-bit unsigned long");

/* Emit one mpfr_get_ui case if `x` fits unsigned long per `rnd`.
 * Returns 1 on emit, 0 on skip (NaN / +Inf / overflow / negative-
 * not-rounding-to-0 all skip). */
static inline int emit_case(FILE *out, const char *tag,
                            mpfr_srcptr x, mpfr_rnd_t rnd) {
    if (!mpfr_fits_ulong_p(x, rnd)) {
        return 0;
    }
    const uint64_t t0 = now_ns();
    const unsigned long result = mpfr_get_ui(x, rnd);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_rnd(out, 0, "rnd", rnd);
    jl_end_inputs(out);
    jl_output_scalar_u64(out, (uint64_t)result);
    jl_finish(out, elapsed);
    return 1;
}

static inline int emit_d(FILE *out, const char *tag,
                         double d, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
    const int emitted = emit_case(out, tag, x, rnd);
    mpfr_clear(x);
    return emitted;
}

static inline int emit_ui(FILE *out, const char *tag,
                          unsigned long n, uint64_t prec, mpfr_rnd_t rnd) {
    mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_ui(x, n, MPFR_RNDN);
    const int emitted = emit_case(out, tag, x, rnd);
    mpfr_clear(x);
    return emitted;
}

int main(void) {
    FILE *out = stdout;
    const mpfr_rnd_t RNDS[5] = {MPFR_RNDN, MPFR_RNDZ, MPFR_RNDU, MPFR_RNDD, MPFR_RNDA};

    /* ============================================================== */
    /* happy: ~25.                                                    */
    /* ============================================================== */
    {
        emit_ui(out, "happy", 0UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 1UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 2UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 10UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 100UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 1000UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 1000000UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 42UL, 24, MPFR_RNDN);
        emit_ui(out, "happy", 42UL, 64, MPFR_RNDN);
        emit_ui(out, "happy", 1234567UL, 100, MPFR_RNDN);
        emit_ui(out, "happy", 1234567UL, 200, MPFR_RNDN);
        emit_ui(out, "happy", 3UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 5UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 7UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 255UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 256UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 65535UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 65536UL, 53, MPFR_RNDN);
        emit_ui(out, "happy", 1UL << 32, 53, MPFR_RNDN);
        emit_ui(out, "happy", (1UL << 32) - 1, 53, MPFR_RNDN);
        emit_ui(out, "happy", 1UL << 40, 64, MPFR_RNDN);
        emit_ui(out, "happy", 1UL << 50, 64, MPFR_RNDN);
        emit_ui(out, "happy", 1UL << 60, 64, MPFR_RNDN);
        emit_ui(out, "happy", 1UL << 63, 64, MPFR_RNDN);
        emit_ui(out, "happy", 1234567890UL, 53, MPFR_RNDN);
    }

    /* ============================================================== */
    /* edge: ~50.                                                     */
    /* ============================================================== */
    {
        /* (1-2) ±0. */
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1);
            emit_case(out, "edge", x, MPFR_RNDN);
            mpfr_clear(x);
        }
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, -1);
            emit_case(out, "edge", x, MPFR_RNDN);
            mpfr_clear(x);
        }

        /* (3-7) 1 across all 5 rnd modes. */
        for (int i = 0; i < 5; ++i) emit_ui(out, "edge", 1UL, 53, RNDS[i]);

        /* (8-12) ULONG_MAX across all 5 rnd modes at prec=64. */
        for (int i = 0; i < 5; ++i) emit_ui(out, "edge", ULONG_MAX, 64, RNDS[i]);

        /* (13-17) Half-integer 0.5. */
        for (int i = 0; i < 5; ++i) emit_d(out, "edge", 0.5, 53, RNDS[i]);

        /* (18-22) Half-integer 1.5. */
        for (int i = 0; i < 5; ++i) emit_d(out, "edge", 1.5, 53, RNDS[i]);

        /* (23-27) 2.5 — RNDN ties to even. */
        for (int i = 0; i < 5; ++i) emit_d(out, "edge", 2.5, 53, RNDS[i]);

        /* (28-32) Negative-rounds-to-0: -0.4 with RNDN, -0.1 with all
         * modes that round toward zero. fits_ulong_p decides. */
        emit_d(out, "edge", -0.1, 53, MPFR_RNDN);
        emit_d(out, "edge", -0.1, 53, MPFR_RNDZ);
        emit_d(out, "edge", -0.1, 53, MPFR_RNDU);  /* rounds to 0 — fits */
        emit_d(out, "edge", -0.4, 53, MPFR_RNDN);  /* rounds to 0 — fits */
        emit_d(out, "edge", -0.49, 53, MPFR_RNDN); /* rounds to 0 — fits */

        /* (33-37) 0 < x < 1 — round-up scenarios. */
        emit_d(out, "edge", 0.1, 53, MPFR_RNDN);
        emit_d(out, "edge", 0.25, 53, MPFR_RNDN);
        emit_d(out, "edge", 0.25, 53, MPFR_RNDU);
        emit_d(out, "edge", 0.5, 53, MPFR_RNDU);
        emit_d(out, "edge", 0.9, 53, MPFR_RNDN);

        /* (38-42) Power-of-2 stress. */
        emit_ui(out, "edge", 1UL << 31, 53, MPFR_RNDN);
        emit_ui(out, "edge", 1UL << 32, 53, MPFR_RNDN);
        emit_ui(out, "edge", 1UL << 53, 53, MPFR_RNDN);
        emit_ui(out, "edge", 1UL << 53, 64, MPFR_RNDN);
        emit_ui(out, "edge", 1UL << 63, 64, MPFR_RNDN);

        /* (43-50) Just past 2^53 / various boundaries. */
        emit_ui(out, "edge", (1UL << 53) + 1, 64, MPFR_RNDN);
        emit_ui(out, "edge", (1UL << 53) + 7, 64, MPFR_RNDN);
        emit_ui(out, "edge", (1UL << 63) - 1, 64, MPFR_RNDN);
        emit_ui(out, "edge", (1UL << 63) + 1, 64, MPFR_RNDN);
        emit_ui(out, "edge", ULONG_MAX - 1, 64, MPFR_RNDN);
        emit_ui(out, "edge", ULONG_MAX, 64, MPFR_RNDZ);
        emit_ui(out, "edge", ULONG_MAX, 64, MPFR_RNDD);
        emit_ui(out, "edge", 1UL, 1, MPFR_RNDN);
    }

    /* ============================================================== */
    /* adversarial: ~30.                                              */
    /* ============================================================== */
    {
        const double vals[] = { 1.4, 1.5, 1.6, 2.5, 3.5, 4.5 };
        const size_t n_vals = sizeof(vals) / sizeof(vals[0]);
        for (size_t v = 0; v < n_vals; ++v) {
            for (int r = 0; r < 5; ++r) {
                emit_d(out, "adversarial", vals[v], 53, RNDS[r]);
            }
        }

        /* Near-ULONG_MAX values that DO fit. */
        emit_ui(out, "adversarial", ULONG_MAX - 1, 64, MPFR_RNDN);
        emit_ui(out, "adversarial", ULONG_MAX - 1, 64, MPFR_RNDD);
        emit_ui(out, "adversarial", ULONG_MAX, 64, MPFR_RNDZ);
        emit_ui(out, "adversarial", ULONG_MAX, 64, MPFR_RNDD);

        /* Negative-rounds-to-0 cases under various rnd modes. */
        emit_d(out, "adversarial", -0.5, 53, MPFR_RNDU);     /* RNDU on -0.5 → 0 */
        emit_d(out, "adversarial", -0.5, 53, MPFR_RNDZ);     /* RNDZ on -0.5 → 0 */
        emit_d(out, "adversarial", -0.3, 53, MPFR_RNDN);     /* RNDN on -0.3 → 0 */
    }

    /* ============================================================== */
    /* fuzz: 55.                                                      */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0x0B0CC1A20B0CC1A2ULL);
        const uint64_t precs[5] = { 53, 64, 100, 200, 300 };

        int emitted = 0;
        int tries = 0;
        while (emitted < 55 && tries < 200) {
            tries++;
            const uint64_t u = xs64_next(&rng);
            const uint64_t prec = precs[xs64_below(&rng, 5)];
            const mpfr_rnd_t rnd = RNDS[xs64_below(&rng, 5)];
            const int frac = (int)(xs64_next(&rng) & 1);

            mpfr_t x; mpfr_init2(x, (mpfr_prec_t)prec);
            /* Bias toward in-range positives: mask to 60 bits to keep
             * most values inside ULONG_MAX. */
            const unsigned long n = (unsigned long)(u & ((1UL << 60) - 1));
            mpfr_set_ui(x, n, MPFR_RNDN);

            if (frac) {
                double scale = 0.5 + ((double)xs64_below(&rng, 11)) * 0.1;
                mpfr_mul_d(x, x, scale, MPFR_RNDN);
            }

            if (emit_case(out, "fuzz", x, rnd)) emitted++;
            mpfr_clear(x);
        }
    }

    /* ============================================================== */
    /* mined: 5 from mpfr/tests/tget_si.c (covers both signed/unsigned). */
    /* ============================================================== */
    {
        {
            mpfr_t x; mpfr_init2(x, 53); mpfr_set_zero(x, +1);
            emit_case(out, "mined", x, MPFR_RNDN);
            mpfr_clear(x);
        }
        emit_ui(out, "mined", 1UL, 53, MPFR_RNDN);
        emit_ui(out, "mined", 1024UL, 53, MPFR_RNDN);
        emit_ui(out, "mined", ULONG_MAX, 64, MPFR_RNDN);
        /* Negative-rounds-to-0 case. */
        emit_d(out, "mined", -0.4, 53, MPFR_RNDN);
    }

    return 0;
}
