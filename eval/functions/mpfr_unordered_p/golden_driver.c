/*
 * golden_driver.c — Golden master for MPFR's mpfr_unordered_p.
 *
 * C signature
 * -----------
 *
 *   int mpfr_unordered_p(mpfr_srcptr x, mpfr_srcptr y);
 *
 *   See mpfr/src/comparisons.c L75–L79.
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"a":{<MPFR>},"b":{<MPFR>}},
 *    "output":<bool>,
 *    "time_ns":<n>}
 *
 *   `output` is a bare JSON boolean via jl_output_scalar_bool.
 *
 * Tag distribution
 * ----------------
 *
 *   happy        :  ~25  (typical non-NaN pairs — all return false)
 *   edge         :  ~50  (kind × kind, NaN cases, signed zero, ±Inf)
 *   adversarial  :  ~85  (NaN-heavy + non-NaN mix to defeat the
 *                          "AND instead of OR" broken port)
 *   fuzz         :   60  (PRNG, seed 0xC0FFEEC0FFEEC0FFULL; mix of
 *                          NaN-injected and non-NaN cases)
 *   mined        :    5  (transcribed from mpfr/tests/tcomparisons.c)
 *
 * The broken port returns `a.kind === 'nan' && b.kind === 'nan'` — only
 * true when BOTH are NaN. It disagrees with the correct port whenever
 * exactly ONE operand is NaN (correct: true, broken: false). The
 * adversarial+fuzz mix is calibrated so ~half the NaN-injected cases
 * have exactly one NaN, ensuring the broken port fails a large fraction
 * of corr-class cases.
 *
 * Ref: mpfr/src/comparisons.c — the C reference.
 * Ref: src/ops/unordered_p.ts — the production port.
 */
#include "common.h"

#include <inttypes.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_unordered_p golden_driver requires GMP_NUMB_BITS == 64"
#endif

static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr a, mpfr_srcptr b) {
    const uint64_t t0 = now_ns();
    const int raw = mpfr_unordered_p(a, b);
    const uint64_t elapsed = now_ns() - t0;

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "a", a);
    jl_kv_mpfr(out, 0, "b", b);
    jl_end_inputs(out);
    jl_output_scalar_bool(out, raw);
    jl_finish(out, elapsed);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
}

static inline void init_pos_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, 1); }
static inline void init_neg_inf(mpfr_ptr x, uint64_t prec)  { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_inf(x, -1); }
static inline void init_pos_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, 1); }
static inline void init_neg_zero(mpfr_ptr x, uint64_t prec) { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_zero(x, -1); }
static inline void init_nan(mpfr_ptr x, uint64_t prec)      { mpfr_init2(x, (mpfr_prec_t)prec); mpfr_set_nan(x); }

