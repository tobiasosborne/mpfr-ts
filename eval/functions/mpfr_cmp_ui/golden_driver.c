/*
 * golden_driver.c — Golden master for MPFR's mpfr_cmp_ui.
 *
 * Mirrors mpfr_cmp_si's structure with `n` as `unsigned long` over
 * `[0, ULONG_MAX]`. The broken-port mutation here is "returns 0
 * always" — much harsher than cmp_si's "ignores sign" because
 * comparing 0 with a typical positive x is +1, so the broken port
 * fails almost every case. The wire and tag conventions match cmp_si.
 *
 * NaN x cases excluded (TS port throws).
 *
 * Ref: mpfr/src/cmp_ui.c — the C reference.
 * Ref: src/ops/cmp_ui.ts — the production port.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_cmp_ui golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline int normalise_cmp(int r) {
    return (r > 0) ? 1 : ((r < 0) ? -1 : 0);
}

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, unsigned long n) {
    assert(!mpfr_nan_p(x));

    const uint64_t t0 = now_ns();
    const int raw = mpfr_cmp_ui(x, n);
    const uint64_t elapsed = now_ns() - t0;
    const int result = normalise_cmp(raw);

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_u64(out, 0, "n", (uint64_t)n);
    jl_end_inputs(out);
    jl_output_scalar_int(out, result);
    jl_finish(out, elapsed);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
}

static inline void init_from_ui(mpfr_ptr x, unsigned long n, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_ui(x, n, MPFR_RNDN);
}

static inline void init_from_si(mpfr_ptr x, long n, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_si(x, n, MPFR_RNDN);
}

static inline void init_pos_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, 1); }
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, -1); }
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, 1); }
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, -1); }

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: typical cases                                            */
    /* ============================================================== */
    {
        /* Equal. */
        { mpfr_t x; init_from_ui(x, 0, 53); emit_case(out, "happy", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 1, 53); emit_case(out, "happy", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 42, 53); emit_case(out, "happy", x, 42); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 1000, 53); emit_case(out, "happy", x, 1000); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 65536, 53); emit_case(out, "happy", x, 65536); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 0xDEADBEEFUL, 53); emit_case(out, "happy", x, 0xDEADBEEFUL); mpfr_clear(x); }

        /* x < n. */
        { mpfr_t x; init_from_ui(x, 0, 53); emit_case(out, "happy", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 1, 53); emit_case(out, "happy", x, 1000); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.5, 53); emit_case(out, "happy", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 53); emit_case(out, "happy", x, 4); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -1, 53); emit_case(out, "happy", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -100, 53); emit_case(out, "happy", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -1000, 53); emit_case(out, "happy", x, ULONG_MAX); mpfr_clear(x); }

        /* x > n. */
        { mpfr_t x; init_from_ui(x, 1, 53); emit_case(out, "happy", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 1000, 53); emit_case(out, "happy", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 53); emit_case(out, "happy", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 53); emit_case(out, "happy", x, 3); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 100.5, 53); emit_case(out, "happy", x, 100); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 1UL << 30, 53); emit_case(out, "happy", x, (1UL << 30) - 1); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 1UL << 40, 53); emit_case(out, "happy", x, 1UL << 30); mpfr_clear(x); }

        /* Large equal. */
        { mpfr_t x; init_from_ui(x, 1UL << 40, 64); emit_case(out, "happy", x, 1UL << 40); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 1UL << 50, 64); emit_case(out, "happy", x, 1UL << 50); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, ULONG_MAX, 64); emit_case(out, "happy", x, ULONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e6, 53); emit_case(out, "happy", x, 1000000); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e9, 53); emit_case(out, "happy", x, 1000000000); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* edge: kind boundaries, n boundaries, signed zero               */
    /* ============================================================== */
    {
        /* ±Inf x (6). */
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, ULONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, ULONG_MAX); mpfr_clear(x); }

        /* ±0 x (6). +0 vs n=0 → 0; ±0 vs n!=0 → -1 (n is non-negative). */
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, ULONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, ULONG_MAX); mpfr_clear(x); }

        /* Negative x (always < non-negative n) (6). */
        { mpfr_t x; init_from_si(x, -1, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -1, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -LONG_MAX, 64); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -LONG_MAX, 64); emit_case(out, "edge", x, ULONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -0.5, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1e-300, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }

        /* n=0 vs positive x (5). */
        { mpfr_t x; init_from_ui(x, 1, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 1000, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.5, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e-300, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e300, 100); emit_case(out, "edge", x, 0); mpfr_clear(x); }

        /* ULONG_MAX boundary (6). */
        { mpfr_t x; init_from_ui(x, ULONG_MAX, 64); emit_case(out, "edge", x, ULONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, ULONG_MAX - 1, 64); emit_case(out, "edge", x, ULONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, ULONG_MAX, 64); emit_case(out, "edge", x, ULONG_MAX - 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e30, 128); emit_case(out, "edge", x, ULONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 0, 53); emit_case(out, "edge", x, ULONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, ULONG_MAX, 64); emit_case(out, "edge", x, 0); mpfr_clear(x); }

        /* Fractional x at various prec (7). */
        { mpfr_t x; init_from_double(x, 0.5, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.999, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.0001, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 53); emit_case(out, "edge", x, 2); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 2.5, 53); emit_case(out, "edge", x, 2); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 2.5, 53); emit_case(out, "edge", x, 3); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 100.5, 53); emit_case(out, "edge", x, 100); mpfr_clear(x); }

        /* Same value, different prec (8). */
        { mpfr_t x; init_from_ui(x, 17, 53); emit_case(out, "edge", x, 17); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 17, 128); emit_case(out, "edge", x, 17); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 17, 200); emit_case(out, "edge", x, 17); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 1, 1); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 1, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, 1, 256); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, ULONG_MAX, 64); emit_case(out, "edge", x, ULONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_from_ui(x, ULONG_MAX, 256); emit_case(out, "edge", x, ULONG_MAX); mpfr_clear(x); }

        /* prec=1 boundary (4). */
        { mpfr_t x; init_from_double(x, 1.0, 1); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.0, 1); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 2.0, 1); emit_case(out, "edge", x, 2); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.5, 1); emit_case(out, "edge", x, 1); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* adversarial: same-value-different-prec + ULP perturbation       */
    /*                                                                 */
    /* The broken-port mutation "returns 0 always" disagrees with the */
    /* correct port on every case where x != n (the vast majority).   */
    /* So the broken port comfortably fails the gate already on happy/*/
    /* edge/fuzz; the adversarial mass adds defence-in-depth against  */
    /* hypothetical alternative broken variants that pass the simpler */
    /* cases.                                                          */
    /* ============================================================== */
    {
        /* Same value across many prec pairs — equal pair, cmp_ui must return 0. */
        const struct { unsigned long n; uint64_t prec; } eq_pairs[] = {
            { 1, 1 }, { 1, 32 }, { 1, 53 }, { 1, 64 }, { 1, 100 }, { 1, 128 }, { 1, 200 }, { 1, 256 },
            { 3, 32 }, { 3, 53 }, { 3, 64 }, { 3, 128 }, { 3, 256 },
            { 5, 53 }, { 5, 128 }, { 5, 256 },
            { 7, 53 }, { 7, 128 }, { 7, 256 },
            { 11, 53 }, { 11, 128 }, { 11, 256 },
            { 13, 53 }, { 13, 128 }, { 13, 256 },
            { 15, 53 }, { 15, 128 }, { 15, 256 },
            { 17, 53 }, { 17, 128 }, { 17, 256 },
            { 31, 53 }, { 31, 256 },
            { 32, 53 }, { 32, 256 },
            { 33, 53 }, { 33, 256 },
            { 63, 53 }, { 63, 256 },
            { 64, 53 }, { 64, 256 },
            { 65, 53 }, { 65, 256 },
            { 127, 53 }, { 127, 256 },
            { 128, 53 }, { 128, 256 },
            { 1023, 53 }, { 1023, 256 },
            { 1024, 53 }, { 1024, 256 },
            { 65535, 53 }, { 65535, 256 },
            { 65537, 53 }, { 65537, 256 },
            { 0xDEADBEEFUL, 53 }, { 0xDEADBEEFUL, 256 },
            { 0xCAFEBABEUL, 53 }, { 0xCAFEBABEUL, 256 },
            { 1UL << 40, 64 }, { 1UL << 40, 256 },
            { 1UL << 50, 64 }, { 1UL << 50, 256 },
            { 1UL << 60, 64 }, { 1UL << 60, 256 },
            { ULONG_MAX, 64 }, { ULONG_MAX, 128 }, { ULONG_MAX, 256 },
            { ULONG_MAX - 1, 64 }, { ULONG_MAX - 1, 128 }, { ULONG_MAX - 1, 256 },
        };
        const size_t n_eq = sizeof(eq_pairs) / sizeof(eq_pairs[0]);
        for (size_t i = 0; i < n_eq; ++i) {
            mpfr_t x;
            init_from_ui(x, eq_pairs[i].n, eq_pairs[i].prec);
            emit_case(out, "adversarial", x, eq_pairs[i].n);
            mpfr_clear(x);
        }

        /* ULP perturbation at high prec — x = n ± 2^-100. */
        {
            const unsigned long ns[] = { 1UL, 17UL, 1000UL, 1UL << 30, 1UL << 50, ULONG_MAX / 4 };
            for (size_t i = 0; i < sizeof(ns)/sizeof(ns[0]); ++i) {
                mpfr_t x;
                mpfr_init2(x, 200);
                mpfr_set_ui(x, ns[i], MPFR_RNDN);
                mpfr_t eps; mpfr_init2(eps, 200);
                mpfr_set_ui_2exp(eps, 1, -100, MPFR_RNDN);
                mpfr_add(x, x, eps, MPFR_RNDN);
                mpfr_clear(eps);
                emit_case(out, "adversarial", x, ns[i]);
                mpfr_clear(x);
            }
            for (size_t i = 0; i < sizeof(ns)/sizeof(ns[0]); ++i) {
                mpfr_t x;
                mpfr_init2(x, 200);
                mpfr_set_ui(x, ns[i], MPFR_RNDN);
                mpfr_t eps; mpfr_init2(eps, 200);
                mpfr_set_ui_2exp(eps, 1, -100, MPFR_RNDN);
                mpfr_sub(x, x, eps, MPFR_RNDN);
                mpfr_clear(eps);
                emit_case(out, "adversarial", x, ns[i]);
                mpfr_clear(x);
            }
        }
    }

    /* ============================================================== */
    /* fuzz: 60 cases — PRNG-driven                                   */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEEC0FFEEC0FFULL);
        const uint64_t precs[5] = { 53, 64, 100, 128, 200 };

        int emitted = 0;
        while (emitted < 60) {
            const uint64_t bits_a = xs64_next(&rng);
            const uint64_t exp_a = (bits_a >> 52) & 0x7FF;
            if (exp_a == 0x7FF) continue;

            double da;
            memcpy(&da, &bits_a, sizeof da);

            const unsigned long n = (unsigned long)xs64_next(&rng);
            const uint64_t pa = precs[xs64_below(&rng, 5)];

            mpfr_t x;
            init_from_double(x, da, pa);
            emit_case(out, "fuzz", x, n);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — transcribed from mpfr/tests/tcmp_ui.c          */
    /* ============================================================== */
    {
        /* tcmp_ui.c L100–L104: mpfr_set_ui(x,17); mpfr_cmp_ui(x,17) == 0. */
        { mpfr_t x; init_from_ui(x, 17, 32); emit_case(out, "mined", x, 17); mpfr_clear(x); }

        /* tcmp_ui.c L146–L150: mpfr_set_ui(x,0); mpfr_cmp_ui(x,0) == 0. */
        { mpfr_t x; init_from_ui(x, 0, 32); emit_case(out, "mined", x, 0); mpfr_clear(x); }

        /* Implicit from tcmp_ui.c TCMP_UI_CHECK_NAN sweep at the NaN
         * boundary: with TCMP_UI_CHECK_NAN=0 the test exercises ±0
         * vs ui=0 (always 0). We use +0 here; -0 covered in edge. */
        { mpfr_t x; init_pos_zero(x, 32); emit_case(out, "mined", x, 0); mpfr_clear(x); }

        /* Generalised tcmp_ui macro test L67–L77 — for non-NaN x, cmp_ui
         * with a positive small n succeeds. */
        { mpfr_t x; init_from_ui(x, 1, 32); emit_case(out, "mined", x, 17); mpfr_clear(x); }

        /* Negative x < 0 < any ui: always -1. */
        { mpfr_t x; init_from_si(x, -1, 32); emit_case(out, "mined", x, 17); mpfr_clear(x); }
    }

    return 0;
}
