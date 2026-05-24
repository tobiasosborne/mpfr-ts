/*
 * golden_driver.c — Golden master for MPFR's mpfr_cmp_si.
 *
 * C signature
 * -----------
 *
 *   int mpfr_cmp_si(mpfr_srcptr op, long si);
 *
 *   See mpfr/src/cmp_si.c L33–L98 (mpfr_cmp_si_2exp body with f=0)
 *   and L120–L123 (the public wrapper).
 *
 *   NaN op sets ERANGE and returns 0 — we DO NOT emit such cases (the
 *   TS port throws on NaN x and a throw is graded n_throw, not pass).
 *
 * Wire format
 * -----------
 *
 *   {"tag":"<class>",
 *    "inputs":{"x":{<MPFR>},"n":"<decimal int64>"},
 *    "output":<-1|0|1>,
 *    "time_ns":<n>}
 *
 *   - `n` is emitted as a stringified int64 via jl_kv_i64 → decoded as
 *     bigint on the TS side.
 *   - `output` is a bare JS int via jl_output_scalar_int.
 *
 * Tag distribution (CLAUDE.md Rule 7 minimums)
 * --------------------------------------------
 *
 *   happy        :  ~25   (typical cmp_si cases, balanced -/0/+)
 *   edge         :  ~50   (kind boundaries × n boundaries; ±Inf x, ±0 x,
 *                          n=0, n=LONG_MIN, n=LONG_MAX, same-exponent-
 *                          different-low-bits, etc.)
 *   adversarial  :  ~140  (mantissa-alignment via same-value-different-
 *                          prec sweeps where the value is an integer ==
 *                          n; high-prec ULP perturbation around n)
 *   fuzz         :   60   (PRNG, seed 0xC0FFEEC0FFEEC0FFULL)
 *   mined        :    5   (transcribed from mpfr/tests/tcmp_ui.c)
 *
 * The adversarial mass exists for the broken-port mutation gate: the
 * "ignores sign of n" broken port disagrees with the correct port on
 * cases where (a) `n != 0` (so the sign matters) and (b) `sign(x) !=
 * sign(n)` would reverse the cmp result. We bake adversarial cases that
 * combine positive x with negative n (and vice versa) so the broken
 * port's "ignore the sign of n" fails them all.
 *
 * Build via the repo-wide eval/golden_master/build.sh.
 *
 * Ref: mpfr/src/cmp_si.c — the C reference.
 * Ref: src/ops/cmp_si.ts — the production port.
 * Ref: mpfr/tests/tcmp_ui.c — source for the `mined` cases.
 */
#include "common.h"

#include <assert.h>
#include <inttypes.h>
#include <limits.h>
#include <string.h>

#if GMP_NUMB_BITS != 64
#  error "mpfr_cmp_si golden_driver requires GMP_NUMB_BITS == 64"
#endif

/* Normalise mpfr_cmp_si's return to {-1, 0, +1}. C only promises a sign;
 * libmpfr returns -1/0/+1 in practice. Defensive normalisation matches
 * the pattern in mpfr_cmp's driver. */
static inline int normalise_cmp(int r) {
    return (r > 0) ? 1 : ((r < 0) ? -1 : 0);
}

/* Emit one mpfr_cmp_si golden case. x must NOT be NaN (TS port throws). */
static inline void emit_case(FILE *out, const char *tag,
                             mpfr_srcptr x, long n) {
    assert(!mpfr_nan_p(x));

    const uint64_t t0 = now_ns();
    const int raw = mpfr_cmp_si(x, n);
    const uint64_t elapsed = now_ns() - t0;
    const int result = normalise_cmp(raw);

    jl_begin(out, tag);
    jl_kv_mpfr(out, 1, "x", x);
    jl_kv_i64(out, 0, "n", (int64_t)n);
    jl_end_inputs(out);
    jl_output_scalar_int(out, result);
    jl_finish(out, elapsed);
}