int main(void) {
    FILE *out = stdout;

    /* ============================================================== */
    /* happy: typical non-NaN pairs (all false)                        */
    /* ============================================================== */
    {
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_from_double(b, 2.0, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 2.0, 53); init_from_double(b, 1.0, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_from_double(b, 1.0, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -1.0, 53); init_from_double(b, 1.0, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 3.14, 53); init_from_double(b, 2.71, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1e100, 53); init_from_double(b, 1e-100, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -1e100, 53); init_from_double(b, -1e-100, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 0.5, 53); init_from_double(b, 0.25, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_from_double(b, 1.0, 128);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.5, 53); init_from_double(b, 1.5, 200);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 2.0, 53); init_from_double(b, 4.0, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -2.0, 53); init_from_double(b, -4.0, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 0.1, 53); init_from_double(b, 0.2, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 42.0, 53); init_from_double(b, 42.0, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 100.0, 53); init_from_double(b, 100.0, 200);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 6.022e23, 53); init_from_double(b, 6.022e23, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 2.71828, 53); init_from_double(b, 3.14159, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -3.14, 53); init_from_double(b, -3.14, 200);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1e9, 53); init_from_double(b, 1e10, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1e-9, 53); init_from_double(b, 1e-10, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 0.0, 53); init_from_double(b, 0.0, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 1); init_from_double(b, 1.0, 256);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 0.5, 2); init_from_double(b, 0.25, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1e-300, 53); init_from_double(b, 1e300, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -1.0, 53); init_from_double(b, -2.0, 53);
          emit_case(out, "happy", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    /* ============================================================== */
    /* edge: NaN cases (true), kind boundaries, signed zero            */
    /* ============================================================== */
    {
        /* NaN × non-NaN: true (10). */
        { mpfr_t a, b; init_nan(a, 53); init_from_double(b, 1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_nan(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_from_double(b, -1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -1.0, 53); init_nan(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_pos_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_inf(a, 53); init_nan(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_neg_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_pos_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_zero(a, 53); init_nan(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_neg_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* NaN × NaN: true (3). */
        { mpfr_t a, b; init_nan(a, 53); init_nan(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_nan(b, 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 200); init_nan(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Signed zero × non-NaN: false (6). */
        { mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_zero(a, 53); init_pos_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_zero(a, 53); init_pos_zero(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_zero(a, 53); init_from_double(b, 1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_zero(a, 53); init_from_double(b, -1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_zero(a, 53); init_pos_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* ±Inf × non-NaN: false (8). */
        { mpfr_t a, b; init_pos_inf(a, 53); init_neg_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_pos_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_neg_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_inf(a, 53); init_from_double(b, 1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_from_double(b, -1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_pos_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_inf(a, 53); init_pos_inf(b, 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Same value, different prec: false (6). */
        { mpfr_t a, b; init_from_double(a, 1.0, 2); init_from_double(b, 1.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.5, 2); init_from_double(b, 1.5, 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 0.5, 1); init_from_double(b, 0.5, 256);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -2.0, 1); init_from_double(b, -2.0, 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 3.14, 53); init_from_double(b, 3.14, 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_zero(a, 53); init_neg_zero(b, 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* prec=1 boundary (4). */
        { mpfr_t a, b; init_from_double(a, 1.0, 1); init_from_double(b, 0.5, 1);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 1); init_from_double(b, 1.0, 1);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 2.0, 1); init_from_double(b, 4.0, 1);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 1); init_from_double(b, 1.0, 1);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* NaN at various precs (4). */
        { mpfr_t a, b; init_nan(a, 1); init_nan(b, 1);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 1); init_from_double(b, 1e100, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 256); init_nan(b, 256);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 256); init_nan(b, 256);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Mixed kinds with one NaN (the load-bearing broken-port gate cases) (9). */
        { mpfr_t a, b; init_nan(a, 53); init_from_double(b, 0.0, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 0.0, 53); init_nan(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_from_double(b, 1e300, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1e-300, 53); init_nan(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_from_double(b, 3.14, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, -3.14, 53); init_nan(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_from_double(b, 1.0, 200);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_from_double(a, 1.0, 200); init_nan(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 200); init_neg_inf(b, 53);
          emit_case(out, "edge", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    /* ============================================================== */
    /* adversarial: NaN-asymmetry mass                                 */
    /*                                                                 */
    /* Broken port returns `a.nan && b.nan` (AND); correct returns OR. */
    /* Disagreement: exactly one operand is NaN.                       */
    /*                                                                 */
    /* We pile on the "exactly one NaN" cases to bury the broken port. */
    /* ============================================================== */
    {
        /* a is NaN, b is various non-NaN: correct=true, broken=false.
         * We pile on cases to ensure the AND-instead-of-OR mutation
         * fails enough to push composite below the 0.5 gate. */
        const struct { double v; uint64_t prec; } b_vs[] = {
            { 0.0, 53 }, { 1.0, 53 }, { -1.0, 53 }, { 1e10, 53 },
            { -1e10, 53 }, { 1e-10, 53 }, { -1e-10, 53 }, { 3.14, 53 },
            { 0.0, 1 }, { 1.0, 1 }, { 0.5, 1 },
            { 1.0, 100 }, { 1.0, 200 }, { 1.0, 256 },
            { 100.0, 64 }, { -100.0, 64 }, { 1e100, 128 },
            { -1e100, 128 }, { 1e-300, 200 }, { 1e300, 200 },
            /* Additional sweep — same value at many precs to widen the */
            /* exactly-one-NaN pile. */
            { 2.0, 53 }, { 2.0, 128 }, { 2.0, 256 },
            { 0.5, 53 }, { 0.5, 128 }, { 0.5, 256 },
            { 0.25, 53 }, { 0.25, 128 }, { 0.25, 256 },
            { 42.0, 53 }, { 42.0, 128 }, { 42.0, 256 },
            { -42.0, 53 }, { -42.0, 256 },
            { 65536.0, 53 }, { 65536.0, 256 },
            { 1024.0, 53 }, { 1024.0, 256 },
            { 0.0625, 53 }, { 0.0625, 256 },
            { 3.14159, 53 }, { 3.14159, 256 },
            { 2.71828, 53 }, { 2.71828, 256 },
            { 1e50, 53 }, { 1e50, 256 },
            { -1e50, 53 }, { -1e50, 256 },
            { 1e-50, 53 }, { 1e-50, 256 },
            { 1.5, 2 }, { 1.5, 53 }, { 1.5, 256 },
            { 7.0, 3 }, { 7.0, 53 }, { 7.0, 256 },
        };
        const size_t n_b = sizeof(b_vs)/sizeof(b_vs[0]);
        for (size_t i = 0; i < n_b; ++i) {
            { mpfr_t a, b; init_nan(a, 53); init_from_double(b, b_vs[i].v, b_vs[i].prec);
              emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
            { mpfr_t a, b; init_from_double(a, b_vs[i].v, b_vs[i].prec); init_nan(b, 53);
              emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        }

        /* a is NaN, b is ±Inf / ±0: correct=true, broken=false. */
        { mpfr_t a, b; init_nan(a, 53); init_pos_inf(b, 53);
          emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_neg_inf(b, 53);
          emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_pos_zero(b, 53);
          emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_neg_zero(b, 53);
          emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_inf(a, 53); init_nan(b, 53);
          emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_inf(a, 53); init_nan(b, 53);
          emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_pos_zero(a, 53); init_nan(b, 53);
          emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_neg_zero(a, 53); init_nan(b, 53);
          emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* NaN × NaN at various precs (both broken and correct true). */
        { mpfr_t a, b; init_nan(a, 1); init_nan(b, 53);
          emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 256); init_nan(b, 256);
          emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 53); init_nan(b, 256);
          emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }
        { mpfr_t a, b; init_nan(a, 256); init_nan(b, 53);
          emit_case(out, "adversarial", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* Non-NaN × non-NaN at various combinations — all false; lets us
         * verify the correct port's negative answers across many kinds. */
        const struct { int ak; int bk; double av; double bv; uint64_t prec; } nonnan[] = {
            /* ak/bk: 0=normal-via-double, 1=+inf, 2=-inf, 3=+0, 4=-0 */
            { 0, 0, 1.0, 1.0, 53 }, { 0, 0, 1.0, 2.0, 53 }, { 0, 0, 2.0, 1.0, 53 },
            { 0, 0, -1.0, 1.0, 53 }, { 0, 0, 1e100, 1e-100, 53 },
            { 0, 1, 1.0, 0, 53 }, { 0, 2, 1.0, 0, 53 }, { 1, 1, 0, 0, 53 },
            { 1, 2, 0, 0, 53 }, { 3, 4, 0, 0, 53 }, { 0, 3, 1.0, 0, 53 },
            { 0, 4, 1.0, 0, 53 }, { 3, 0, 0, 1.0, 53 }, { 4, 0, 0, -1.0, 53 },
            { 0, 0, 0.5, 0.25, 53 }, { 0, 0, 100.0, 100.0, 128 },
        };
        const size_t n_nn = sizeof(nonnan)/sizeof(nonnan[0]);
        for (size_t i = 0; i < n_nn; ++i) {
            mpfr_t a, b;
            switch (nonnan[i].ak) {
                case 0: init_from_double(a, nonnan[i].av, nonnan[i].prec); break;
                case 1: init_pos_inf(a, nonnan[i].prec); break;
                case 2: init_neg_inf(a, nonnan[i].prec); break;
                case 3: init_pos_zero(a, nonnan[i].prec); break;
                default: init_neg_zero(a, nonnan[i].prec); break;
            }
            switch (nonnan[i].bk) {
                case 0: init_from_double(b, nonnan[i].bv, nonnan[i].prec); break;
                case 1: init_pos_inf(b, nonnan[i].prec); break;
                case 2: init_neg_inf(b, nonnan[i].prec); break;
                case 3: init_pos_zero(b, nonnan[i].prec); break;
                default: init_neg_zero(b, nonnan[i].prec); break;
            }
            emit_case(out, "adversarial", a, b);
            mpfr_clear(a); mpfr_clear(b);
        }
    }

    /* ============================================================== */
    /* fuzz: 60 cases — PRNG-driven                                   */
    /*                                                                 */
    /* Mix: ~33% inject NaN into a, ~33% inject NaN into b, ~33% both */
    /* finite. This ensures the broken "AND" port fails the ~66% with */
    /* exactly one NaN, and the correct port reports true on them.    */
    /* ============================================================== */
    {
        xs64_t rng;
        xs64_seed(&rng, 0xC0FFEEC0FFEEC0FFULL);
        const uint64_t precs[5] = { 53, 64, 100, 128, 200 };

        int emitted = 0;
        while (emitted < 60) {
            const uint64_t bits_a = xs64_next(&rng);
            const uint64_t bits_b = xs64_next(&rng);

            const uint64_t exp_a = (bits_a >> 52) & 0x7FF;
            const uint64_t exp_b = (bits_b >> 52) & 0x7FF;
            /* We let NaN bit-patterns through randomly; they ARE relevant
             * for unordered_p. Skip ±Inf bit patterns only to keep them
             * in the edge bucket. Actually no — both NaN and Inf are 0x7FF
             * differ only by mantissa. We INCLUDE them all here. */

            double da, db;
            memcpy(&da, &bits_a, sizeof da);
            memcpy(&db, &bits_b, sizeof db);

            const uint64_t pa = precs[xs64_below(&rng, 5)];
            const uint64_t pb = precs[xs64_below(&rng, 5)];

            /* Force NaN injection on roughly 2/3 of cases by mode. */
            const int mode = (int)xs64_below(&rng, 3);

            mpfr_t a, b;
            if (mode == 0) {
                init_nan(a, pa);
                init_from_double(b, db, pb);
                /* If db happens to be NaN, that's fine — both are NaN, still true. */
                (void)exp_a;
            } else if (mode == 1) {
                init_from_double(a, da, pa);
                init_nan(b, pb);
                (void)exp_b;
            } else {
                /* Both finite (skip NaN/Inf bit patterns to keep the
                 * branch easy to reason about). */
                if (exp_a == 0x7FF || exp_b == 0x7FF) continue;
                init_from_double(a, da, pa);
                init_from_double(b, db, pb);
            }
            emit_case(out, "fuzz", a, b);
            mpfr_clear(a); mpfr_clear(b);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — transcribed from mpfr/tests/tcomparisons.c     */
    /* ============================================================== */
    {
        /* tcomparisons.c L52–L70: NaN-forced operands → unordered_p
         * is the ONLY predicate that should be true (cmpbool == 0x40). */
        { mpfr_t a, b; init_nan(a, 53); init_nan(b, 53);
          emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }

        { mpfr_t a, b; init_nan(a, 53); init_from_double(b, 1.0, 53);
          emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }

        { mpfr_t a, b; init_from_double(a, 1.0, 53); init_nan(b, 53);
          emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* tcomparisons.c L104–L108 eq_tests: same value, different prec
         * → unordered_p false. */
        { mpfr_t a, b; init_from_double(a, 12345.0, 53); init_from_double(b, 12345.0, 117);
          emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }

        /* tcomparisons.c L26–L84 cmp_tests for i > 2: non-NaN pairs →
         * unordered_p false. */
        { mpfr_t a, b; init_from_double(a, -1.0, 53); init_from_double(b, 1.0, 53);
          emit_case(out, "mined", a, b); mpfr_clear(a); mpfr_clear(b); }
    }

    return 0;
}