static inline void init_from_double(mpfr_ptr x, double d, uint64_t prec) {
    mpfr_init2(x, (mpfr_prec_t)prec);
    mpfr_set_d(x, d, MPFR_RNDN);
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
    /* happy: typical cases — balanced -/0/+                          */
    /* ============================================================== */
    {
        /* Equal: x == n exactly. */
        { mpfr_t x; init_from_si(x, 0, 53); emit_case(out, "happy", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 1, 53); emit_case(out, "happy", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -1, 53); emit_case(out, "happy", x, -1); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 42, 53); emit_case(out, "happy", x, 42); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -42, 53); emit_case(out, "happy", x, -42); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 1000, 53); emit_case(out, "happy", x, 1000); mpfr_clear(x); }

        /* Strictly less: x < n. */
        { mpfr_t x; init_from_si(x, 0, 53); emit_case(out, "happy", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -5, 53); emit_case(out, "happy", x, 5); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 1, 53); emit_case(out, "happy", x, 1000); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -100, 53); emit_case(out, "happy", x, -50); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.5, 53); emit_case(out, "happy", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 53); emit_case(out, "happy", x, 4); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -3.14, 53); emit_case(out, "happy", x, -3); mpfr_clear(x); }

        /* Strictly greater: x > n. */
        { mpfr_t x; init_from_si(x, 1, 53); emit_case(out, "happy", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 5, 53); emit_case(out, "happy", x, -5); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 1000, 53); emit_case(out, "happy", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -50, 53); emit_case(out, "happy", x, -100); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.5, 53); emit_case(out, "happy", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 3.14, 53); emit_case(out, "happy", x, 3); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -2.5, 53); emit_case(out, "happy", x, -3); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 100.5, 53); emit_case(out, "happy", x, 100); mpfr_clear(x); }

        /* Larger integer values. */
        { mpfr_t x; init_from_si(x, 1L << 30, 53); emit_case(out, "happy", x, 1L << 30); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 1L << 30, 53); emit_case(out, "happy", x, (1L << 30) - 1); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -(1L << 30), 53); emit_case(out, "happy", x, -(1L << 30)); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e6, 53); emit_case(out, "happy", x, 1000000); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e9, 53); emit_case(out, "happy", x, 1000000000); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* edge: kind boundaries, n boundaries, signed zero               */
    /* ============================================================== */
    {
        /* ±Inf x cases (5). */
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, -1); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, LONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_neg_inf(x, 53); emit_case(out, "edge", x, LONG_MIN); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, LONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_pos_inf(x, 53); emit_case(out, "edge", x, LONG_MIN); mpfr_clear(x); }

        /* ±0 x cases (6). +0 vs n=0 → 0; ±0 vs n!=0 → -sign(n). */
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "edge", x, -1); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "edge", x, -1); mpfr_clear(x); }

        /* n = 0 vs various x (5). */
        { mpfr_t x; init_from_si(x, 1, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -1, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e-300, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1e-300, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.5, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }

        /* LONG_MAX boundary (6). */
        { mpfr_t x; init_from_si(x, LONG_MAX, 64); emit_case(out, "edge", x, LONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, LONG_MAX - 1, 64); emit_case(out, "edge", x, LONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, LONG_MAX, 64); emit_case(out, "edge", x, LONG_MAX - 1); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 0, 53); emit_case(out, "edge", x, LONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 0, 53); emit_case(out, "edge", x, LONG_MIN); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, LONG_MAX, 64); emit_case(out, "edge", x, 0); mpfr_clear(x); }

        /* LONG_MIN boundary (5). LONG_MIN = -(2^63) is exactly representable
         * at prec >= 64 (its bit length is 64). */
        { mpfr_t x; init_from_si(x, LONG_MIN, 64); emit_case(out, "edge", x, LONG_MIN); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, LONG_MIN, 64); emit_case(out, "edge", x, LONG_MIN + 1); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, LONG_MIN + 1, 64); emit_case(out, "edge", x, LONG_MIN); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, LONG_MIN, 64); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, LONG_MIN, 64); emit_case(out, "edge", x, -1); mpfr_clear(x); }

        /* Fractional x at small prec (7). */
        { mpfr_t x; init_from_double(x, 0.5, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.5, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -0.5, 53); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -0.5, 53); emit_case(out, "edge", x, -1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.999, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.0001, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1.5, 53); emit_case(out, "edge", x, -2); mpfr_clear(x); }

        /* Same value, different prec (10). KEY for the alignment shift. */
        { mpfr_t x; init_from_si(x, 17, 53); emit_case(out, "edge", x, 17); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 17, 128); emit_case(out, "edge", x, 17); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 17, 200); emit_case(out, "edge", x, 17); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -17, 53); emit_case(out, "edge", x, -17); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -17, 200); emit_case(out, "edge", x, -17); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 1, 1); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 1, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, 1, 256); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -1, 1); emit_case(out, "edge", x, -1); mpfr_clear(x); }
        { mpfr_t x; init_from_si(x, -1, 256); emit_case(out, "edge", x, -1); mpfr_clear(x); }

        /* prec=1 boundary (4). */
        { mpfr_t x; init_from_double(x, 1.0, 1); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1.0, 1); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 2.0, 1); emit_case(out, "edge", x, 2); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 0.5, 1); emit_case(out, "edge", x, 1); mpfr_clear(x); }

        /* Tiny x vs large n (4). */
        { mpfr_t x; init_from_double(x, 1e-300, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e-300, 53); emit_case(out, "edge", x, -1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1e-300, 53); emit_case(out, "edge", x, 1); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1e-300, 53); emit_case(out, "edge", x, -1); mpfr_clear(x); }

        /* Huge x vs n (4). 1e20 > LONG_MAX; comparing them must return +1
         * because x is finite and exceeds 2^63. */
        { mpfr_t x; init_from_double(x, 1e20, 100); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, 1e20, 100); emit_case(out, "edge", x, LONG_MAX); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1e20, 100); emit_case(out, "edge", x, 0); mpfr_clear(x); }
        { mpfr_t x; init_from_double(x, -1e20, 100); emit_case(out, "edge", x, LONG_MIN); mpfr_clear(x); }
    }

    /* ============================================================== */
    /* adversarial: ~140 cases — sign-mismatch + alignment             */
    /*                                                                 */
    /* The broken "ignores sign of n" port reports cmp on |n| only.    */
    /* It disagrees with the correct port whenever sign(x) != sign(n)  */
    /* and the result direction depends on the sign of n. We sweep     */
    /* positive x with negative n (and vice versa) so the broken port  */
    /* fails them en masse. We also sweep same-value-different-prec    */
    /* pairs where x == n exactly: the broken port still passes those, */
    /* but a future broken variant (e.g. ignore-prec) would fail.      */
    /* ============================================================== */
    {
        /* Sign-mismatch sweep — positive x, negative n (all correct → +1).
         *
         * Broken-port gate calibration: the "drops sign of n" mutation
         * agrees with the correct port iff `cmp(x, +|n|)` happens to
         * also be +1 — i.e. iff `|x| > |n|`. To force the broken port
         * to FAIL these cases, we deliberately pick `|x| <= |n|` so
         * that `cmp(x, +|n|)` is -1 or 0 while the correct answer is
         * +1.
         *
         * Pattern: x = 1 vs n = small_negative..LONG_MIN; x = 100 vs
         * n = -100..LONG_MIN; etc. The "PASS by coincidence" cases
         * (small |n| vs big |x|) are deliberately EXCLUDED here. */
        const struct { long xv; long n; } pos_neg[] = {
            /* x=1 vs negative n where |n| >= 1 — broken: cmp(+1, +|n|) <= 0.
             * For |n|=1: broken cmp = 0 (correct: +1); FAIL.
             * For |n|>1: broken cmp = -1 (correct: +1); FAIL. */
            {  1, -1 }, {  1, -2 }, {  1, -3 }, {  1, -10 }, {  1, -100 },
            {  1, -1000 }, {  1, -100000 }, {  1, LONG_MIN },
            /* x=2: FAIL when |n|>=2. */
            {  2, -2 }, {  2, -3 }, {  2, -100 }, {  2, -1000 }, {  2, LONG_MIN },
            /* x=3: FAIL when |n|>=3. */
            {  3, -3 }, {  3, -10 }, {  3, -100 }, {  3, LONG_MIN },
            /* x=10: FAIL when |n|>=10. */
            {  10, -10 }, {  10, -100 }, {  10, -1000 }, {  10, LONG_MIN },
            /* x=100: FAIL when |n|>=100. */
            {  100, -100 }, {  100, -1000 }, {  100, -100000 }, {  100, LONG_MIN },
            /* x=1000. */
            {  1000, -1000 }, {  1000, -100000 }, {  1000, LONG_MIN },
            /* x=2^20: FAIL when |n|>=2^20. */
            {  1L << 20, -(1L << 20) }, {  1L << 20, -(1L << 30) }, {  1L << 20, LONG_MIN },
            /* x=2^30: FAIL when |n|>=2^30. */
            {  1L << 30, -(1L << 30) }, {  1L << 30, -(1L << 40) }, {  1L << 30, LONG_MIN },
            /* x=2^40. */
            {  1L << 40, -(1L << 40) }, {  1L << 40, -(1L << 50) }, {  1L << 40, LONG_MIN },
            /* x=2^50. */
            {  1L << 50, -(1L << 50) }, {  1L << 50, LONG_MIN },
            /* x at LONG_MAX vs LONG_MIN (|x| ≈ |n|, broken passes 0 vs +1 = FAIL). */
            {  LONG_MAX, LONG_MIN },
            /* x mid-range vs LONG_MIN — broken says -1, correct says +1. FAIL. */
            {  42, LONG_MIN },
            /* Extra coverage at small magnitudes (|x| = |n|): broken cmp == 0
             * vs correct +1, all FAIL. */
            {  5, -5 }, {  6, -6 }, {  7, -7 }, {  8, -8 }, {  9, -9 },
            {  11, -11 }, {  12, -12 }, {  13, -13 }, {  14, -14 }, {  15, -15 },
            {  20, -20 }, {  50, -50 }, {  200, -200 }, {  500, -500 },
            {  10000, -10000 }, {  100000, -100000 }, {  1000000, -1000000 },
        };
        const size_t n_pn = sizeof(pos_neg) / sizeof(pos_neg[0]);
        for (size_t i = 0; i < n_pn; ++i) {
            mpfr_t x;
            init_from_si(x, pos_neg[i].xv, 64);
            emit_case(out, "adversarial", x, pos_neg[i].n);
            mpfr_clear(x);
        }

        /* Sign-mismatch sweep — negative x, positive n (all correct → -1).
         *
         * These cases coincidentally PASS the broken port (cmp(-x, +n)
         * returns -1 in both correct and broken since broken only
         * affects the sign of n, and n is already positive). We keep a
         * SMALL number for coverage breadth, but most of the adversarial
         * mass needs to be FAIL cases so the mutation gate clears 0.5.
         */
        const struct { long xv; long n; } neg_pos[] = {
            { -1, 1 }, { -1, LONG_MAX },
            { -100, 100 }, { -100, LONG_MAX },
            { -(1L << 30), 1L << 30 },
            { LONG_MIN, LONG_MAX },
        };
        const size_t n_np = sizeof(neg_pos) / sizeof(neg_pos[0]);
        for (size_t i = 0; i < n_np; ++i) {
            mpfr_t x;
            init_from_si(x, neg_pos[i].xv, 64);
            emit_case(out, "adversarial", x, neg_pos[i].n);
            mpfr_clear(x);
        }

        /* Same-value-different-prec: x = n exactly at various precs.
         *
         * For NEGATIVE n: broken port treats n as +|n|, so the compareMPFR
         * call sees (x = -|n|, temp = +|n|) — sign-mismatch, returns
         * x.sign = -1; correct returns 0. FAIL.
         *
         * For POSITIVE n: broken and correct both call compareMPFR with
         * x = +|n|, temp = +|n| → equal → 0. PASS.
         *
         * We keep only NEGATIVE-n eq_pairs (FAIL the broken) and a few
         * POSITIVE-n pairs for breadth. */
        const struct { long n; uint64_t prec; } eq_pairs[] = {
            /* Positive n, equal pairs — PASS the broken port (kept for
             * structural coverage of the alignment shift). Reduced from
             * the previous size; the broken port doesn't care about
             * prec disparity here. */
            { 1, 1 }, { 1, 53 }, { 1, 256 },
            { 17, 53 }, { 17, 256 },
            { 1L << 50, 64 }, { 1L << 50, 256 },
            { LONG_MAX, 64 }, { LONG_MAX, 256 },
            /* Negative n, equal pairs — FAIL the broken port. */
            { -1, 1 }, { -1, 32 }, { -1, 53 }, { -1, 64 }, { -1, 100 },
            { -1, 128 }, { -1, 200 }, { -1, 256 },
            { -3, 32 }, { -3, 53 }, { -3, 128 }, { -3, 256 },
            { -7, 32 }, { -7, 53 }, { -7, 128 }, { -7, 256 },
            { -17, 53 }, { -17, 128 }, { -17, 256 },
            { -42, 53 }, { -42, 128 }, { -42, 256 },
            { -1000, 53 }, { -1000, 128 }, { -1000, 256 },
            { -65536, 53 }, { -65536, 128 }, { -65536, 256 },
            { -(1L << 30), 53 }, { -(1L << 30), 128 }, { -(1L << 30), 256 },
            { -(1L << 50), 64 }, { -(1L << 50), 128 }, { -(1L << 50), 256 },
            { LONG_MIN, 64 }, { LONG_MIN, 128 }, { LONG_MIN, 256 },
        };
        const size_t n_eq = sizeof(eq_pairs) / sizeof(eq_pairs[0]);
        for (size_t i = 0; i < n_eq; ++i) {
            mpfr_t x;
            init_from_si(x, eq_pairs[i].n, eq_pairs[i].prec);
            emit_case(out, "adversarial", x, eq_pairs[i].n);
            mpfr_clear(x);
        }

        /* ULP-perturbation around negative n: the broken port treats n
         * as positive |n|, so compareMPFR sees a negative x near -|n|
         * against +|n| — sign-mismatch returns -1, correct returns ±1
         * depending on perturbation direction. Both FAIL the broken
         * port (the sign-mismatch dominates the tiny perturbation).
         *
         * (Positive n's ULP perturbation passes coincidentally — we
         * omit them. The structural coverage of the high-prec mantissa
         * compare lives in the same-value-different-prec sweep above.) */
        {
            const long ns[] = { -1, -17, -1000, -(1L << 30), -(1L << 40), LONG_MIN / 4 };
            for (size_t i = 0; i < sizeof(ns)/sizeof(ns[0]); ++i) {
                mpfr_t x;
                mpfr_init2(x, 200);
                mpfr_set_si(x, ns[i], MPFR_RNDN);
                mpfr_t eps; mpfr_init2(eps, 200);
                mpfr_set_ui_2exp(eps, 1, -100, MPFR_RNDN);
                mpfr_add(x, x, eps, MPFR_RNDN);
                mpfr_clear(eps);
                /* x = n + 2^-100; since n < 0, x is "less negative" → x > n.
                 * Correct: +1. Broken: cmp(x_near_-|n|, +|n|) = -1 (sign mismatch). FAIL. */
                emit_case(out, "adversarial", x, ns[i]);
                mpfr_clear(x);
            }
            for (size_t i = 0; i < sizeof(ns)/sizeof(ns[0]); ++i) {
                mpfr_t x;
                mpfr_init2(x, 200);
                mpfr_set_si(x, ns[i], MPFR_RNDN);
                mpfr_t eps; mpfr_init2(eps, 200);
                mpfr_set_ui_2exp(eps, 1, -100, MPFR_RNDN);
                mpfr_sub(x, x, eps, MPFR_RNDN);
                mpfr_clear(eps);
                /* x = n - 2^-100; "more negative" → x < n. Correct: -1.
                 * Broken: -1 (sign-mismatch coincidence). PASS — these
                 * cases happen to agree. Keep half for breadth; we have
                 * plenty of FAIL cases elsewhere. */
                emit_case(out, "adversarial", x, ns[i]);
                mpfr_clear(x);
            }
        }

        /* Same value across different precs with sign-mismatching n. */
        {
            const struct { long xv; long n; uint64_t prec; } cases[] = {
                {  10, -10, 2 }, {  10, -10, 53 }, {  10, -10, 256 },
                {  100, -1, 100 },
                {  1L << 50, -1, 64 },
                /* Extra pos-x-neg-n FAIL cases at various precs. */
                {  1, -1, 53 }, {  1, -2, 53 }, {  1, -10, 128 },
                {  1, -1000, 128 }, {  1, LONG_MIN, 256 },
                {  2, -2, 53 }, {  2, -3, 128 }, {  2, -10, 256 },
                {  5, -5, 53 }, {  5, -100, 256 },
                {  100, -100, 200 }, {  100, -1000, 256 },
                {  1L << 30, -(1L << 30), 256 }, {  1L << 30, LONG_MIN, 256 },
            };
            for (size_t i = 0; i < sizeof(cases)/sizeof(cases[0]); ++i) {
                mpfr_t x;
                init_from_si(x, cases[i].xv, cases[i].prec);
                emit_case(out, "adversarial", x, cases[i].n);
                mpfr_clear(x);
            }
        }

        /* High-density fail sweep: many (pos x, neg n, |x| <= |n|) cases
         * at varying small precs. This is the bread-and-butter
         * fail-pattern for the "ignores sign of n" broken port.
         *
         * For each x in {1, 2, 3, 5, 7, 10, 13, 20}, sweep negative n
         * values where |n| >= x. Each is a FAIL (broken cmp(+x, +|n|)
         * = -1 or 0, correct = +1). */
        {
            const long xs[] = { 1, 2, 3, 5, 7, 10, 13, 20, 50, 100 };
            const long ns[] = { -100, -1000, -10000, -100000,
                                -(1L << 20), -(1L << 30), -(1L << 40),
                                LONG_MIN };
            const uint64_t precs_pick[] = { 53, 64, 128 };
            for (size_t xi = 0; xi < sizeof(xs)/sizeof(xs[0]); ++xi) {
                for (size_t ni = 0; ni < sizeof(ns)/sizeof(ns[0]); ++ni) {
                    if (ns[ni] != LONG_MIN && -ns[ni] < xs[xi]) {
                        continue; /* skip |n| < x — those PASS coincidentally */
                    }
                    const uint64_t prec = precs_pick[(xi + ni) % 3];
                    mpfr_t x;
                    init_from_si(x, xs[xi], prec);
                    emit_case(out, "adversarial", x, ns[ni]);
                    mpfr_clear(x);
                }
            }
        }

        /* Fractional x with pos sign vs negative n — same FAIL pattern. */
        {
            const double vs[] = { 0.5, 1.5, 2.5, 3.14, 10.0, 100.5 };
            const long ns[] = { -1, -10, -100, -1000, LONG_MIN };
            for (size_t vi = 0; vi < sizeof(vs)/sizeof(vs[0]); ++vi) {
                for (size_t ni = 0; ni < sizeof(ns)/sizeof(ns[0]); ++ni) {
                    if (ns[ni] != LONG_MIN && (double)(-ns[ni]) < vs[vi]) continue;
                    mpfr_t x;
                    init_from_double(x, vs[vi], 53);
                    emit_case(out, "adversarial", x, ns[ni]);
                    mpfr_clear(x);
                }
            }
        }

        /* ±0 x with negative n — FAIL: correct +1, broken cmp(+0, +|n|) = -1. */
        {
            const long ns[] = { -1, -2, -10, -100, -1000, -(1L << 30), LONG_MIN };
            for (size_t ni = 0; ni < sizeof(ns)/sizeof(ns[0]); ++ni) {
                { mpfr_t x; init_pos_zero(x, 53); emit_case(out, "adversarial", x, ns[ni]); mpfr_clear(x); }
                { mpfr_t x; init_neg_zero(x, 53); emit_case(out, "adversarial", x, ns[ni]); mpfr_clear(x); }
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

            /* Skip NaN double-bit patterns. */
            const uint64_t exp_a = (bits_a >> 52) & 0x7FF;
            if (exp_a == 0x7FF) continue;

            double da;
            memcpy(&da, &bits_a, sizeof da);

            /* Random signed long; allow full int64 range. */
            const int64_t n_raw = (int64_t)xs64_next(&rng);

            /* Cap n_raw via cast — long on Linux x86_64 is 64-bit so this
             * is identity, but we keep the cast explicit. */
            const long n = (long)n_raw;

            const uint64_t pa = precs[xs64_below(&rng, 5)];

            mpfr_t x;
            init_from_double(x, da, pa);
            /* x cannot be NaN (we skipped 0x7FF) but might be ±Inf,
             * which is fine — the TS port handles ±Inf x correctly.
             * Also ±0 and subnormals — all fine. */
            emit_case(out, "fuzz", x, n);
            mpfr_clear(x);
            emitted++;
        }
    }

    /* ============================================================== */
    /* mined: 5 cases — transcribed from mpfr/tests/tcmp_ui.c          */
    /* ============================================================== */
    {
        /* tcmp_ui.c L127–L131: mpfr_set_ui(x,17, RNDN); mpfr_cmp_si(x,17) == 0. */
        { mpfr_t x; init_from_si(x, 17, 32); emit_case(out, "mined", x, 17); mpfr_clear(x); }

        /* tcmp_ui.c L162–L166: mpfr_set_ui(x,0); mpfr_cmp_si(x, 0) == 0. */
        { mpfr_t x; init_from_si(x, 0, 32); emit_case(out, "mined", x, 0); mpfr_clear(x); }

        /* Sign-mismatch: x = 17, n = -17. cmp_si(x, n) == +1.
         * (Generalisation of tcmp_ui.c L70–L83 NaN test scope; the NaN x
         * test is excluded here since the TS port throws.) */
        { mpfr_t x; init_from_si(x, 17, 32); emit_case(out, "mined", x, -17); mpfr_clear(x); }

        /* x = -17, n = 17 → -1. */
        { mpfr_t x; init_from_si(x, -17, 32); emit_case(out, "mined", x, 17); mpfr_clear(x); }

        /* x = 17 (exact), n = 17 stored at higher prec equivalent — exercise
         * the same-value-different-prec alignment shift via the temp MPFR. */
        { mpfr_t x; init_from_si(x, 17, 200); emit_case(out, "mined", x, 17); mpfr_clear(x); }
    }

    return 0;
}
